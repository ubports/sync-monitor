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
#include "sync-dbus.h"
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
      m_dbusAddaptor(0),
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
    Q_EMIT accountsChanged();
}

void SyncDaemon::setupTriggers()
{
    m_eds = new EdsHelper(this);
    connect(m_eds, &EdsHelper::dataChanged,
            this, &SyncDaemon::onDataChanged);
}

void SyncDaemon::onDataChanged(const QString &serviceName, const QString &sourceName)
{
    if (sourceName.isEmpty()) {
        syncAll(serviceName);
    } else {
        Q_FOREACH(SyncAccount *acc, m_accounts.values()) {
            if (acc->displayName() == sourceName) {
                sync(acc, serviceName);
                return;
            }
        }
    }
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

void SyncDaemon::cancel(const QString &serviceName)
{
    Q_FOREACH(SyncAccount *acc, m_accounts.values()) {
        if (serviceName.isEmpty()) {
            cancel(acc);
        } else if (acc->availableServices().contains(serviceName)) {
            cancel(acc, serviceName);
        }
    }
}

void SyncDaemon::sync()
{
    m_syncing = true;
    // wait some time for new sync requests
    m_timeout->start();
}

void SyncDaemon::continueSync()
{
    // sync the next service on the queue
    if (!m_aboutToQuit && !m_syncQueue->isEmpty()) {
        m_currentServiceName = m_syncQueue->popNext(&m_currentAccount);
        m_currentAccount->sync(m_currentServiceName);
    } else {
        m_currentAccount = 0;
        m_currentServiceName.clear();
        m_syncing = false;
        Q_EMIT done();
    }
}

bool SyncDaemon::registerService()
{
    if (!m_dbusAddaptor) {
        QDBusConnection connection = QDBusConnection::sessionBus();
        if (connection.interface()->isServiceRegistered(SYNCMONITOR_SERVICE_NAME)) {
            qWarning() << "SyncMonitor service already registered";
            return false;
        } else if (!connection.registerService(SYNCMONITOR_SERVICE_NAME)) {
            qWarning() << "Could not register service!" << SYNCMONITOR_SERVICE_NAME;
            return false;
        }

        m_dbusAddaptor = new SyncDBus(connection, this);
        if (!connection.registerObject(SYNCMONITOR_OBJECT_PATH, this))
        {
            qWarning() << "Could not register object!" << SYNCMONITOR_OBJECT_PATH;
            delete m_dbusAddaptor;
            m_dbusAddaptor = 0;
            return false;
        }
    }
    return true;
}

void SyncDaemon::run()
{
    setupAccounts();
    setupTriggers();

    // export dbus interface
    registerService();
}

bool SyncDaemon::isSyncing() const
{
    return m_syncing;
}

QStringList SyncDaemon::availableServices() const
{
    // TODO: check for all providers
    return m_provider->supportedServices("google");
}

QStringList SyncDaemon::enabledServices() const
{
    QSet<QString> services;
    QStringList available = availableServices();
    Q_FOREACH(SyncAccount *syncAcc, m_accounts) {
        Q_FOREACH(const QString &service, syncAcc->enabledServices()) {
            if (available.contains(service)) {
                services << service;
            }
        }
    }
    return services.toList();
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
        connect(syncAcc, SIGNAL(syncStarted(QString, bool)),
                         SLOT(onAccountSyncStarted(QString, bool)));
        connect(syncAcc, SIGNAL(syncFinished(QString, bool, QString)),
                         SLOT(onAccountSyncFinished(QString, bool, QString)));
        connect(syncAcc, SIGNAL(syncError(QString, int)),
                         SLOT(onAccountSyncError(QString, int)));
        connect(syncAcc, SIGNAL(enableChanged(QString, bool)),
                         SLOT(onAccountEnableChanged(QString, bool)));
        connect(syncAcc, SIGNAL(configured(QString)),
                         SLOT(onAccountConfigured(QString)), Qt::DirectConnection);
        if (startSync) {
            sync(syncAcc);
        }
        Q_EMIT accountsChanged();
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
            Q_EMIT syncAboutToStart();
        }
    }
}

void SyncDaemon::cancel(SyncAccount *syncAcc, const QString &serviceName)
{
    NotifyMessage::instance()->show("Syncronization",
                                    QString("Sync canceled: %1").arg(syncAcc->displayName()));
    m_syncQueue->remove(syncAcc, serviceName);
    syncAcc->cancel(serviceName);
    Q_EMIT syncError(syncAcc, serviceName, "canceled");
}

void SyncDaemon::removeAccount(const AccountId &accountId)
{
    SyncAccount *syncAcc = m_accounts.take(accountId);
    if (syncAcc) {
        cancel(syncAcc);
        syncAcc->deleteLater();
    }
    Q_EMIT accountsChanged();
}

void SyncDaemon::onAccountSyncStarted(const QString &serviceName, bool firstSync)
{
    if (firstSync) {
        NotifyMessage::instance()->show("Syncronization",
                                        QString("Start sync:  %1 (%2)")
                                        .arg(m_currentAccount->displayName())
                                        .arg(serviceName));
    }
    m_syncElapsedTime.restart();
    qDebug() << QString("[%3] Start sync:  %1 (%2)")
                .arg(m_currentAccount->displayName())
                .arg(serviceName)
                .arg(QDateTime::currentDateTime().toString(Qt::SystemLocaleShortDate));
    Q_EMIT syncStarted(m_currentAccount, serviceName);
}

void SyncDaemon::onAccountSyncFinished(const QString &serviceName, const bool firstSync, const QString &status)
{
    QString errorMessage = SyncAccount::statusDescription(status);
    if (firstSync && errorMessage.isEmpty()) {
        NotifyMessage::instance()->show("Syncronization",
                                        QString("Sync done: %1 (%2)")
                                        .arg(m_currentAccount->displayName())
                                        .arg(serviceName));
    } else if (!errorMessage.isEmpty()) {
        NotifyMessage::instance()->show("Syncronization",
                                        QString("Fail to sync %1 (%2).\n%3")
                                        .arg(m_currentAccount->displayName())
                                        .arg(serviceName)
                                        .arg(errorMessage));
    }

    qDebug() << QString("[%6] Sync done: %1 (%2) Status: %3 Error: %4 Duration: %5s")
                .arg(m_currentAccount->displayName())
                .arg(serviceName)
                .arg(status)
                .arg(errorMessage.isEmpty() ? "None" : errorMessage)
                .arg((m_syncElapsedTime.elapsed() < 1000 ? 1  : m_syncElapsedTime.elapsed() / 1000))
                .arg(QDateTime::currentDateTime().toString(Qt::SystemLocaleShortDate));

    Q_EMIT syncFinished(m_currentAccount, serviceName);
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

    Q_EMIT syncError(m_currentAccount, serviceName, QString(errorCode));
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
    Q_EMIT accountsChanged();
}

void SyncDaemon::onAccountConfigured(const QString &serviceName)
{
    SyncAccount *acc = qobject_cast<SyncAccount*>(QObject::sender());
    m_eds->createSource(serviceName, acc->displayName());
}

void SyncDaemon::quit()
{
    m_aboutToQuit = true;

    if (m_dbusAddaptor) {
        delete m_dbusAddaptor;
        m_dbusAddaptor = 0;
    }

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
