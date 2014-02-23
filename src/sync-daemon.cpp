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
#include "address-book-trigger.h"
#include "notify-message.h"

#include <QtCore/QDebug>
#include <QtCore/QTimer>

using namespace Accounts;

#define GOOGLE_PROVIDER_NAME    "google"
#define DAEMON_SYNC_TIMEOUT     1000

SyncDaemon::SyncDaemon()
    : QObject(0),
      m_manager(0),
      m_addressbook(0),
      m_syncing(false),
      m_aboutToQuit(false)
{
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
    m_addressbook = new AddressBookTrigger(this);
    connect(m_addressbook, SIGNAL(contactsUpdated()), SLOT(syncAll()));
}

void SyncDaemon::syncAll()
{
    Q_FOREACH(SyncAccount *acc, m_accounts.values()) {
        sync(acc);
    }
}

void SyncDaemon::sync()
{
    // wait some time for new sync requests
    m_timeout->start();
}

void SyncDaemon::continueSync()
{
    // sync one account by time
    if (!m_aboutToQuit && m_syncQueue.size()) {
        m_syncing = true;
        m_currenctAccount = m_syncQueue.takeFirst();
        m_currenctAccount->sync();
    } else {
        NotifyMessage::instance()->show("Syncronization",
                                        QString("All accounts synced"));
        m_currenctAccount = 0;
        m_syncing = false;
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
    Q_ASSERT(!m_accounts.contains(accountId));
    Account *acc = m_manager->account(accountId);
    if (!acc) {
        qWarning() << "Fail to retrieve accounts:" << m_manager->lastError().message();
    } else if (acc->providerName() == GOOGLE_PROVIDER_NAME) {
        SyncAccount *syncAcc = new SyncAccount(acc, this);
        m_accounts.insert(accountId, syncAcc);
        connect(syncAcc, SIGNAL(syncStarted()), SLOT(onAccountSyncStarted()));
        connect(syncAcc, SIGNAL(syncFinished()), SLOT(onAccountSyncFinished()));
        connect(syncAcc, SIGNAL(syncError(int)), SLOT(onAccountSyncError(int)));
        connect(syncAcc, SIGNAL(enableChanged(bool)), SLOT(onAccountEnableChanged(bool)));
        connect(syncAcc, SIGNAL(configured()), SLOT(onAccountConfigured()), Qt::DirectConnection);
        if (startSync) {
            sync(syncAcc);
        }
    }
}

void SyncDaemon::sync(SyncAccount *syncAcc)
{
    if (!m_syncQueue.contains(syncAcc) &&
        m_currenctAccount != syncAcc) {
        m_syncQueue.push_back(syncAcc);
        if (!m_syncing) {
            sync();
        }
    }
}

void SyncDaemon::cancel(SyncAccount *syncAcc)
{
    NotifyMessage::instance()->show("Syncronization",
                                    QString("Sync canceled: %1").arg(syncAcc->displayName()));
    m_syncQueue.removeOne(syncAcc);
    syncAcc->cancel();
}

void SyncDaemon::removeAccount(const AccountId &accountId)
{
    SyncAccount *syncAcc = m_accounts.take(accountId);
    if (syncAcc) {
        cancel(syncAcc);
        syncAcc->deleteLater();
    }
}

void SyncDaemon::onAccountSyncStarted()
{
    NotifyMessage::instance()->show("Syncronization",
                                    QString("Start sync account: %1").arg(m_currenctAccount->displayName()));
}

void SyncDaemon::onAccountSyncFinished()
{
    // sync next account
    NotifyMessage::instance()->show("Syncronization",
                                    QString("Sync done: %1").arg(m_currenctAccount->displayName()));

    continueSync();
}

void SyncDaemon::onAccountSyncError(int errorCode)
{
    NotifyMessage::instance()->show("Syncronization",
                                    QString("Sync error account: %1, %2")
                                    .arg(m_currenctAccount->displayName())
                                    .arg(errorCode));
    // sync next account
    continueSync();
}

void SyncDaemon::onAccountEnableChanged(bool enabled)
{
    SyncAccount *acc = qobject_cast<SyncAccount*>(QObject::sender());
    if (enabled) {
        sync(acc);
    } else {
        cancel(acc);
    }
}

void SyncDaemon::onAccountConfigured()
{
    SyncAccount *acc = qobject_cast<SyncAccount*>(QObject::sender());
    m_addressbook->createSource(acc->displayName());
}

void SyncDaemon::quit()
{
    m_aboutToQuit = true;

    if (m_addressbook) {
        delete m_addressbook;
        m_addressbook = 0;
    }

    // cancel all sync operation
    while(m_syncQueue.size()) {
        SyncAccount *acc = m_syncQueue.takeFirst();
        acc->cancel();
        acc->wait();
        delete acc;
    }

    if (m_manager) {
        delete m_manager;
        m_manager = 0;
    }
}
