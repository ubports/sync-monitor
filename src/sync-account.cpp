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

#include "config.h"
#include "sync-account.h"
#include "syncevolution-server-proxy.h"
#include "syncevolution-session-proxy.h"

using namespace Accounts;

#define SESSION_NAME                "ubuntu-google-contacts-%1"
#define TARGET_CONFIG_NAME          "target-config@google-%1"
#define ACCOUNT_CONFIG_NAME         "google-%1"

const QString SyncAccount::GoogleCalendarService = QStringLiteral("google-carddav");
const QString SyncAccount::GoogleContactService = QStringLiteral("google-carddav");

SyncAccount::SyncAccount(Account *account, QObject *parent)
    : QObject(parent),
      m_currentSession(0),
      m_account(account),
      m_state(SyncAccount::Empty)
{
    m_sessionName = QString(SESSION_NAME).arg(account->id());
    m_syncOperation.insert(GoogleCalendarService, QStringLiteral("disabled"));
    m_syncOperation.insert(GoogleContactService, QStringLiteral("disabled"));
    setup();
}

SyncAccount::~SyncAccount()
{
    cancel();
}

void SyncAccount::setup()
{
    // enable sync for enabled service on account
    Q_FOREACH(Service service, m_account->enabledServices()) {
        if (m_syncOperation.contains(service.name())) {
            m_syncOperation[service.name()] = QStringLiteral("two-way");
        }
    }

    connect(m_account,
            SIGNAL(enabledChanged(QString,bool)),
            SLOT(onAccountEnabledChanged(QString,bool)));
}

void SyncAccount::cancel()
{
    if (m_currentSession) {
        m_currentSession->destroy();
        m_currentSession = 0;
    }
}

void SyncAccount::sync()
{
    switch(m_state) {
    case SyncAccount::Empty:
        configure();
        break;
    case SyncAccount::Idle:
        continueSync();
        break;
    default:
        break;
    }
}

void SyncAccount::continueSync()
{
    SyncEvolutionServerProxy *proxy = SyncEvolutionServerProxy::instance();
    SyncEvolutionSessionProxy *session = proxy->openSession(QString(ACCOUNT_CONFIG_NAME).arg(m_account->id()),
                                                            QStringList());
    if (session) {
        attachSession(session);

        QStringMap syncFlags;
        bool forceSlowSync = lastSyncStatus() != "0";
        syncFlags.insert("addressbook", (forceSlowSync ? "slow" : m_syncOperation[GoogleContactService]));
        session->sync(syncFlags);
    } else {
        setState(SyncAccount::Invalid);
    }
}

void SyncAccount::wait()
{
    //TODO
}

SyncAccount::AccountState SyncAccount::state() const
{
    return m_state;
}

QDateTime SyncAccount::lastSyncDate() const
{
    QStringMap lastReport = this->lastReport();
    if (lastReport.contains("start")) {
        QString lastSync = lastReport.value("start", "0");
        return QDateTime::fromTime_t(lastSync.toInt());
    } else {
        return QDateTime();
    }
}

QString SyncAccount::lastSyncStatus() const
{
    QStringMap lastReport = this->lastReport();
    qDebug() << "last report" << lastReport;
    return lastReport.value("source-addressbook-status", "-1");
}

QStringMap SyncAccount::lastReport() const
{
    const uint pageSize = 100;
    uint index = 0;
    QStringMap lastReport;

    // load all reports
    QArrayOfStringMap reports = m_currentSession->reports(index, pageSize);
    while (reports.size() == pageSize) {
        lastReport = reports.last();
        index += pageSize;
        reports = m_currentSession->reports(index, pageSize);
    }

    if (reports.size()) {
        lastReport = reports.last();
    }

    return lastReport;
}

bool SyncAccount::enabled() const
{
    return m_account->enabled();
}

QString SyncAccount::displayName() const
{
    return m_account->displayName();
}

int SyncAccount::id() const
{
    return m_account->id();
}

void SyncAccount::onAccountEnabledChanged(const QString &serviceName, bool enabled)
{
    if (m_syncOperation.contains(serviceName)) {
        m_syncOperation[serviceName] = (enabled ? QStringLiteral("two-way") : QStringLiteral("disabled"));
        Q_EMIT enableChanged(enabled);
    }
}

bool SyncAccount::configServerSide()
{
    AccountId accountId = m_account->id();
    // config server side
    QStringMultiMap config = m_currentSession->getConfig("SyncEvolution", true);
    Q_ASSERT(!config.isEmpty());
    config[""]["syncURL"] = QStringLiteral("https://www.googleapis.com/.well-known/carddav");
    config[""]["username"] = QString("uoa:%1,google-carddav").arg(accountId);
    config[""]["consumerReady"] = "0";
    config["source/addressbook"]["backend"] = "CardDAV";
    config.remove("source/calendar");
    config.remove("source/todo");
    config.remove("source/memo");
    bool result = m_currentSession->saveConfig(QString(TARGET_CONFIG_NAME).arg(accountId), config);
    if (!result) {
        qWarning() << "Fail to save account client config";
        return false;
    }
    return true;
}

bool SyncAccount::configClientSide()
{
    Q_ASSERT(m_currentSession);
    AccountId accountId = m_account->id();
    // config server side
    QStringMultiMap config = m_currentSession->getConfig("SyncEvolution_Client", true);
    Q_ASSERT(!config.isEmpty());
    config[""]["syncURL"] = QString("local://@google-%1").arg(accountId);
    config[""]["username"] = QString();
    config[""]["password"] = QString();
    config["source/addressbook"]["database"] = m_account->displayName();
    config.remove("source/calendar");
    config.remove("source/todo");
    config.remove("source/memo");
    bool result = m_currentSession->saveConfig(QString(ACCOUNT_CONFIG_NAME).arg(accountId), config);
    if (!result) {
        qWarning() << "Fail to save account client config";
        return false;
    }
    return result;
}

void SyncAccount::continueConfigure()
{
    Q_ASSERT(m_currentSession);
    AccountId accountId = m_account->id();
    bool isConfigured = m_currentSession->hasConfig(QString(TARGET_CONFIG_NAME).arg(accountId));
    if (isConfigured) {
        qDebug() << "Account already configured";
    } else if (configServerSide() && configClientSide()) {
        qDebug() << "Account configured";
        Q_EMIT configured();
    } else {
        qWarning() << "Fail to configure account" << accountId;
        setState(SyncAccount::Invalid);
    }
    releaseSession();
    if (state() != SyncAccount::Invalid) {
        setState(SyncAccount::Idle);
        continueSync();
    }
}

void SyncAccount::onSessionStatusChanged(const QString &newStatus)
{
    qDebug() << "Session status changed" << newStatus;
    switch (m_state) {
    case SyncAccount::Idle:
        if (newStatus == "running") {
            setState(SyncAccount::Syncing);
            Q_EMIT syncStarted();
        }
        break;
    case SyncAccount::Configuring:
        if (newStatus != "queueing") {
            continueConfigure();
        }
        break;
    case SyncAccount::Syncing:
        if (newStatus == "done") {
            releaseSession();
            setState(SyncAccount::Idle);
            Q_EMIT syncFinished();
        }
        break;
    default:
        break;
    }
}

void SyncAccount::onSessionProgressChanged(int progress)
{
    qDebug() << "Progress" << progress;
}

void SyncAccount::onSessionError(uint error)
{
    qWarning() << "Session error" << error;
    setState(SyncAccount::Invalid);
}

void SyncAccount::configure()
{
    qDebug() << "Configure account";
    if (m_state == SyncAccount::Empty) {
        setState(SyncAccount::Configuring);
        SyncEvolutionServerProxy *proxy = SyncEvolutionServerProxy::instance();
        SyncEvolutionSessionProxy *session = proxy->openSession(m_sessionName, QStringList() << "all-configs");
        if (session) {
            attachSession(session);
            qDebug() << "Configure account status" << session->status();
            if (session->status() != "queueing") {
                continueConfigure();
            }
        } else {
            setState(SyncAccount::Invalid);
            qWarning() << "Fail to configure account" << m_account->id() << m_account->displayName();
        }
    } else {
        qWarning() << "Called configure for a account with invalid state" << m_state;
    }
}

void SyncAccount::setState(SyncAccount::AccountState state)
{
    if (m_state != state) {
        m_state = state;
        Q_EMIT stateChanged(m_state);
    }
}

void SyncAccount::attachSession(SyncEvolutionSessionProxy *session)
{
    Q_ASSERT(m_currentSession == 0);
    m_currentSession = session;
    m_sessionConnections << connect(m_currentSession,
                                    SIGNAL(statusChanged(QString)),
                                    SLOT(onSessionStatusChanged(QString)));
    m_sessionConnections << connect(m_currentSession,
                                    SIGNAL(progressChanged(int)),
                                    SLOT(onSessionProgressChanged(int)));
    m_sessionConnections << connect(m_currentSession,
                                    SIGNAL(error(uint)),
                                    SLOT(onSessionError(uint)));

}

void SyncAccount::releaseSession()
{
    if (m_currentSession) {
        Q_FOREACH(QMetaObject::Connection conn, m_sessionConnections) {
            disconnect(conn);
        }
        m_sessionConnections.clear();
        m_currentSession->destroy();
        m_currentSession->deleteLater();
        m_currentSession = 0;
    }
}

