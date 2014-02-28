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

#include <Accounts/Account>

#include "dbustypes.h"

class SyncEvolutionSessionProxy;
class SyncAccount : public QObject
{
    Q_OBJECT
public:
    static const QString GoogleCalendarService;
    static const QString GoogleContactService;

    enum AccountState {
        Empty,
        Configuring,
        Syncing,
        Idle,
        Invalid
    };

    SyncAccount(Accounts::Account *account, QObject *parent=0);
    ~SyncAccount();

    void setup();
    void cancel();
    void sync();
    void wait();
    void status() const;
    AccountState state() const;
    QDateTime lastSyncDate() const;
    bool enabled() const;
    QString displayName() const;
    int id() const;

Q_SIGNALS:
    void stateChanged(AccountState newState);
    void syncStarted();
    void syncFinished();
    void syncError(int);
    void enableChanged(bool enable);
    void configured();

private Q_SLOTS:
    void onAccountEnabledChanged(const QString &serviceName, bool enabled);
    void onSessionStatusChanged(const QString &newStatus);
    void onSessionProgressChanged(int progress);
    void onSessionError(uint error);

private:
    Accounts::Account *m_account;
    SyncEvolutionSessionProxy *m_currentSession;

    QString m_sessionName;
    QStringMap m_syncSources;
    QStringMap m_syncOperation;
    AccountState m_state;
    QList<QMetaObject::Connection> m_sessionConnections;

    void configure();
    void continueConfigure();
    bool configSync();
    bool configTarget();
    void setState(AccountState state);
    void continueSync();
    void attachSession(SyncEvolutionSessionProxy *session);
    void releaseSession();
    QStringMap lastReport() const;
    QString lastSyncStatus() const;
};

#endif
