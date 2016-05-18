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

#include "sync-configure.h"
#include "syncevolution-server-proxy.h"
#include "syncevolution-session-proxy.h"
#include "eds-helper.h"
#include "dbustypes.h"

#include "config.h"

using namespace Accounts;

SyncConfigure::SyncConfigure(Account *account,
                             QSettings *settings,
                             QObject *parent)
    : QObject(parent),
      m_account(account),
      m_settings(settings)
{
}

SyncConfigure::~SyncConfigure()
{
}

AccountId SyncConfigure::accountId() const
{
    return m_account->id();
}

QString SyncConfigure::accountSessionName(Account *account)
{
    return QString("%1-%2")
            .arg(account->providerName())
            .arg(account->id());
}

void SyncConfigure::configure()
{
    fetchRemoteCalendars();
}

void SyncConfigure::fetchRemoteCalendars()
{
    fetchRemoteCalendarsFromCommand();
    return;

    SyncEvolutionServerProxy *proxy = SyncEvolutionServerProxy::instance();

    QString peerName("gcal_tmp");
    SyncEvolutionSessionProxy *session = proxy->openSession(peerName, QStringList());

    if (session->status() != "queueing") {
        fetchRemoteCalendarsFromSession(session);
    } else {
        connect(session, &SyncEvolutionSessionProxy::statusChanged,
                [this, session](const QString &status, uint errorNuber, QSyncStatusMap source) {
            if (errorNuber != 0) {
                qWarning() << "Fail to configure peer" << errorNuber;
                session->destroy();
                Q_EMIT error(QStringList(CALENDAR_SERVICE_NAME));
            } else if (status != "queueing") {
                fetchRemoteCalendarsFromSession(session);
            }
        });
    }
}

void SyncConfigure::fetchRemoteCalendarsFromSession(SyncEvolutionSessionProxy *session)
{
    QStringMultiMap config = session->getConfig("Google", true);
    config[""]["username"] = QString("uoa:%1,google-caldav").arg(m_account->id());
    config[""]["password"] = QString();
    config["source/calendar"]["backend"] = QString("caldav");
    if (session->saveConfig(QString(), config, true)) {
        connect(session, &SyncEvolutionSessionProxy::databasesReceived,
                this, &SyncConfigure::fetchRemoteCalendarsSessionDone);
        session->getDatabases(CALENDAR_SERVICE_NAME);
        qDebug() << "Fetching remote calendars (wait...)";
    } else {
        session->destroy();
        fetchRemoteCalendarsSessionDone(QArrayOfDatabases());
    }
}

void SyncConfigure::fetchRemoteCalendarsFromCommand()
{
    // syncevolution --print-databases backend=caldav
    QStringList args;
    args << "--print-databases"
         << "backend=caldav"
         << QString("username=uoa:%1,google-caldav").arg(m_account->id())
         << "syncURL=https://apidata.googleusercontent.com/caldav/v2";
    QProcess *syncEvo = new QProcess;
    syncEvo->setProcessChannelMode(QProcess::MergedChannels);
    syncEvo->start("syncevolution", args);
    connect(syncEvo, SIGNAL(finished(int,QProcess::ExitStatus)),
            SLOT(fetchRemoteCalendarsProcessDone(int,QProcess::ExitStatus)));
    qDebug() << "Fetching remote calendars (wait...)";
}

void SyncConfigure::fetchRemoteCalendarsProcessDone(int exitCode, QProcess::ExitStatus exitStatus)
{
    QProcess *syncEvo = qobject_cast<QProcess*>(QObject::sender());
    QArrayOfDatabases databases;

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

            SyncDatabase db;
            QStringList fields = line.split("(");
            if (fields.count() == 2) {
                db.name = fields.first().trimmed();
                db.source = fields.at(1).split(")").first();
                db.flag =fields.at(1).trimmed().endsWith("<default>");
            } else {
                qWarning() << "Fail to parse db output" << line;
            }

            qDebug() << "DB" << db.name << "source" << db.source << "flag" << db.flag;
            databases << db;
        }
    }

    if (!databases.isEmpty()) {
        m_remoteDatabasesByService.insert(CALENDAR_SERVICE_NAME, databases);
        configurePeer(QStringList() << CALENDAR_SERVICE_NAME);
    } else {
        error(QStringList() << CALENDAR_SERVICE_NAME);
    }
}

void SyncConfigure::fetchRemoteCalendarsSessionDone(const QArrayOfDatabases &databases)
{
    SyncEvolutionSessionProxy *session = qobject_cast<SyncEvolutionSessionProxy*>(QObject::sender());
    if (session) {
        session->destroy();
    }

    qDebug() << "Remote calendars received:";
    Q_FOREACH(const SyncDatabase &db, databases) {
        qDebug() << "Name" << db.name << "Source" << db.source << "flag" << db.flag;
    }

    m_remoteDatabasesByService.insert(CALENDAR_SERVICE_NAME, databases);
    configurePeer(QStringList() << CALENDAR_SERVICE_NAME);
}

void SyncConfigure::configurePeer(const QStringList &services)
{
    SyncEvolutionServerProxy *proxy = SyncEvolutionServerProxy::instance();
    QString peerName = accountSessionName(m_account);
    SyncEvolutionSessionProxy *session = proxy->openSession(peerName,
                                                            QStringList() << "all-configs");

    if (session->status() != "queueing") {
        continuePeerConfig(session, services);
    } else {
        connect(session, &SyncEvolutionSessionProxy::statusChanged,
                [this, session, services](const QString &status, uint errorNuber, QSyncStatusMap source) {
            if (errorNuber != 0) {
                qWarning() << "Fail to configure peer" << errorNuber;
                session->destroy();
                Q_EMIT error(services);
            } else if (status != "queueing") {
                continuePeerConfig(session, services);
            }
        });
    }
}

void SyncConfigure::continuePeerConfig(SyncEvolutionSessionProxy *session, const QStringList &services)
{
    //TODO: should we disconnect statusChanged ???
    SyncEvolutionServerProxy *proxy = SyncEvolutionServerProxy::instance();
    QStringList configs = proxy->configs();

    QString peerName = accountSessionName(m_account);
    QString peerConfigName = QString("target-config@%1").arg(peerName);

    // config peer
    QStringMultiMap config;
    if (configs.contains(peerConfigName)) {
        config = session->getConfig(peerConfigName, false);
    } else {
        m_settings->beginGroup(GLOBAL_CONFIG_GROUP);
        QString templateName = m_settings->value("template", "Google").toString();
        qDebug() << "Create New config with template" << templateName;
        config = session->getConfig(templateName, true);
        //FIXME: use hardcoded calendar service, we only support calendar for now
        config[""]["username"] = QString("uoa:%1,google-caldav").arg(m_account->id());
        config[""]["password"] = QString();
        config[""]["consumerReady"] = "0";
        //config[""]["dumpData"] = "0";
        //config[""]["printChanges"] = "0";
        config[""]["maxlogdirs"] = "2";
    }

    static QMap<QString, QString> templates;
    if (templates.isEmpty()) {
        //templates.insert(CONTACTS_SERVICE_NAME, QString("source/addressbook"));
        templates.insert(CALENDAR_SERVICE_NAME, QString("source/calendar"));
    }

    EdsHelper eds;

    bool changed = false;
    QMap<QString, QString> sourceToDatabase;
    QStringList sourcesRemoved;

    Q_FOREACH(const QString &service, services.toSet()) {
        qDebug() << "Configure source for service" << service;
        QString templateSource = templates.value(service, "");
        if (templateSource.isEmpty()) {
            qWarning() << "Fail to find template source. Skip service" << service;
            continue;
        }

        // create new sources if necessary
        QStringMap configTemplate = config.value(templateSource);
        if (configTemplate.isEmpty()) {
            qWarning() << "Template not found" << templateSource;
            continue;
        }

        // check for new database
        QArrayOfDatabases dbs = m_remoteDatabasesByService.value(service);
        if (dbs.isEmpty()) {
            qWarning() << "Fail to get remote databases";
            continue;
        }

        Q_FOREACH(const SyncDatabase &db, dbs) {
            if (db.name.isEmpty()) {
                continue;
            }
            // local dabase
            QString localDbId = eds.createSource(service, db.name, m_account->id()).split("::").last();
            qDebug() << "Create evolution source:" << localDbId;

            // remote database
            QString sourceName = QString("%1_%2").arg(service).arg(formatSourceName(db.name));
            QString fullSourceName = QString("source/%1").arg(sourceName);
            qDebug() << "Create syncevolution source" << fullSourceName;
            if (config.contains(fullSourceName)) {
                qDebug() << "Source already configured" << sourceName << fullSourceName;
            } else {
                changed = true;
                qDebug() << "Config source" << fullSourceName << sourceName << "for database" << db.name << db.source;
                QStringMap sourceConfig(configTemplate);
                sourceConfig["database"] = db.source;
                config[fullSourceName] = sourceConfig;
            }

            sourceToDatabase.insert(fullSourceName, localDbId);
        }

        // remove old sources if necessary
        QString sourcePrefix = QString("source/%1_").arg(service);
        Q_FOREACH(const QString &key, config.keys()) {
            if (key.startsWith(sourcePrefix)) {
                if (!sourceToDatabase.contains(key)) {
                    QString localDbName = config[key].value("database");
                    if (!localDbName.isEmpty()) {
                        eds.removeSource(CALENDAR_SERVICE_NAME, localDbName, -1);
                    }
                    qDebug() << "Removing old source" << key << localDbName;
                    sourcesRemoved << key;
                    config[key] = QStringMap();
                    changed = true;
                }
            }
        }

        // TODO: remove old local database
    }

    if (changed) {
        bool result = session->saveConfig(peerConfigName, config);
        if (!result) {
            qWarning() << "Fail to save account client config";
            Q_EMIT error(services);
        } else {
            qDebug() << "Peer created" << peerName;
        }
    }

    session->destroy();

    if (!changed) {
        qDebug() << "Sources config did not change. No confign needed";
        Q_EMIT done(services);
        return;
    }

    // local session
    session = proxy->openSession("", QStringList() << "all-configs");
    if (session->status() == "queueing") {
        qWarning() << "Fail to open local session";
        session->destroy();
        return;
    }

    // create local sources
    changed = sourcesRemoved.count() > 1;
    config = session->getConfig("@default", false);
    for(QMap<QString, QString>::Iterator i = sourceToDatabase.begin();
        i != sourceToDatabase.end(); i++) {
        if (!config.contains(i.key())) {
            changed = true;
            // extract service name from source name "source/<service>_<database>
            QString service = i.key().split("/").last().split("_").first();
            config[i.key()].insert("backend", QString("evolution-%1").arg(service));
            config[i.key()].insert("database", i.value());
        }
    }

    // remove local sources if necessary
    Q_FOREACH(const QString &key, sourcesRemoved) {
        qDebug() << "Reset local config" << key;
        config.remove(key);
    }

    if (changed && !session->saveConfig("@default", config)) {
        qWarning() << "Fail to save @default config";
    }

    // create sync config
    if (!session->hasConfig(peerName)) {
        qDebug() << "Create peer config on default config" << peerName;
        config = session->getConfig("SyncEvolution_Client", true);
        config[""]["syncURL"] = QString("local://@%1").arg(peerName);
        config[""]["username"] = QString();
        config[""]["password"] = QString();
        config[""]["loglevel"] = "4";
        //config[""]["dumpData"] = "0";
        //config[""]["printChanges"] = "0";
        config[""]["maxlogdirs"] = "2";
        if (!session->saveConfig(peerName, config)) {
            qWarning() << "Fail to save sync config" << peerName;
        }
    }

    session->destroy();
    SyncEvolutionServerProxy::destroy();

    // remove sources dir when necessary
    Q_FOREACH(const QString &key, sourcesRemoved) {
        QString sourceName = key.mid(key.indexOf('/') + 1);
        removeAccountSourceConfig(m_account, sourceName);
    }

    Q_EMIT done(services);
}

QString SyncConfigure::formatSourceName(const QString &name)
{
    QString sourceName;
    for(int i=0; i < name.length(); i++) {
        if (name.at(i).isLetterOrNumber()) {
            sourceName += name.at(i);
        }
    }
    return sourceName.toLower();
}

void SyncConfigure::removeAccountSourceConfig(Account *account, const QString &sourceName)
{
    QString configPath = QString("%1/default/sources/%2")
            .arg(QStandardPaths::locate(QStandardPaths::ConfigLocation,
                                        QStringLiteral("syncevolution"),
                                        QStandardPaths::LocateDirectory))
            .arg(sourceName);
    removeConfigDir(configPath);

    configPath = QString("%1/default/peers/%2-%3/sources/%4")
            .arg(QStandardPaths::locate(QStandardPaths::ConfigLocation,
                                        QStringLiteral("syncevolution"),
                                        QStandardPaths::LocateDirectory))
            .arg(account->providerName())
            .arg(account->id())
            .arg(sourceName);
    removeConfigDir(configPath);
}

bool SyncConfigure::removeConfigDir(const QString &dirPath)
{
    QDir dir(dirPath);
    if (dir.exists()) {
        if (dir.removeRecursively()) {
            qDebug() << "Config dir removed" << dir.absolutePath();
        } else {
            qWarning() << "Fail to remove config dir" << dir.absolutePath();
        }
    } else {
        qDebug() << "Remove source config dir not found" << dir.absolutePath();
    }
}

void SyncConfigure::dumpMap(const QStringMap &map)
{
    QMapIterator<QString, QString> i(map);
    while (i.hasNext()) {
        i.next();
        qDebug() << i.key() << ": " << i.value() << endl;
    }
}

void SyncConfigure::dumpMap(const QStringMultiMap &map)
{
    for (QStringMultiMap::const_iterator i=map.begin(); i != map.end(); i++) {
        for (QStringMap::const_iterator iv=i.value().begin(); iv != i.value().end(); iv++) {
            qDebug() << QString("[%1][%2] = %3").arg(i.key()).arg(iv.key()).arg(iv.value());
        }
    }
}

