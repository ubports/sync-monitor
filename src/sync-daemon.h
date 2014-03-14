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
#include <QtCore/QTimer>

#include <Accounts/Manager>

class SyncAccount;
class EdsHelper;
class ProviderTemplate;
class SyncQueue;

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
    void syncAll(const QString &serviceName = QString());
    void continueSync();
    void addAccount(const Accounts::AccountId &accountId, bool startSync=true);
    void removeAccount(const Accounts::AccountId &accountId);

    void onAccountSyncStarted(const QString &serviceName, const QString &mode);
    void onAccountSyncFinished(const QString &serviceName, const QString &mode);
    void onAccountSyncError(const QString &serviceName, int errorCode);
    void onAccountEnableChanged(const QString &serviceName, bool enabled);
    void onAccountConfigured(const QString &serviceName);
    void onDataChanged(const QString &serviceName, const QString &sourceName);

private:
    Accounts::Manager *m_manager;
    QTimer *m_timeout;
    QHash<Accounts::AccountId, SyncAccount*> m_accounts;
    SyncQueue *m_syncQueue;
    SyncAccount *m_currentAccount;
    QString m_currentServiceName;
    EdsHelper *m_eds;
    ProviderTemplate *m_provider;
    bool m_syncing;
    bool m_aboutToQuit;

    void setupAccounts();
    void setupTriggers();
    void sync(SyncAccount *syncAcc, const QString &serviceName = QString());
    void cancel(SyncAccount *syncAcc, const QString &serviceName = QString());
    void setup();
    void sync();
};

#endif
