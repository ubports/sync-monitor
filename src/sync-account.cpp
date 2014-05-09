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
#include "sync-i18n.h"

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
    if (m_settings) {
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
}

QString SyncAccount::sessionName(const QString &serviceName) const
{
    return QString("%1-%2-%3")
            .arg(m_account->providerName())
            .arg(serviceName)
            .arg(m_account->id());
}

QString SyncAccount::sourceName(const QString &serviceName) const
{
    return QString("%1_uoa_%2")
            .arg(serviceName)
            .arg(m_account->id());
}

void SyncAccount::dumpReport(const QStringMap &report) const
{
    Q_FOREACH(const QString &key, report.keys()) {
        qDebug() << "\t" << key << ":" << report[key];
    }
}

void SyncAccount::setup()
{
    setupServices();
    if (m_account) {
        connect(m_account,
                SIGNAL(enabledChanged(QString,bool)),
                SLOT(onAccountEnabledChanged(QString,bool)));
    }
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
        if (prepareSession(serviceName)) {
            m_syncMode = syncMode(serviceName, &m_firstSync);
            releaseSession();
            configure(serviceName, m_syncMode);
        } else {
            qWarning() << "Fail to connect with syncevolution";
        }
        break;
    default:
        break;
    }
}

bool SyncAccount::prepareSession(const QString &serviceName)
{
    m_syncServiceName = serviceName;
    QString sessionName = this->sessionName(serviceName);
    SyncEvolutionServerProxy *proxy = SyncEvolutionServerProxy::instance();
    SyncEvolutionSessionProxy *session = proxy->openSession(sessionName,
                                                            QStringList());
    if (session) {
        attachSession(session);
        return true;
    } else {
        return false;
    }
}

bool SyncAccount::syncService(const QString &serviceName)
{
    bool enabledService = m_availabeServices.value(serviceName, false);
    if (!enabledService) {
        setState(SyncAccount::Idle);
        Q_EMIT syncFinished(serviceName, false, "");
        return true;
    }

    if (prepareSession(serviceName)) {
        QStringMap syncFlags;
        syncFlags.insert(sourceName(serviceName), m_syncMode);
        m_currentSession->sync(syncFlags);
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

    QArrayOfStringMap reports = m_currentSession->reports(index, pageSize);
    if (reports.isEmpty()) {
        return QStringMap();
    } else if (serviceName.isEmpty()) {
        return reports.value(0);
    }

    QString sessionName = this->sessionName(serviceName);
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

QString SyncAccount::syncMode(const QString &serviceName, bool *firstSync) const
{
    QString lastSyncMode = "two-way";
    QString lastStatus = lastSyncStatus(serviceName, &lastSyncMode);
    *firstSync = lastStatus.isEmpty();
    if (*firstSync) {
        return "slow";
    }
    switch(lastStatus.toInt())
    {
    case 22000:
        // "Fail to run \"two-way\" sync";
        return "slow";
    case 0:
    case 200:
    case 204:
    case 207:
        // status ok
        return "two-way";
    case 401:
    case 403:
        // "Forbidden / access denied";
    case 404:
        // "Object not found / unassigned field";
    case 405:
        // "Command not allowed";
    case 406:
    case 407:
        // "Proxy authentication required";
    case 420:
        // "Disk full";
    case 506:
        // "Fail to sync due some remote problem";
    case 22001:
        // "Fail to sync some items";
    case 22002:
        // "Last process unexpected die.";
    case 20006:
    case 20007:
        // "Server sent bad content";
    case 20020:
        // "Connection timeout";
    case 20021:
        // "Connection certificate has expired";
    case 20022:
        // "Connection certificate is invalid";
    case 20026:
    case 20027:
    case 20028:
        // "Fail to connect with the server";
    case 20046:
    case 20047:
        // "Server not found";
    default:
        return lastSyncMode;
    }
}

QString SyncAccount::lastSyncStatus(const QString &serviceName, QString *lastSyncMode) const
{
    QString sourceName = this->sourceName(serviceName).replace("_", "__");
    QStringMap lastReport = this->lastReport(serviceName);
    QString lastStatus;
    if (!lastReport.isEmpty()) {
        lastStatus = lastReport.value("status", "");
        *lastSyncMode = lastReport.value(QString("source-%1-mode").arg(sourceName), "slow");
    } else {
        QString statusMessage = statusDescription(lastStatus);
        qDebug() << QString("Last report start date: %1, Status: %2 Message: %3")
                    .arg(QDateTime::fromTime_t(lastReport.value("start", "0").toUInt()).toString(Qt::SystemLocaleShortDate))
                    .arg(lastStatus)
                    .arg(statusMessage.isEmpty() ? "OK" : statusMessage);
    }

    return lastStatus;
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

QString SyncAccount::iconName(const QString &serviceName) const
{
    QString iconName;
    Q_FOREACH(const Service &service, m_account->services()) {
        if (service.serviceType() == serviceName) {
            iconName = service.iconName();
            break;
        }
    }

    // use icon name based on the current theme intead of hardcoded name
    if (iconName.isEmpty()) {
        return QString("/usr/share/icons/ubuntu-mobile/actions/scalable/reload.svg");
    } else {
        return QString("/usr/share/icons/ubuntu-mobile/apps/scalable/%1.svg").arg(iconName);
    }
}

QStringList SyncAccount::availableServices() const
{
    return m_availabeServices.keys();
}

QStringList SyncAccount::enabledServices() const
{
    QStringList result;
    Q_FOREACH(const Service &service, m_account->enabledServices()) {
        result << service.serviceType();
    }
    return result;
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
        QString lastSyncMode;
        QString lastStatus = lastSyncStatus(m_syncServiceName, &lastSyncMode);
        QStringMap lastReport = this->lastReport(m_syncServiceName);
        qDebug() << "Sync Report";
        dumpReport(lastReport);
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
void SyncAccount::configure(const QString &serviceName, const QString &syncMode)
{
    qDebug() << "Configure account for service:"
             << m_account->displayName() << serviceName << syncMode;

    setState(SyncAccount::Configuring);
    SyncConfigure *configure = new SyncConfigure(m_account, m_settings, this);
    m_pendingConfigs << configure;

    connect(configure, &SyncConfigure::done,
            this, &SyncAccount::onAccountConfigured);
    connect(configure, &SyncConfigure::error,
            this, &SyncAccount::onAccountConfigureError);

    configure->configure(serviceName, syncMode);
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

QString SyncAccount::statusDescription(const QString &status)
{
    if (status.isEmpty()) {
        return "";
    }

    switch(status.toInt())
    {
    case 0:
    case 200:
    case 204:
    case 207:
        // OK
        return "";
    case 401:
    case 403:
        return _("Forbidden / access denied");
    case 404:
        return _("Object not found / unassigned field");
    case 405:
        return _("Command not allowed");
    case 406:
    case 407:
        return _("Proxy authentication required");
    case 420:
        return _("Disk full");
    case 506:
        return _("Fail to sync due some remote problem");
    case 22000:
        return _("Fail to run \"two-way\" sync");
    case 22001:
        return _("Fail to sync some items");
    case 22002:
        return _("Process unexpected die.");
    case 20006:
    case 20007:
        return _("Server sent bad content");
    case 20020:
        return _("Connection timeout");
    case 20021:
        return _("Connection certificate has expired");
    case 20022:
        return _("Connection certificate is invalid");
    case 20026:
    case 20027:
    case 20028:
        return _("Fail to connect with the server");
    case 20046:
    case 20047:
        return _( "Server not found");
    default:
        return _("Unknown status");
    }
}
