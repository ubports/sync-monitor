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

#include <QtNetwork/QNetworkReply>

#include <Accounts/Account>

#include "dbustypes.h"

class SyncEvolutionSessionProxy;
class SyncConfigure;

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

    SyncAccount(Accounts::Account *account,
                const QSettings *settings,
                QObject *parent=0);
    SyncAccount(Accounts::Account *account,
                const QString &service,
                const QSettings *settings,
                QObject *parent);
    virtual ~SyncAccount();

    virtual void setup();
    void cancel(const QString &serviceName = QString());
    void sync(const QString &serviceName = QString());
    void wait();
    void status() const;
    AccountState state() const;
    bool enabled() const;
    QString displayName() const;
    int id() const;
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
    QString lastSuccessfulSyncDate(const QString &serviceName, const QString &sourceName);
    Accounts::Account *account() const;

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

    void remoteSourcesAvailable(const QArrayOfDatabases &sources);

private Q_SLOTS:
    void onAccountConfigured(const QStringList &services);
    void onAccountConfigureError(const QStringList &services);

    void onAccountEnabledChanged(const QString &serviceName, bool enabled);
    void onSessionStatusChanged(const QString &status, quint32 error, const QSyncStatusMap &sources);
    void onSessionProgressChanged(int progress);

    // calendar list
    void onAuthSucess();
    void onAuthFailed();
    void onReplyFinished(QNetworkReply *reply);

private:
    Accounts::Account *m_account;
    SyncEvolutionSessionProxy *m_currentSession;
    const QSettings *m_settings;
    SyncConfigure *m_config;
    QStringList m_servicesToSync;
    QStringList m_sourcesOnSync;
    QMap<QString, QString> m_currentSyncResults;

    QMap<QString, bool> m_availabeServices;
    AccountState m_state;
    QList<QMetaObject::Connection> m_sessionConnections;
    uint m_lastError;
    bool m_retrySync;

    // current sync information
    QString m_syncMode;
    QString m_syncServiceName;

    void configure();
    void continueSync();

    void setState(AccountState state);
    QStringMap lastReport(const QString &sessionName, const QString &serviceName, const QString &sourceName, bool onlySuccessful = false) const;
    QString syncMode(const QString &serviceName, const QString &sourceName, bool *firstSync) const;
    QString lastSyncStatus(const QString &sessionName, const QString &serviceName, const QString &sourceName) const;
    bool syncService(const QString &serviceName);
    void setupServices();
    void dumpReport(const QStringMap &report) const;
    bool prepareSession(const QString &session = QString::null);
    void attachSession(SyncEvolutionSessionProxy *session);
    void releaseSession();

    QStringList sources(const QString &serviceName) const;
    QStringMap filterSourceReport(const QStringMap &report, const QString &serviceName, uint accountId, const QString &sourceName) const;
};

#endif
