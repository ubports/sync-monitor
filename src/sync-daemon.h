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
#include <QtCore/QElapsedTimer>

#include <Accounts/Manager>

#include "sync-network.h"

class SyncAccount;
class EdsHelper;
class ProviderTemplate;
class SyncQueue;
class SyncDBus;

class SyncDaemon : public QObject
{
    Q_OBJECT
public:
    SyncDaemon();
    ~SyncDaemon();
    void run();
    bool isPending() const;
    bool isSyncing() const;
    QStringList availableServices() const;
    QStringList enabledServices() const;
    bool isOnline() const;

Q_SIGNALS:
    void syncStarted(SyncAccount *syncAcc, const QString &serviceName);
    void syncFinished(SyncAccount *syncAcc, const QString &serviceName);
    void syncError(SyncAccount *syncAcc, const QString &serviceName, const QString &error);
    void syncAboutToStart();
    void done();
    void accountsChanged();
    void isOnlineChanged(bool isOnline);

public Q_SLOTS:
    void quit();
    void syncAll(const QString &serviceName = QString(), bool runNow=false);
    void cancel(const QString &serviceName = QString());

private Q_SLOTS:
    void continueSync();
    void addAccount(const Accounts::AccountId &accountId, bool startSync=true);
    void removeAccount(const Accounts::AccountId &accountId);
    void removeAccountSource();
    void destroyAccount();
    void authenticateAccount(const SyncAccount *account,
                             const QString &serviceName);
    void runAuthentication();

    void onAccountSourceSyncStarted(const QString &serviceName, const QString &source, bool firstSync);
    void onAccountSourceSyncFinished(const QString &serviceName, const QString &sourceName, const bool firstSync, const QString &status, const QString &mode);
    void onAccountSyncFinished(const QString &serviceName, const QMap<QString, QString> &statusList);
    void onAccountEnableChanged(const QString &serviceName, bool enabled);
    void onDataChanged(const QString &serviceName, const QString &sourceName);
    void onClientAttached();

    void onOnlineStatusChanged(SyncNetwork::NetworkState state);

private:
    Accounts::Manager *m_manager;
    QTimer *m_timeout;
    QHash<Accounts::AccountId, SyncAccount*> m_accounts;
    SyncQueue *m_syncQueue;
    SyncQueue *m_offlineQueue;
    SyncAccount *m_currentAccount;
    QString m_currentServiceName;
    EdsHelper *m_eds;
    ProviderTemplate *m_provider;
    SyncDBus *m_dbusAddaptor;
    SyncNetwork *m_networkStatus;
    bool m_syncing;
    bool m_aboutToQuit;
    QElapsedTimer m_syncElapsedTime;
    bool m_firstClient;

    void setupAccounts();
    void setupTriggers();
    void sync(SyncAccount *syncAcc, const QString &serviceName = QString(), bool runNow = false);
    void cancel(SyncAccount *syncAcc, const QString &serviceName = QString());
    void setup();
    void sync(bool runNow);
    bool registerService();
    void syncFinishedImpl();
};

#endif
