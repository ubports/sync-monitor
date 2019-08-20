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
#include <QtCore/QSettings>

#include <Accounts/Manager>

#include "sync-network.h"
#include "sync-queue.h"

class SyncAccount;
class EdsHelper;
class ProviderTemplate;
class SyncDBus;
class PowerdProxy;

class SyncDaemon : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool syncOnMobileConnection READ syncOnMobileConnection WRITE setSyncOnMobileConnection)
public:
    SyncDaemon();
    ~SyncDaemon();
    void run();
    bool isPending() const;
    bool isSyncing() const;
    QStringList availableServices() const;
    QStringList enabledServices() const;
    bool isOnline() const;
    QString lastSuccessfulSyncDate(quint32 accountId, const QString &calendarId);
    bool syncOnMobileConnection() const;
    void setSyncOnMobileConnection(bool flag);

    SyncAccount *accountById(quint32 accountId);

Q_SIGNALS:
    void syncStarted(SyncAccount *syncAcc, const QString &source);
    void syncFinished(SyncAccount *syncAcc, const QString &source);
    void syncError(SyncAccount *syncAcc, const QString &source, const QString &error);
    void syncAboutToStart();
    void done();
    void accountsChanged();
    void isOnlineChanged(bool isOnline);

public Q_SLOTS:
    void quit();
    void syncAll(bool runNow, bool syncOnMobile);
    void syncAccount(quint32 accountId, const QStringList &calendars, bool runNow = true, bool syncOnMobile = false);
    void cancel(uint accountId = 0, const QStringList &sources = QStringList());

private Q_SLOTS:
    void continueSync();
    void addAccount(const Accounts::AccountId &accountId, bool startSync=true);
    void removeAccount(const Accounts::AccountId &accountId);
    void destroyAccount();
    void authenticateAccount(const SyncAccount *account,
                             const QString &serviceName);
    void runAuthentication();


    void onAccountSyncStart();
    void onAccountSyncFinished(const QString &serviceName, const QMap<QString, QString> &statusList);
    void onAccountSourceSyncStarted(const QString &serviceName, const QString &source, bool firstSync);
    void onAccountSourceSyncFinished(const QString &serviceName, const QString &sourceName, const bool firstSync, const QString &status, const QString &mode);
    void onAccountSyncError(const QString &serviceName, const QString &error);
    void onAccountEnableChanged(const QString &serviceName, bool enabled);
    void onAccountSourceRemoved(const QString &source);
    void onDataChanged(const QString &sourceId);
    void onClientAttached();

    void onOnlineStatusChanged(SyncNetwork::NetworkState state);

private:
    Accounts::Manager *m_manager;
    QTimer *m_timeout;
    QHash<Accounts::AccountId, SyncAccount*> m_accounts;
    SyncQueue *m_syncQueue;
    SyncQueue *m_offlineQueue;
    SyncJob m_currentJob;
    EdsHelper *m_eds;
    ProviderTemplate *m_provider;
    SyncDBus *m_dbusAddaptor;
    SyncNetwork *m_networkStatus;
    PowerdProxy *m_powerd;
    bool m_syncing;
    bool m_wentOffline;
    bool m_aboutToQuit;
    QElapsedTimer m_syncElapsedTime;
    bool m_firstClient;
    QSettings m_settings;

    void setupAccounts();
    void setupTriggers();
    void cleanupConfig();
    void sync(SyncAccount *syncAcc, const QStringList &calendars, bool runNow, bool syncOnMobile);
    void cancel(SyncAccount *syncAcc, const QStringList &sources);
    void sync(bool runNow);
    bool registerService();
    void syncFinishedImpl();

    void saveSyncResult(uint accountId, const QString &sourceName, const QString &result, const QString &date);
    void clearResultForSource(uint accountId, const QString &sourceName);
    QString loadSyncResult(uint accountId, const QString &sourceName);
    bool isFirstSync(uint accountId);
    void cleanupLogs();
};

#endif
