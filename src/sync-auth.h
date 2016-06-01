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

#ifndef __SYNC_AUTH_H__
#define __SYNC_AUTH_H__

#include <Accounts/Account>
#include <Accounts/AccountService>

#include <SignOn/AuthService>
#include <SignOn/Identity>

#include <QtCore/QObject>

class SyncAuth : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString token READ token NOTIFY tokenChanged)

public:
    SyncAuth(uint accountId, const QString &serviceName, QObject *parent = 0);

    QString token() const;
    bool authenticate();

Q_SIGNALS:
    void tokenChanged();
    void fail();
    void success();

private Q_SLOTS:
    void onSessionResponse(const SignOn::SessionData &sessionData);
    void onError(const SignOn::Error &error);

private:
    uint m_accountId;
    QString m_serviceName;
    QString m_token;

    QPointer<Accounts::Manager> m_accountManager;
    QPointer<SignOn::Identity> m_identity;
    QPointer<SignOn::AuthSession> m_session;
    QPointer<Accounts::Account> m_account;
};

#endif
