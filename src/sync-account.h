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

#include <Accounts/Account>

#include "dbustypes.h"

class SyncEvolutionSessionProxy;
class SyncConfigure;

class SyncAccount : public QObject
{
    Q_OBJECT
public:
    static const QString GoogleCalendarService;
    static const QString GoogleContactService;

    enum AccountState {
        Configuring = 0,
        Syncing,
        Idle,
        Invalid
    };

    SyncAccount(Accounts::Account *account,
                QSettings *settings,
                QObject *parent=0);
    ~SyncAccount();

    void setup();
    void cancel(const QString &serviceName = QString());
    void sync(const QString &serviceName = QString());
    void wait();
    void status() const;
    AccountState state() const;
    bool enabled() const;
    QString displayName() const;
    int id() const;
    QStringList availableServices() const;
    uint lastError() const;

Q_SIGNALS:
    void stateChanged(AccountState newState);
    void syncStarted(const QString &serviceName, bool firstSync);
    void syncFinished(const QString &serviceName, bool firstSync, const QString &status);
    void syncError(const QString &serviceName, int errorCode);
    void enableChanged(const QString &serviceName, bool enable);
    void configured(const QString &serviceName);

private Q_SLOTS:
    void onAccountConfigured();
    void onAccountConfigureError();

    void onAccountEnabledChanged(const QString &serviceName, bool enabled);
    void onSessionStatusChanged(const QString &newStatus);
    void onSessionProgressChanged(int progress);
    void onSessionError(uint error);

private:
    Accounts::Account *m_account;
    SyncEvolutionSessionProxy *m_currentSession;
    QSettings *m_settings;

    QMap<QString, bool> m_availabeServices;
    AccountState m_state;
    QList<QMetaObject::Connection> m_sessionConnections;
    QList<SyncConfigure*> m_pendingConfigs;
    uint m_lastError;

    // current sync information
    QString m_syncMode;
    QString m_syncServiceName;
    bool m_firstSync;

    void configure(const QString &serviceName);
    void setState(AccountState state);
    void continueSync(const QString &serviceName);
    void attachSession(SyncEvolutionSessionProxy *session);
    void releaseSession();
    QStringMap lastReport(const QString &serviceName) const;
    QString syncMode(const QString &serviceName, bool *firstSync) const;
    QString lastSyncStatus(const QString &serviceName) const;
    bool syncService(const QString &serviceName);
    void setupServices();
    QString statusDescription(const QString &status) const;
};

#endif
