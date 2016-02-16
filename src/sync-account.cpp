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
      m_config(0),
      m_currentSession(0),
      m_account(account),
      m_state(SyncAccount::Idle),
      m_settings(settings),
      m_lastError(0)
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
        setState(SyncAccount::Idle);
    }
}

void SyncAccount::sync(const QString &serviceName)
{
    switch(m_state) {
    case SyncAccount::Idle:
        qDebug() << "Sync requested service:" << m_account->displayName() << serviceName;
        if (serviceName.isEmpty()) {
            m_servicesToSync  = m_settings->childGroups();
        } else {
            m_servicesToSync << serviceName;
        }
        configure();
        break;
    default:
        qWarning() << "Sync request with account in an invalid state" << m_state;
        break;
    }
}

bool SyncAccount::prepareSession(const QString &session)
{
    QString sessionName(session);
    if (session == QString::null) {
        sessionName = SyncConfigure::accountSessionName(m_account);
    }

    SyncEvolutionServerProxy *proxy = SyncEvolutionServerProxy::instance();
    SyncEvolutionSessionProxy *sessionProxy = proxy->openSession(sessionName,
                                                            QStringList());
    if (sessionProxy) {
        attachSession(sessionProxy);
        return true;
    } else {
        qWarning() << "Fail to open session" << sessionName;
        return false;
    }
}

QStringList SyncAccount::sources(const QString &serviceName) const
{
    if (!m_currentSession) {
        return QStringList();
    }

    QStringList sources;
    QStringMultiMap config = m_currentSession->getConfig("@default", false);
    QString sourcePrefix = QString("source/%1_").arg(serviceName);
    Q_FOREACH(const QString &key, config.keys()) {
        if (key.startsWith(sourcePrefix)) {
            sources << key.split("/").last();
        }
    }

    return sources;
}

void SyncAccount::continueSync()
{
    setState(SyncAccount::AboutToSync);

    QStringMap syncFlags;
    if (!prepareSession()) {
        setState(SyncAccount::Invalid);
        return;
    }

    Q_FOREACH(const QString &service, m_servicesToSync) {
        bool enabledService = m_availabeServices.value(service, false);
        if (!enabledService) {
            qDebug() << "Service" << service << "disabled. Skip sync.";
            Q_EMIT syncFinished(service, "", false, "", "");
        } else {
            qDebug() << "Will prepare to sync" << service;
            Q_FOREACH(const QString &source, sources(service)) {
                bool firstSync = false;
                QString mode = syncMode(service, source, &firstSync);
                syncFlags.insert(source, mode);
                qDebug() << "Source sync" << source << mode;
                m_sourcesOnSync << source;
            }
        }
    }

    if (!syncFlags.isEmpty()) {
        qDebug() << "Will sync with flags" << syncFlags;
        m_currentSession->sync("none", syncFlags);
    } else {
        setState(SyncAccount::Idle);
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
    if (!m_currentSession) {
        qDebug() << "Session cancelled";
        return QStringMap();
    }

    QArrayOfStringMap reports = m_currentSession->reports(index, pageSize);
//    qDebug() << "REPORT====================================================";
//    Q_FOREACH(const QStringMap &map, reports) {
//        SyncConfigure::dumpMap(map);
//    }
//    qDebug() << "==========================================================";
    if (reports.isEmpty()) {
        return QStringMap();
    } else if (serviceName.isEmpty()) {
        return reports.value(0);
    }

    QString sessionName = SyncConfigure::accountSessionName(m_account);
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

QString SyncAccount::syncMode(const QString &serviceName,
                              const QString &sourceName,
                              bool *firstSync) const
{
    QString lastSyncMode = "two-way";
    QString lastStatus = lastSyncStatus(serviceName, sourceName);
    *firstSync = lastStatus.isEmpty();
    qDebug() << "Service" << serviceName
             << "source" << sourceName
             << "Last status" << lastStatus
             << "Is first sync" << *firstSync;

    if (*firstSync) {
        return "refresh-from-remote";
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

QString SyncAccount::lastSyncStatus(const QString &serviceName,
                                    const QString &sourceName) const
{
    QStringMap lastReport = this->lastReport(serviceName);
    QString lastStatus;
    if (!lastReport.isEmpty()) {
        lastStatus = lastReport.value("status", "");
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


void SyncAccount::removeConfig()
{
    //TODO
    QString configPath;
    Q_FOREACH(const QString &service, m_availabeServices.keys()) {
        configPath = QString("%1/%2-%3-%4")
                .arg(QStandardPaths::locate(QStandardPaths::ConfigLocation,
                                            QStringLiteral("syncevolution"),
                                            QStandardPaths::LocateDirectory))
                .arg(m_account->providerName())
                .arg(service)
                .arg(m_account->id());
        QDir configDir(configPath);
        if (configDir.exists()) {
            if (configDir.removeRecursively()) {
                qDebug() << "Config dir removed" << configPath;
            } else {
                qWarning() << "Fail to remove config dir" << configPath;
            }
        }
    }
}

void SyncAccount::setLastError(uint errorCode)
{
    m_lastError = errorCode;
}

QString SyncAccount::serviceId(const QString &serviceName) const
{
    Q_FOREACH(Service service, m_account->services()) {
        qDebug() << service.serviceType() << service.name();
        if (service.serviceType() == serviceName) {
            return service.name();
        }
    }
    return QString();
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

void SyncAccount::onSessionStatusChanged(const QString &status, quint32 error, const QSyncStatusMap &sources)
{
    qDebug() << "onSessionStatusChanged" << status << error << "Sources" << sources.size();

    for(QSyncStatusMap::const_iterator i = sources.begin();
        i != sources.end();
        i++) {
        QString newStatus = i.value().status;
        QString sourceName = i.key();
        QString serviceName = sourceName.split("_").first();
        QString syncMode = i.value().mode;

        qDebug() << "\t" << sourceName
                 << "Error:" << i.value().error
                 << "Status" << i.value().status
                 << "Mode" << i.value().mode;

        if (newStatus == "running") {
            //TODO: check m_firstSync
            Q_EMIT syncStarted(serviceName, sourceName, m_firstSync);

            switch (m_state) {
            case SyncAccount::Idle:
                setState(SyncAccount::Syncing);
                break;
            case SyncAccount::Syncing:
                break;
            default:
                qWarning() << "State changed to" << newStatus << "during" << state();
                break;
            }
        } else if (newStatus == "done") {
            //TODO: check m_firstSync
            Q_EMIT syncFinished(serviceName, sourceName, m_firstSync, "", syncMode);
            m_sourcesOnSync.removeOne(sourceName);
            if (m_sourcesOnSync.isEmpty()) {
                qDebug() << "Sync finished";
                setState(SyncAccount::Idle);
            }
        } else if ((newStatus == "running;waiting") ||
                   (newStatus == "idle")) {
            // ignore
        } else {
            qWarning() << "Status changed invalid;" << newStatus;
        }
    }
}

void SyncAccount::onSessionProgressChanged(int progress)
{
    qDebug() << "Progress" << progress;
}

// configure syncevolution with the necessary information for sync
void SyncAccount::configure()
{
    qDebug() << "Configure account:" << m_account->displayName() << m_servicesToSync;

    if (m_config) {
        qWarning() << "Account already configured" << m_account->displayName();
        continueSync();
        return;
    }

    setState(SyncAccount::Configuring);
    m_config = new SyncConfigure(m_account,
                                 m_settings,
                                 this);
    connect(m_config, &SyncConfigure::done,
            this, &SyncAccount::onAccountConfigured);
    connect(m_config, &SyncConfigure::error,
            this, &SyncAccount::onAccountConfigureError);

    m_config->configure();
}

void SyncAccount::onAccountConfigured(const QStringList &services)
{
    m_config->deleteLater();
    m_config = 0;

    Q_EMIT configured(services);

    continueSync();
}

void SyncAccount::onAccountConfigureError(const QStringList &services)
{
    m_config->deleteLater();
    m_config = 0;

    // TODO: notify error
    setState(SyncAccount::Invalid);
    qWarning() << "Fail to configure account" << m_account->displayName() << services;
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
                                    SIGNAL(statusChanged(QString,uint,QSyncStatusMap)),
                                    SLOT(onSessionStatusChanged(QString, quint32, QSyncStatusMap)));
    m_sessionConnections << connect(m_currentSession,
                                    SIGNAL(progressChanged(int)),
                                    SLOT(onSessionProgressChanged(int)));
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
