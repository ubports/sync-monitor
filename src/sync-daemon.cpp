/*
 * Copyright 2014 Canonical Ltd.
 *
 * This file is part of sync-monitor.
 *
 * sync-monitor is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * contact-service-app is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "sync-daemon.h"
#include "sync-account.h"
#include "sync-queue.h"
#include "eds-helper.h"
#include "notify-message.h"
#include "provider-template.h"

#include <QtCore/QDebug>
#include <QtCore/QTimer>

using namespace Accounts;

#define DAEMON_SYNC_TIMEOUT     1000 * 6 // one minute

SyncDaemon::SyncDaemon()
    : QObject(0),
      m_manager(0),
      m_eds(0),
      m_syncing(false),
      m_aboutToQuit(false)
{
    m_provider = new ProviderTemplate();
    m_provider->load();

    m_syncQueue = new SyncQueue();

    m_timeout = new QTimer(this);
    m_timeout->setInterval(DAEMON_SYNC_TIMEOUT);
    m_timeout->setSingleShot(true);
    connect(m_timeout, SIGNAL(timeout()), SLOT(continueSync()));
}

SyncDaemon::~SyncDaemon()
{
    quit();
}

void SyncDaemon::setupAccounts()
{
    if (m_manager) {
        return;
    }
    qDebug() << "Loading accounts...";

    m_manager = new Manager(this);
    Q_FOREACH(const AccountId &accountId, m_manager->accountList()) {
        addAccount(accountId, false);
    }
    connect(m_manager,
            SIGNAL(accountCreated(Accounts::AccountId)),
            SLOT(addAccount(Accounts::AccountId)));
    connect(m_manager,
            SIGNAL(accountRemoved(Accounts::AccountId)),
            SLOT(removeAccount(Accounts::AccountId)));
}

void SyncDaemon::setupTriggers()
{
    m_eds = new EdsHelper(this);
    connect(m_eds, &EdsHelper::dataChanged,
            this, &SyncDaemon::onDataChanged);
}

void SyncDaemon::onDataChanged(const QString &serviceName, const QString &sourceName)
{
    // TODO: filter by sourceName
    syncAll(serviceName);
}

void SyncDaemon::syncAll(const QString &serviceName)
{
    Q_FOREACH(SyncAccount *acc, m_accounts.values()) {
        if (serviceName.isEmpty()) {
            sync(acc);
        } else if (acc->availableServices().contains(serviceName)) {
            sync(acc, serviceName);
        }
    }
}

void SyncDaemon::sync()
{
    // wait some time for new sync requests
    m_timeout->start();
}

void SyncDaemon::continueSync()
{
    // sync the next service on the queue
    if (!m_aboutToQuit && !m_syncQueue->isEmpty()) {
        m_syncing = true;
        m_currentServiceName = m_syncQueue->popNext(&m_currentAccount);
        m_currentAccount->sync(m_currentServiceName);
    } else {
        m_currentAccount = 0;
        m_currentServiceName.clear();
        m_syncing = false;
        qDebug() << "All accounts synced";
    }
}

void SyncDaemon::run()
{
    setupAccounts();
    setupTriggers();
    syncAll();
}

void SyncDaemon::addAccount(const AccountId &accountId, bool startSync)
{
    Account *acc = m_manager->account(accountId);
    qDebug() << "Found account:" << acc->displayName();
    if (!acc) {
        qWarning() << "Fail to retrieve accounts:" << m_manager->lastError().message();
    } else if (m_provider->contains(acc->providerName())) {
        SyncAccount *syncAcc = new SyncAccount(acc,
                                               m_provider->settings(acc->providerName()),
                                               this);
        m_accounts.insert(accountId, syncAcc);
        connect(syncAcc, SIGNAL(syncStarted(QString, QString)),
                         SLOT(onAccountSyncStarted(QString, QString)));
        connect(syncAcc, SIGNAL(syncFinished(QString, QString)),
                         SLOT(onAccountSyncFinished(QString, QString)));
        connect(syncAcc, SIGNAL(syncError(QString, int)),
                         SLOT(onAccountSyncError(QString, int)));
        connect(syncAcc, SIGNAL(enableChanged(QString, bool)),
                         SLOT(onAccountEnableChanged(QString, bool)));
        connect(syncAcc, SIGNAL(configured(QString)),
                         SLOT(onAccountConfigured(QString)), Qt::DirectConnection);
        if (startSync) {
            sync(syncAcc);
        }
    }
}

void SyncDaemon::sync(SyncAccount *syncAcc, const QString &serviceName)
{
    qDebug() << "syn requested for account:" << syncAcc->displayName() << serviceName;

    // check if the service is already in the sync queue or is the current operation
    if (m_syncQueue->contains(syncAcc, serviceName) ||
        (m_currentAccount == syncAcc) && (serviceName.isEmpty() || (serviceName == m_currentServiceName))) {
        qDebug() << "Account aready in the queue";
    } else {
        qDebug() << "Pushed into queue";
        m_syncQueue->push(syncAcc, serviceName);
        // if not syncing start a full sync
        if (!m_syncing) {
            sync();
        }
    }
}

void SyncDaemon::cancel(SyncAccount *syncAcc, const QString &serviceName)
{
    NotifyMessage::instance()->show("Syncronization",
                                    QString("Sync canceled: %1").arg(syncAcc->displayName()));
    m_syncQueue->remove(syncAcc, serviceName);
    syncAcc->cancel(serviceName);
}

void SyncDaemon::removeAccount(const AccountId &accountId)
{
    SyncAccount *syncAcc = m_accounts.take(accountId);
    if (syncAcc) {
        cancel(syncAcc);
        syncAcc->deleteLater();
    }
}

void SyncDaemon::onAccountSyncStarted(const QString &serviceName, const QString &mode)
{
    if (mode == "slow") {
        NotifyMessage::instance()->show("Syncronization",
                                        QString("Start sync:  %1 (%2)")
                                        .arg(m_currentAccount->displayName())
                                        .arg(serviceName));
    } else {
        qDebug() << "Syncronization"
                 << QString("Start sync:  %1 (%2)")
                    .arg(m_currentAccount->displayName())
                    .arg(serviceName);
    }
}

void SyncDaemon::onAccountSyncFinished(const QString &serviceName, const QString &mode)
{
    qDebug() << "Account sync done" << serviceName << mode;
    if (mode == "slow") {
        NotifyMessage::instance()->show("Syncronization",
                                        QString("Sync done: %1 (%2)")
                                        .arg(m_currentAccount->displayName())
                                        .arg(serviceName));
    } else {
        qDebug() << "Syncronization"
                 << QString("Sync done: %1 (%2)")
                    .arg(m_currentAccount->displayName())
                    .arg(serviceName);
    }

    // sync next account
    continueSync();
}

void SyncDaemon::onAccountSyncError(const QString &serviceName, int errorCode)
{
    NotifyMessage::instance()->show("Syncronization",
                                    QString("Sync error account: %1, %2, %3")
                                    .arg(m_currentAccount->displayName())
                                    .arg(serviceName)
                                    .arg(errorCode));
    // sync next account
    continueSync();
}

void SyncDaemon::onAccountEnableChanged(const QString &serviceName, bool enabled)
{
    SyncAccount *acc = qobject_cast<SyncAccount*>(QObject::sender());
    if (enabled) {
        sync(acc, serviceName);
    } else {
        cancel(acc, serviceName);
    }
}

void SyncDaemon::onAccountConfigured(const QString &serviceName)
{
    SyncAccount *acc = qobject_cast<SyncAccount*>(QObject::sender());
    m_eds->createSource(serviceName, acc->displayName());
}

void SyncDaemon::quit()
{
    m_aboutToQuit = true;

    if (m_eds) {
        delete m_eds;
        m_eds = 0;
    }

    // cancel all sync operation
    while(m_syncQueue->count()) {
        SyncAccount *acc = m_syncQueue->popNext();
        acc->cancel();
        acc->wait();
        delete acc;
    }

    if (m_manager) {
        delete m_manager;
        m_manager = 0;
    }
}
