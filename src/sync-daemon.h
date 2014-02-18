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

#ifndef __SYNC_DAEMON_H__
#define __SYNC_DAEMON_H__

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QQueue>
#include <QtCore/QTimer>

#include <Accounts/Manager>

class SyncAccount;
class AddressBookTrigger;

class SyncDaemon : public QObject
{
    Q_OBJECT
public:
    SyncDaemon();
    ~SyncDaemon();
    void run();

public Q_SLOTS:
    void quit();

private Q_SLOTS:
    void syncAll();
    void continueSync();
    void addAccount(const Accounts::AccountId &accountId, bool startSync=true);
    void removeAccount(const Accounts::AccountId &accountId);

    void onAccountSyncStarted();
    void onAccountSyncFinished();
    void onAccountSyncError(int errorCode);
    void onAccountEnableChanged(bool enabled);
    void onAccountConfigured();

private:
    Accounts::Manager *m_manager;
    QTimer *m_timeout;
    QHash<Accounts::AccountId, SyncAccount*> m_accounts;
    QQueue<SyncAccount*> m_syncQueue;
    SyncAccount *m_currenctAccount;
    AddressBookTrigger *m_addressbook;
    bool m_syncing;
    bool m_aboutToQuit;

    void setupAccounts();
    void setupTriggers();
    void sync(SyncAccount *syncAcc);
    void cancel(SyncAccount *syncAcc);
    void setup();
    void sync();
};

#endif
