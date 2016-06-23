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

#ifndef __SYNC_ACCOUNT_H__
#define __SYNC_ACCOUNT_H__

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QSettings>
#include <QtCore/QPair>
#include <QtCore/QString>

#include <QtNetwork/QNetworkReply>

#include <Accounts/Account>

#include "dbustypes.h"

class SyncEvolutionSessionProxy;
class SyncConfigure;

class SourceData
{
public:
    QString sourceName;
    QString remoteId;
    bool writable;

    SourceData(const QString &_sourceName, const QString &_remoteId, bool _writable)
        : sourceName(_sourceName), remoteId(_remoteId), writable(_writable)
    {}
};

class SyncAccount : public QObject
{
    Q_OBJECT
public:
    enum AccountState {
        Configuring = 0,
        AboutToSync,
        Syncing,
        Idle,
        Invalid
    };

    enum SourceState {
        SourceSyncStarting = 0,
        SourceSyncRunning,
        SourceSyncDone
    };

    SyncAccount(Accounts::Account *account,
                const QSettings *settings,
                QObject *parent=0);
    SyncAccount(Accounts::Account *account,
                const QString &service,
                const QSettings *settings,
                QObject *parent);
    virtual ~SyncAccount();

    virtual void setup();
    void cancel(const QStringList &sources = QStringList());
    void sync(const QStringList &sources = QStringList());
    void wait();
    void status() const;
    AccountState state() const;
    bool enabled() const;
    QString displayName() const;
    virtual int id() const;
    QString iconName(const QString &serviceName) const;
    virtual QStringList availableServices() const;
    QStringList enabledServices() const;
    uint lastError() const;
    void removeConfig();
    void removeOldConfig() const;
    void setLastError(uint errorCode);
    QString serviceId(const QString &serviceName) const;
    bool retrySync() const;
    void setRetrySync(bool retry);
    Accounts::Account *account() const;
    QDateTime lastSyncTime() const;

    void fetchRemoteSources(const QString &serviceName);

    static QString statusDescription(const QString &status);

Q_SIGNALS:
    void stateChanged(AccountState newState);
    void syncSourceStarted(const QString &serviceName, const QString &sourceName, bool firstSync);
    void syncSourceFinished(const QString &serviceName, const QString &sourceName, bool firstSync, const QString &status, const QString &mode);

    void syncStarted();
    void syncFinished(const QString &serviceName, QMap<QString, QString> sourcesStatus);
    void syncError(const QString &serviceName, const QString &syncError);

    void enableChanged(const QString &serviceName, bool enable);
    void configured(const QStringList &services);
    void sourceRemoved(const QString &sourceName);

    void remoteSourcesAvailable(const QArrayOfDatabases &sources, int error);

private Q_SLOTS:
    void onAccountConfigured(const QStringList &services);
    void onAccountConfigureError(int error);

    void onAccountEnabledChanged(const QString &serviceName, bool enabled);
    void onSessionStatusChanged(const QString &status, quint32 error, const QSyncStatusMap &sources);
    void onSessionProgressChanged(int progress);

    // calendar list
    void onAuthSucess();
    void onAuthFailed();
    void onReplyFinished(QNetworkReply *reply);

private:
    Accounts::Account *m_account;
    QDateTime m_startSyncTime;
    SyncEvolutionSessionProxy *m_currentSession;
    const QSettings *m_settings;
    SyncConfigure *m_config;
    QStringList m_sourcesToSync;
    QMap<QString, SyncAccount::SourceState> m_sourcesOnSync;
    QMap<QString, QString> m_currentSyncResults;
    QElapsedTimer m_syncTime;

    QMap<QString, bool> m_availabeServices;
    AccountState m_state;
    QList<QMetaObject::Connection> m_sessionConnections;
    uint m_lastError;
    bool m_retrySync;
    QArrayOfDatabases m_remoteSources;

    // current sync information
    QString m_syncMode;
    QString m_syncServiceName;

    void configure();
    void continueSync();

    void setState(AccountState state);
    QString syncMode(const QString &sourceName, bool *firstSync) const;
    bool syncService(const QString &serviceName);
    void setupServices();

    // session control
    bool prepareSession(const QString &session = QString::null);
    void attachSession(SyncEvolutionSessionProxy *session);
    void releaseSession();

    QList<SourceData> sources() const;
    QStringMap filterSourceReport(const QStringMap &report, const QString &serviceName, uint accountId, const QString &sourceName) const;

    QString lastSyncStatus(const QString &sourceName) const;
};

#endif
