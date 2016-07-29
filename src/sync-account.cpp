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
#include "sync-auth.h"
#include "sync-configure.h"
#include "syncevolution-server-proxy.h"
#include "syncevolution-session-proxy.h"
#include "sync-i18n.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkAccessManager>

#include "config.h"

using namespace Accounts;

#define REFRESH_FROM_REMOTE_SYNC "refresh-from-remote"

SyncAccount::SyncAccount(Account *account,
                         const QSettings *settings,
                         QObject *parent)
    : QObject(parent),
      m_config(0),
      m_currentSession(0),
      m_account(account),
      m_state(SyncAccount::Idle),
      m_settings(settings),
      m_lastError(0),
      m_retrySync(true)
{
    setup();
}

SyncAccount::SyncAccount(Account *account,
                         const QString &service,
                         const QSettings *settings,
                         QObject *parent)
    : QObject(parent),
      m_currentSession(0),
      m_account(account),
      m_state(SyncAccount::Idle),
      m_settings(settings),
      m_lastError(0),
      m_retrySync(true)
{
    m_availabeServices.insert(service, true);
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
        const QStringList supportedSevices = m_settings->childGroups();
        const ServiceList enabledServices = m_account->enabledServices();
        Q_FOREACH(Service service, m_account->services()) {
            if (supportedSevices.contains(service.serviceType())) {
                const bool enabled = m_account->enabled() && enabledServices.contains(service);
                m_availabeServices.insert(service.serviceType(), enabled);
            }
        }
        qDebug() << "Supported sevices for protocol:" << m_account->providerName() << supportedSevices;
        qDebug() << "Services available for:" << m_account->displayName() << m_availabeServices;
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

void SyncAccount::cancel(const QStringList &sources)
{
    Q_UNUSED(sources);
    qDebug() << "Sync cancel requested" << sources;

    //TODO: cancel the only the source
    if (m_currentSession) {
        m_currentSession->destroy();
        m_currentSession = 0;

        if (m_state == SyncAccount::Syncing) {
            Q_EMIT syncError("", "canceled");
        } else {
            qDebug() << "Cancelled with no sync state";
        }
        setState(SyncAccount::Idle);

    }
}

void SyncAccount::sync(const QStringList &sources)
{
    switch(m_state) {
    case SyncAccount::Idle:
        qDebug() << "Sync requested:" << m_account->displayName() << sources;
        m_sourcesToSync.clear();
        m_sourcesToSync << sources;
        m_startSyncTime = QDateTime::currentDateTime();
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

QList<SourceData> SyncAccount::sources() const
{
    QList<SourceData> sources;

    if (!m_currentSession) {
        return sources;
    }

    QStringMultiMap config = m_currentSession->getConfig("@default", false);
    Q_FOREACH(const QString &key, config.keys()) {
        if (config[key]["backend"] == CALENDAR_EDS_BACKEND) {
            const QString sourceName = key.split("/").last();
            bool writable = true;
            QString remoteId;
            Q_FOREACH(const SyncDatabase &db, m_remoteSources) {
                // build sync evolution source name based on service and account.
                QString dbSourceName = SyncConfigure::formatSourceName(m_account->id(), db.remoteId);
                if (dbSourceName == sourceName) {
                    writable = db.writable;
                    remoteId = db.remoteId;
                    break;
                }
            }
            if (!remoteId.isEmpty())
                sources << SourceData(sourceName, remoteId, writable);
        }
    }

    return sources;
}

QString SyncAccount::lastSyncStatus(const QString &sourceName) const
{
    const QString logKey = QString(ACCOUNT_LOG_GROUP_FORMAT).arg(m_account->id()).arg(sourceName);
    QSettings settings;

    return settings.value(logKey + ACCOUNT_LOG_LAST_SYNC_RESULT).toString();
}

QString SyncAccount::pickAColor()
{
    static QStringList colorNames;
    if (colorNames.isEmpty()) {
        colorNames << "#2C001E"
                   << "#333333"
                   << "#DD4814"
                   << "#DF382C"
                   << "#EFB73E"
                   << "#19B6EE"
                   << "#38B44A"
                   << "#001F5C";
        qsrand(colorNames.size());
    }

    const int index = (rand() % (colorNames.size() - 1));
    qDebug() << "Color" << index;
    return colorNames.value(index, 0);
}

void SyncAccount::continueSync()
{
    setState(SyncAccount::AboutToSync);

    QStringMap syncFlags;
    if (!prepareSession()) {
        setState(SyncAccount::Invalid);
        return;
    }

    if (!isEnabled()) {
        qDebug() << "Calendar Service disabled for account:" << m_account->id() << ". Skip sync!";
    } else {
        qDebug() << "Will prepare to sync:" << m_account->id() << m_sourcesToSync;
        Q_FOREACH(const SourceData &source, sources()) {
            if (m_sourcesToSync.isEmpty() || m_sourcesToSync.contains(source.remoteId)) {
                bool firstSync = false;
                // read-only sources aways sync with "refresh-from-remote"
                QString mode(REFRESH_FROM_REMOTE_SYNC);
                if (source.writable) {
                    mode = syncMode(source.sourceName, &firstSync);
                }
                syncFlags.insert(source.sourceName, mode);
                m_sourcesOnSync.insert(source.sourceName, SyncAccount::SourceSyncStarting);
                m_sourcesToSync.removeAll(source.remoteId);
            }
        }
        if (!m_sourcesToSync.isEmpty()) {
            qDebug() << "Source not present on remote side:" << m_sourcesToSync;
            m_sourcesToSync.clear();
        }
    }

    if (!syncFlags.isEmpty()) {
        qDebug() << "Will sync with flags" << syncFlags;
        m_syncTime.restart();
        m_currentSession->sync("none", syncFlags);
    } else {
        qDebug() << "Nothing to sync!";
        releaseSession();
        setState(SyncAccount::Idle);
        Q_EMIT syncFinished(CALENDAR_SERVICE_TYPE, QMap<QString, QString>());
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

QString SyncAccount::syncMode(const QString &sourceName,
                              bool *firstSync) const
{
    const QString lastStatus = lastSyncStatus(sourceName);
    *firstSync = lastStatus.isEmpty();
    qDebug() << "\tAccount" << m_account->displayName()
             << "source" << sourceName
             << "Last status" << lastStatus
             << "Is first sync" << *firstSync;

    if (*firstSync) {
        return REFRESH_FROM_REMOTE_SYNC;
    }
    switch(lastStatus.toInt())
    {
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
    case 514:
        // "Remote problem, not way to recovery";
        return "refresh-from-remote";
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
    case 22000:
        // "Fail to run \"two-way\" sync";
    default:
        return "slow";
    }
}

bool SyncAccount::isEnabled() const
{
    return enabledServices().contains(CALENDAR_SERVICE_TYPE);
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
    if (!m_account->enabled())
        return result;

    Q_FOREACH(const Service &service, m_account->enabledServices()) {
        result << service.serviceType();
    }
    return result;
}

uint SyncAccount::lastError() const
{
    return m_lastError;
}

void SyncAccount::removeOldConfig() const
{
    QString configPath;

    // remove source
    configPath = QString("%1/%2-%3-%4")
            .arg(QStandardPaths::locate(QStandardPaths::ConfigLocation,
                                        QStringLiteral("syncevolution"),
                                        QStandardPaths::LocateDirectory))
            .arg(m_account->providerName())
            .arg(CALENDAR_SERVICE_TYPE)
            .arg(m_account->id());
    QDir configDir(configPath);
    if (configDir.exists()) {
        if (configDir.removeRecursively()) {
            qDebug() << "\tConfig dir removed" << configPath;
        } else {
            qWarning() << "\tFail to remove config dir" << configPath;
        }
    } else {
        qDebug() << "\tOld config dir not found" << configDir.absolutePath();
    }

    // remove 'default/source/<service>_uoa_<account-id>
    configPath = QString("%1/default/sources/%2_uoa_%3")
            .arg(QStandardPaths::locate(QStandardPaths::ConfigLocation,
                                        QStringLiteral("syncevolution"),
                                        QStandardPaths::LocateDirectory))
            .arg(CALENDAR_SERVICE_TYPE)
            .arg(m_account->id());
    configDir = QDir(configPath);
    if (configDir.exists()) {
        if (configDir.removeRecursively()) {
            qDebug() << "\tsource dir removed" << configPath;
        } else {
            qWarning() << "\tFail to remove source dir" << configPath;
        }
    } else {
        qDebug() << "\tOld config dir not found" << configDir.absolutePath();
    }

    // remove 'default/peers/<provider>-<service>-<account-id>
    configPath = QString("%1/default/peers/%2-%3-%4")
            .arg(QStandardPaths::locate(QStandardPaths::ConfigLocation,
                                        QStringLiteral("syncevolution"),
                                        QStandardPaths::LocateDirectory))
            .arg(m_account->providerName())
            .arg(CALENDAR_SERVICE_TYPE)
            .arg(m_account->id());
    configDir = QDir(configPath);
    if (configDir.exists()) {
        if (configDir.removeRecursively()) {
            qDebug() << "\tpeer dir removed" << configPath;
        } else {
            qWarning() << "\tFail to remove peer dir" << configPath;
        }
    } else {
        qDebug() << "\tOld config dir not found" << configDir.absolutePath();
    }
}

void SyncAccount::removeConfig()
{
    //TODO
    QString configPath;
    Q_FOREACH(const QString &service, m_availabeServices.keys()) {
        configPath = QString("%1/%2-%3")
                .arg(QStandardPaths::locate(QStandardPaths::ConfigLocation,
                                            QStringLiteral("syncevolution"),
                                            QStandardPaths::LocateDirectory))
                .arg(m_account->providerName())
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

bool SyncAccount::retrySync() const
{
    return m_retrySync;
}

void SyncAccount::setRetrySync(bool retry)
{
    m_retrySync = retry;
}

Account *SyncAccount::account() const
{
    return m_account;
}

QDateTime SyncAccount::lastSyncTime() const
{
    return m_startSyncTime;
}

QString SyncAccount::host() const
{
    // Append "?SyncEvolution=Google" to tell syncevolution to enable all hacks necessary to work with google
    static const QString googleSyncUrl = "https://apidata.googleusercontent.com/caldav/v2?SyncEvolution=Google";

    if (m_account) {
        QString myHost = m_account->value("host", "").toString();
        if (myHost.isEmpty() && (providerName() == GOOGLE_PROVIDER_NAME)) {
            return googleSyncUrl;
        }
        return myHost;
    }
    return QString();
}

QString SyncAccount::providerName() const
{
    if (m_account) {
        return m_account->providerName();
    }
    return QString();
}

QString SyncAccount::calendarServiceName() const
{
    Q_FOREACH(Service service, m_account->services()) {
        if (service.serviceType() == CALENDAR_SERVICE_TYPE) {
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
    if (status != "done") {
        switch (m_state) {
        case SyncAccount::AboutToSync:
            setState(SyncAccount::Syncing);
            Q_EMIT syncStarted();
            break;
        case SyncAccount::Syncing:
            break;
        default:
            qWarning() << "State changed to" << status << "during" << state();
            break;
        }
    }

    for(QSyncStatusMap::const_iterator i = sources.begin();
        i != sources.end();
        i++) {
        const QString newStatus = i.value().status;
        const QString sourceName = i.key();

        if (newStatus == "idle") {
            // skip idle sources
            continue;
        }

        qDebug() << "\tSource:" << sourceName
                 << "Error:" << i.value().error
                 << "Status" << i.value().status
                 << "Mode" << i.value().mode;

        const bool isFirstSync = (i.value().mode == REFRESH_FROM_REMOTE_SYNC);
        if (newStatus == "running") {
            if (m_sourcesOnSync.value(sourceName) == SyncAccount::SourceSyncStarting) {
                m_sourcesOnSync[sourceName] = SyncAccount::SourceSyncRunning;
                Q_EMIT syncSourceStarted(CALENDAR_SERVICE_TYPE, newStatus, isFirstSync);
            }

        } else if (newStatus == "done") {
            if (m_sourcesOnSync.value(sourceName) == SyncAccount::SourceSyncRunning) {
                m_sourcesOnSync[sourceName] = SyncAccount::SourceSyncDone;
                m_currentSyncResults.insert(sourceName, QString::number(i.value().error));
                Q_EMIT syncSourceFinished(CALENDAR_SERVICE_TYPE, sourceName, isFirstSync, newStatus, "");
            }
        } else if ((status == "running;waiting") ||
                   (status == "idle")) {
            // ignore
        } else {
            qWarning() << "Status changed invalid;" << newStatus;
        }
    }

    if (status == "done") {
        bool done = true;
        Q_FOREACH(const QString &source, m_sourcesOnSync.keys()) {
            if (m_sourcesOnSync[source] != SyncAccount::SourceSyncDone) {
                done = false;
                qWarning() << "Sync status changed to done. But source still syncing" << source;
            }
        }

        if (error != 0) {
            QString errorMessage = statusDescription(QString::number(error));
            qWarning() << "Sync Error" << error << errorMessage;
            Q_EMIT syncError(CALENDAR_SERVICE_TYPE, errorMessage);
            m_currentSyncResults.insert("", QString::number(error));
            // fail to sync, notify sync finished
            done = true;
        }

        if (done) {
            m_sourcesOnSync.clear();
            m_sourcesToSync.clear();
            setState(SyncAccount::Idle);
            releaseSession();

            Q_EMIT syncFinished(CALENDAR_SERVICE_TYPE, m_currentSyncResults);
            m_currentSyncResults.clear();
            qDebug() << "---------------------------------------------------------Sync finished:" << m_syncTime.elapsed() / 1000 << "secs";
        }
    }
}

void SyncAccount::onSessionProgressChanged(int progress)
{
    qDebug() << "Progress" << progress << "elapsed:" << m_syncTime.elapsed() / 1000 << "secs";
}

// configure syncevolution with the necessary information for sync
void SyncAccount::configure()
{
    qDebug() << "Configure account:" << m_account->displayName() << "[" << m_sourcesToSync << "]";

    if (m_config) {
        qWarning() << "Account already configured" << m_account->displayName();
        continueSync();
        return;
    }

    setState(SyncAccount::Configuring);
    m_config = new SyncConfigure(this,
                                 m_settings,
                                 this);
    connect(m_config, &SyncConfigure::sourceRemoved,
            this, &SyncAccount::sourceRemoved);
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

void SyncAccount::onAccountConfigureError(int error)
{
    m_config->deleteLater();
    m_config = 0;

    qWarning() << "Fail to configure account" << m_account->displayName() << error;
    setState(SyncAccount::Idle);
    Q_EMIT syncError("", QString::number(error));

    // Send sync finish due the config error there is nothing to do
    QMap<QString, QString> errorMap;
    errorMap.insert("", QString::number(error));
    Q_EMIT syncFinished("", errorMap);
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
    case -1:
        return _("Fail to configure account");
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
    case 20017:
        return _("Sync canceled");
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


void SyncAccount::fetchRemoteSources(const QString &serviceName)
{
    m_remoteSources.clear();

    SyncAuth *auth = new SyncAuth(m_account->id(), serviceName, this);
    connect(auth, SIGNAL(success()), SLOT(onAuthSucess()));
    connect(auth, SIGNAL(fail()), SLOT(onAuthFailed()));
    if (!auth->authenticate()) {
        auth->deleteLater();
        qWarning() << "fail to authenticate account!";
        Q_EMIT remoteSourcesAvailable(m_remoteSources, 304);
    }
}

void SyncAccount::onAuthSucess()
{
    SyncAuth *auth = qobject_cast<SyncAuth*>(QObject::sender());
    Q_ASSERT(auth);

    if (providerName() == GOOGLE_PROVIDER_NAME) {
        QNetworkAccessManager *manager = new QNetworkAccessManager(this);
        connect(manager, SIGNAL(finished(QNetworkReply*)),
                this, SLOT(onReplyFinished(QNetworkReply*)));

        QNetworkRequest req;
        req.setUrl(QUrl("https://www.googleapis.com/calendar/v3/users/me/calendarList"));
        req.setRawHeader(QByteArray("GData-Version"), QByteArray("3.0"));
        req.setRawHeader(QByteArray("Authorization"), QByteArray("Bearer " + auth->token().toUtf8()));

        manager->get(req);
    } else {
        const QString username = QString("uoa:%1,%2").arg(id()).arg(calendarServiceName());
        fetchRemoteCalendarsFromCommand(username, "");
    }
    auth->deleteLater();
}

void SyncAccount::onAuthFailed()
{
    SyncAuth *auth = qobject_cast<SyncAuth*>(QObject::sender());
    Q_ASSERT(auth);
    auth->deleteLater();

    qWarning() << "Fail to authenticate";
    Q_EMIT remoteSourcesAvailable(m_remoteSources, 403);
}

void SyncAccount::onReplyFinished(QNetworkReply *reply)
{
    static QStringList writableRoles;
    static const QString calendarSyncUrl("https://apidata.googleusercontent.com:443/caldav/v2/%u/events/?SyncEvolution=Google");
    QNetworkAccessManager *manager = qobject_cast<QNetworkAccessManager*>(QObject::sender());
    manager->deleteLater();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Fail to fetch remote sources:" << reply->errorString();
        Q_EMIT remoteSourcesAvailable(m_remoteSources, reply->error() == QNetworkReply::AuthenticationRequiredError ? 403 : 20007);
        return;
    }

    int responseCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (responseCode != 200) {
        qWarning() << "Fail to fetch remote sources response:" << responseCode;
        Q_EMIT remoteSourcesAvailable(m_remoteSources, 20007);
        return;
    }

    if (writableRoles.isEmpty()) {
        writableRoles << "writer"
                      << "owner";
    }

    QByteArray data = reply->readAll();

    // parse result
    QJsonParseError jError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &jError);
    if (jError.error == QJsonParseError::NoError) {
        QJsonObject body = doc.object();
        if (body.value("kind").toString() == "calendar#calendarList") {
            QJsonArray items = body.value("items").toArray();
            Q_FOREACH(const QJsonValue &i, items) {
                QJsonObject calendar = i.toObject();
                if (calendar.value("kind").toString() == "calendar#calendarListEntry") {
                    qDebug() << "Found db:"
                             << "\n\tSummary:" << calendar.value("summary").toString()
                             << "\n\tID:" << calendar.value("id").toString()
                             << "\n\tSelected:" << calendar.value("selected").toBool()
                             << "\n\tAccessRole:" << calendar.value("accessRole").toString();

                    if (calendar.value("selected").toBool()) {
                        SyncDatabase db;

                        db.name = calendar.value("summary").toString();
                        db.remoteId = calendar.value("id").toString();
                        db.source = QString(calendarSyncUrl).replace("%u", QUrl::toPercentEncoding(db.remoteId));
                        db.writable =  writableRoles.contains(calendar.value("accessRole").toString());
                        db.defaultCalendar = calendar.value("primary").toBool();
                        db.title = calendar.value("summaryOverride").toString();
                        db.color = calendar.value("backgroundColor").toString();
                        m_remoteSources << db;
                    }
                }
            }
        }
    }

    Q_EMIT remoteSourcesAvailable(m_remoteSources, 0);
}


void SyncAccount::fetchRemoteCalendarsFromCommand(const QString &username, const QString &password) const
{
    // syncevolution --print-databases backend=caldav
    QStringList args;
    QString syncUrl(host());

    // Use well-known url that will re-direct to the correct path
    if (providerName().toLower() == "owncloud") {
        syncUrl += QStringLiteral("/remote.php/dav/");
    }

    args << "--print-databases"
         << "backend=caldav"
         << QString("username=%1").arg(username)
         << QString("password=%1").arg(password)
         << QString("syncURL=%1").arg(host());
    QProcess *syncEvo = new QProcess;
    syncEvo->setProcessChannelMode(QProcess::MergedChannels);
    syncEvo->start("syncevolution", args);
    connect(syncEvo, SIGNAL(finished(int,QProcess::ExitStatus)),
            SLOT(fetchRemoteCalendarsProcessDone(int,QProcess::ExitStatus)));
    qDebug() << "Fetching remote calendars (wait...)";
}

void SyncAccount::fetchRemoteCalendarsProcessDone(int exitCode, QProcess::ExitStatus exitStatus)
{
    QProcess *syncEvo = qobject_cast<QProcess*>(QObject::sender());

    if (exitStatus == QProcess::NormalExit) {
        QString output = syncEvo->readAll();
        QStringList lines = output.split("\n");
        while (lines.count() > 0) {
            if (lines.first().startsWith("caldav:")) {
                lines.takeFirst();
                break;
            }
            lines.takeFirst();
        }

        while (lines.count() > 0) {
            QString line = lines.takeFirst();
            if (line.isEmpty()) {
                continue;
            }

            QStringList fields = line.split("(");
            if (fields.count() == 2) {
                SyncDatabase db;

                db.name = fields.first().trimmed();

                const QString syncUrl = fields.at(1).split(")").first();
                db.source = syncUrl;
                db.remoteId = QUrl::fromPercentEncoding(syncUrl.split("/", QString::SkipEmptyParts).last().toLatin1());
                db.defaultCalendar =fields.at(1).trimmed().endsWith("<default>");
                //TODO: get calendar permissions
                db.writable = true;
                //TODO: get calendar color
                db.color = pickAColor();

                m_remoteSources << db;

                qDebug() << "DB" << db.name
                         << "\n\tId:" << db.remoteId
                         << "\n\tSource" << db.source
                         << "\n\tFlag" << db.writable;

            } else {
                qWarning() << "Fail to parse db output" << line;
            }
        }

        Q_EMIT remoteSourcesAvailable(m_remoteSources, 0);
    } else {
        Q_EMIT remoteSourcesAvailable(m_remoteSources, 20007);
    }
}
