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


#include "sync-auth.h"

#include <Accounts/Manager>
#include <Accounts/Account>

#include <QtCore/QDebug>

using namespace Accounts;
using namespace SignOn;


SyncAuth::SyncAuth(uint accountId, const QString &serviceName, QObject *parent)
    : QObject(parent),
      m_accountId(accountId),
      m_serviceName(serviceName),
      m_accountManager(new Manager(this))
{
    m_account = Account::fromId(m_accountManager, accountId, this);
}

QString SyncAuth::token() const
{
    return m_token;
}

bool SyncAuth::authenticate()
{
    if (!m_account) {
        qWarning() << "Invalid account id:" << m_accountId;
        return false;
    }

    if (m_session) {
        qWarning() << QString("error: Account %1 Authenticate already requested")
                .arg(m_account->displayName());
        return true;
    }

    Accounts::Service srv(m_accountManager->service(m_serviceName));
    if (!srv.isValid()) {
        qWarning() << QString("error: Service [%1] not found for account [%2].")
                .arg(m_serviceName)
                .arg(m_account->displayName());
        return false;
    }
    m_account->selectService(srv);

    Accounts::AccountService *accSrv = new Accounts::AccountService(m_account, srv);
    if (!accSrv) {
        qWarning() << QString("error: Account %1 has no valid account service")
                      .arg(m_account->displayName());
        return false;
    }

    if (!accSrv->isEnabled()) {
        qWarning() << QString("error: Service %1 not enabled for account %2.")
                .arg(m_serviceName)
                .arg(m_account->displayName());
        accSrv->deleteLater();
        return false;

    }

    QVariantMap signonSessionData;
    AuthData authData = authData = accSrv->authData();
    m_identity = SignOn::Identity::existingIdentity(authData.credentialsId());
    if (!m_identity) {
        qWarning() << QString("error: Account %1 has no valid credentials")
                .arg(m_account->displayName());
        goto auth_error;
    }

    m_session = m_identity->createSession(authData.method());
    if (!m_session) {
        qWarning() << QString("error: could not create signon session for Google account %1")
                .arg(m_account->displayName());
        goto auth_error;
    }

    connect(m_session.data(),SIGNAL(response(SignOn::SessionData)),
            SLOT(onSessionResponse(SignOn::SessionData)), Qt::QueuedConnection);
    connect(m_session.data(), SIGNAL(error(SignOn::Error)),
            SLOT(onError(SignOn::Error)), Qt::QueuedConnection);

    signonSessionData = authData.parameters();
    signonSessionData.insert("UiPolicy", SignOn::NoUserInteractionPolicy);
    m_session->process(signonSessionData, authData.mechanism());
    accSrv->deleteLater();
    return true;

auth_error:
    accSrv->deleteLater();
    m_session.data()->deleteLater();
    return false;
}

void SyncAuth::onSessionResponse(const SignOn::SessionData &sessionData)
{
    Q_ASSERT(m_session);
    m_session->disconnect(this);
    m_session.data()->deleteLater();

    m_token = sessionData.getProperty(QStringLiteral("AccessToken")).toString();
    qDebug() << "Authenticated !!!";

    Q_EMIT tokenChanged();
    Q_EMIT success();
}

void SyncAuth::onError(const SignOn::Error &error)
{
    Q_ASSERT(m_session);
    m_session->disconnect(this);
    m_session.data()->deleteLater();

    qWarning() << "Fail to authenticate:" << error.message();

    m_token = "";
    Q_EMIT tokenChanged();
    Q_EMIT fail();
}
