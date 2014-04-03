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

#include "sync-account.h"
#include "sync-configure.h"
#include "syncevolution-server-proxy.h"
#include "syncevolution-session-proxy.h"

#include "config.h"

using namespace Accounts;

SyncAccount::SyncAccount(Account *account,
                         QSettings *settings,
                         QObject *parent)
    : QObject(parent),
      m_currentSession(0),
      m_account(account),
      m_state(SyncAccount::Idle),
      m_settings(settings)
{
    setup();
}

SyncAccount::~SyncAccount()
{
    cancel();
}

// Load all available services for the online-account
// fill the m_availabeServices with the service name
// and a flag to say it is enabled or not
void SyncAccount::setupServices()
{
    m_availabeServices.clear();
    QStringList supportedSevices = m_settings->childGroups();
    ServiceList enabledServices = m_account->enabledServices();
    Q_FOREACH(Service service, m_account->services()) {
        if (supportedSevices.contains(service.serviceType())) {
            bool enabled = m_account->enabled() && enabledServices.contains(service);
            m_availabeServices.insert(service.serviceType(), enabled);
        }
    }
    qDebug() << "Supported sevices for protocol:" << m_account->providerName() << supportedSevices;
    qDebug() << "Services available for:" << m_account->displayName() << m_availabeServices;
}

void SyncAccount::setup()
{
    setupServices();
    connect(m_account,
            SIGNAL(enabledChanged(QString,bool)),
            SLOT(onAccountEnabledChanged(QString,bool)));
}

void SyncAccount::cancel(const QString &serviceName)
{
    //TODO: cancel the only the seviceName sync
    if (m_currentSession) {
        m_currentSession->destroy();
        m_currentSession = 0;
    }
}

void SyncAccount::sync(const QString &serviceName)
{
    switch(m_state) {
    case SyncAccount::Idle:
        qDebug() << "Sync requested service:" << m_account->displayName() << serviceName;
        configure(serviceName);
        break;
    default:
        break;
    }
}

bool SyncAccount::syncService(const QString &serviceName)
{
    bool enabledService = m_availabeServices.value(serviceName, false);
    if (!enabledService) {
        Q_EMIT syncFinished(serviceName, false, "");
        return true;
    }

    m_syncServiceName = serviceName;
    QString sessionName = QString("%1-%2-%3")
            .arg(m_account->providerName())
            .arg(serviceName)
            .arg(m_account->id());

    QString sourceName = QString("%1_uoa_%2")
            .arg(serviceName)
            .arg(m_account->id());

    SyncEvolutionServerProxy *proxy = SyncEvolutionServerProxy::instance();
    SyncEvolutionSessionProxy *session = proxy->openSession(sessionName,
                                                            QStringList());
    if (session) {
        attachSession(session);

        QStringMap syncFlags;
        m_syncMode = syncMode(serviceName, &m_firstSync);
        syncFlags.insert(sourceName, m_syncMode);
        qDebug() << "sync Mod" << syncFlags;
        session->sync(m_syncMode, syncFlags);
        return true;
    } else {
        setState(SyncAccount::Invalid);
        return false;
    }
}

void SyncAccount::continueSync(const QString &serviceName)
{
    QStringList services;
    if (serviceName.isEmpty()) {
        services  = m_settings->childGroups();
    } else {
        services << serviceName;
    }

    Q_FOREACH(const QString &service, services) {
        syncService(service);
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

QStringMap SyncAccount::lastReport(const QString &serviceName) const
{
    const uint pageSize = 100;
    uint index = 0;
    QArrayOfStringMap allReports;
    QArrayOfStringMap result;

    // load all reports
    QArrayOfStringMap reports = m_currentSession->reports(index, pageSize);
    if (reports.isEmpty()) {
        return QStringMap();
    } else if (serviceName.isEmpty()) {
        return reports.value(0);
    }

    QString sessionName = QString("%1-%2-%3")
            .arg(m_account->providerName())
            .arg(serviceName)
            .arg(m_account->id());

    index += pageSize;
    while (reports.size() != pageSize) {
        Q_FOREACH(const QStringMap &report, reports) {
            if (report.value("peer") == sessionName) {
                return report;
            }
        }

        reports = m_currentSession->reports(index, pageSize);
        index += pageSize;
    }

    Q_FOREACH(const QStringMap &report, reports) {
        if (report.value("peer") == sessionName) {
            return report;
        }
    }

    return QStringMap();
}

QString SyncAccount::statusDescription(const QString &status) const
{
    if (status.isEmpty()) {
        return "";
    }

    switch(status.toInt())
    {
    case 0:
        return "STATUS_OK";
    case 200:
        return "STATUS_HTTP_OK";
    case 204:
        return "STATUS_NO_CONTENT";
    case 207:
        return "STATUS_DATA_MERGED";
    case 22001:
        return "Fail to sync some items";
    case 22002:
        return "Last process unexpected die.";
    case 22000:
        return "Fail to run \"two-way\" sync";
    case 403:
        return "Forbidden / access denied";
    case 404:
        return "Object not found / unassigned field";
    case 405:
        return "Command not allowed";
    case 406:
    case 407:
    case 420:
        return "Disk full";
    default:
        return "Unkown status";
    }
}

QString SyncAccount::syncMode(const QString &serviceName, bool *firstSync) const
{
    QString lastStatus = lastSyncStatus(serviceName);
    qDebug() << "Last sync status:" << lastStatus << statusDescription(lastStatus);
    *firstSync = lastStatus.isEmpty();
    if (lastStatus.isEmpty()) {
        return "slow";
    }
    switch(lastStatus.toInt())
    {
    case 0:                         // STATUS_OK
    case 200:                       // STATUS_HTTP_OK
    case 204:                       // STATUS_NO_CONTENT
    case 207:                       // STATUS_DATA_MERGED
        return "two-way";
    case 22001:                     // fail to sync some items
    case 22002:                     // last process unexpected die
        return "two-way";
    case 22000:                     // fail to run "two-way" sync
        return "slow";
    case 403:                       // forbidden / access denied
    case 404:                       // bject not found / unassigned field
    case 405:                       // command not allowed
    case 406:
    case 407:
    case 420:                       // disk full
        return "two-way";
    default:
        qWarning() << "last status unkown using slow sync:" << lastStatus;
        return "slow";
    }
}

QString SyncAccount::lastSyncStatus(const QString &serviceName) const
{
    QStringMap lastReport = this->lastReport(serviceName);
    return lastReport.value("status", "");
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

QStringList SyncAccount::availableServices() const
{
    return m_availabeServices.keys();
}

uint SyncAccount::lastError() const
{
    return m_lastError;
}

void SyncAccount::onAccountEnabledChanged(const QString &serviceName, bool enabled)
{
    // empty service name means that the hole account has been enabled/disabled
    if (serviceName.isEmpty()) {
        setupServices();
        Q_EMIT enableChanged(QString(), enabled);
    } else {
        // get service type
        QString serviceType = serviceName;
        Q_FOREACH(Service service, m_account->services()) {
            if (service.name() == serviceName) {
                serviceType = service.serviceType();
                break;
            }
        }

        if (m_availabeServices.contains(serviceType)) {
            m_availabeServices[serviceType] = enabled;
            Q_EMIT enableChanged(serviceType, enabled);
        }
    }
}

void SyncAccount::onSessionStatusChanged(const QString &newStatus)
{
    if (newStatus == "running") {
        switch (m_state) {
        case SyncAccount::Idle:
            setState(SyncAccount::Syncing);
            Q_EMIT syncStarted(m_syncServiceName, m_firstSync);
            break;
        case SyncAccount::Syncing:
            break;
        default:
            qWarning() << "State changed to" << newStatus << "during" << state();
            break;
        }
    } else if (newStatus == "done") {
        QString lastStatus = lastSyncStatus(m_syncServiceName);
        releaseSession();
        switch (m_state) {
        case SyncAccount::Syncing:
        {
            QString currentServiceName = m_syncServiceName;
            bool firstSync = m_firstSync;

            m_syncMode.clear();
            m_syncServiceName.clear();
            m_firstSync = false;
            setState(SyncAccount::Idle);

            Q_EMIT syncFinished(currentServiceName, firstSync, lastStatus);
            break;
        }
        default:
            qWarning() << "State changed to" << newStatus << "during" << state();
            break;
        }

    } else if (newStatus == "running;waiting") {
        // ignore
    } else {
        qWarning() << "Status changed invalid;" << newStatus;
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

// configure syncevolution with the necessary information for sync
void SyncAccount::configure(const QString &serviceName)
{
    qDebug() << "Configure account for service:"
             << m_account->displayName() << serviceName;

    setState(SyncAccount::Configuring);
    SyncConfigure *configure = new SyncConfigure(m_account, m_settings, this);
    m_pendingConfigs << configure;

    connect(configure, &SyncConfigure::done,
            this, &SyncAccount::onAccountConfigured);
    connect(configure, &SyncConfigure::error,
            this, &SyncAccount::onAccountConfigureError);

    configure->configure(serviceName);
}

void SyncAccount::onAccountConfigured()
{
    SyncConfigure *configure = qobject_cast<SyncConfigure*>(QObject::sender());
    m_pendingConfigs.removeOne(configure);
    QString serviceName = configure->serviceName();
    configure->deleteLater();

    Q_EMIT configured(serviceName);

    setState(SyncAccount::Idle);
    continueSync(serviceName);
}

void SyncAccount::onAccountConfigureError()
{
    SyncConfigure *configure = qobject_cast<SyncConfigure*>(QObject::sender());
    m_pendingConfigs.removeOne(configure);
    configure->deleteLater();
    // TODO: notify error
    setState(SyncAccount::Invalid);
    qWarning() << "Fail to configure account" << m_account->id() << m_account->displayName();
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
