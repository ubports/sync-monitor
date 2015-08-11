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

SyncConfigure::SyncConfigure(Accounts::Account *account,
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

void SyncConfigure::configure()
{
    QStringList services;
    Q_FOREACH(Service service, m_account->services()) {
        services << service.serviceType();
    }
    configurePeer(services);
}

QString SyncConfigure::accountSessionName(Account *account)
{
    return QString("%1-%2")
            .arg(account->providerName())
            .arg(account->id());
}

void SyncConfigure::configurePeer(const QStringList &services)
{
    SyncEvolutionServerProxy *proxy = SyncEvolutionServerProxy::instance();
    QString peerName = accountSessionName(m_account);
    SyncEvolutionSessionProxy *session = proxy->openSession(peerName,
                                                            QStringList() << "all-configs");
    m_peers.insert(session, services);
    connect(session, &SyncEvolutionSessionProxy::statusChanged,
        this, &SyncConfigure::onPeerSessionStatusChanged);

    qDebug() << "peer session created" << peerName << session->status();
    if (session->status() != "queueing") {
        continuePeerConfig(session, services);
    }
}

void SyncConfigure::onPeerSessionStatusChanged(const QString &status, uint errorNuber, QSyncStatusMap source)
{
    SyncEvolutionSessionProxy *session = qobject_cast<SyncEvolutionSessionProxy*>(QObject::sender());

    if (errorNuber != 0) {
        qWarning() << "Fail to configure peer" << errorNuber;
        if (session) {
            Q_EMIT error(m_peers.take(session));
            session->destroy();
            delete session;
        }
    } else {
        if (status != "queueing") {
            continuePeerConfig(session, m_peers.value(session));
        }
    }
}

void SyncConfigure::continuePeerConfig(SyncEvolutionSessionProxy *session, const QStringList &services)
{
    SyncEvolutionServerProxy *proxy = SyncEvolutionServerProxy::instance();
    QStringList configs = proxy->configs();

    QString peerName = accountSessionName(m_account);
    QString peerConfigName = QString("target-config@%1").arg(peerName);

    // config peer
    QStringMultiMap config;
    if (configs.contains(peerConfigName)) {
        config = session->getConfig(peerConfigName, true);
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
        templates.insert(CONTACTS_SERVICE_NAME, QString("source/addressbook"));
        templates.insert(CALENDAR_SERVICE_NAME, QString("source/calendar"));
    }

    EdsHelper eds;

    bool changed = false;
    QMap<QString, QString> sourceToDatabase;

    Q_FOREACH(const QString &service, services.toSet()) {
        qDebug() << "Configure source for service" << service;
        QString templateSource = templates.value(service, "");
        if (templateSource.isEmpty()) {
            qWarning() << "Fail to find template source. Skip service" << service;
            continue;
        }

        QArrayOfDatabases dbs = listDatabase(session, peerName, service);
        if (dbs.isEmpty()) {
            qWarning() << "Fail to get remote databases";
            continue;
        }

        // create new sources if necessary
        QStringMap configTemplate = config.value(templateSource);
        if (configTemplate.isEmpty()) {
            qWarning() << "Template not found" << templateSource;
            continue;
        }

        Q_FOREACH(const SyncDatabase &db, dbs) {
            // local dabase
            QString localDbName = QString("%1_%2").arg(db.name).arg(m_account->id());
            QString localDbId = eds.createSource(service, localDbName).split("::").last();

            // remote database
            QString sourceName = QString("%1_%2").arg(service).arg(formatSourceName(db.name));
            QString fullSourceName = QString("source/%1").arg(sourceName);
            if (config.contains(fullSourceName)) {
                qDebug() << "Source already configured" << sourceName;
                continue;
            }

            changed |= true;
            qDebug() << "Config source" << sourceName << "for database" << db.name << db.source;
            QStringMap sourceConfig(configTemplate);
            sourceConfig["database"] = db.source;
            config[fullSourceName] = sourceConfig;

            sourceToDatabase.insert(fullSourceName, localDbId);
        }

        // remove old sources if necessary
        QString sourcePrefix = QString("source/%1_").arg(service);
        Q_FOREACH(const QString &key, config.keys()) {
            if (key.startsWith(sourcePrefix)) {
                if (!sourceToDatabase.contains(key)) {
                    qDebug() << "Removing old source" << key;
                    config.remove(key);
                    changed |= true;
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
    } else {
        qDebug() << "Sources config did not change.";
    }

    session->destroy();
    delete session;

    // local session
    session = proxy->openSession("", QStringList() << "all-configs");
    if (session->status() == "queueing") {
        qWarning() << "Fail to open local session";
        session->destroy();
        delete session;
        return;
    }

    // create local sources
    changed = false;
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
    delete session;
    Q_EMIT done(services);
}

QArrayOfDatabases SyncConfigure::listDatabase(SyncEvolutionSessionProxy *session,
                                              const QString &peerName,
                                              const QString &serviceName)
{
    QArrayOfDatabases dbs;
    if (serviceName != "calendar") {
        SyncDatabase defaultDB;
        defaultDB.flag = true;
        defaultDB.name = m_account->displayName();
        defaultDB.source = m_account->displayName();
        return dbs << defaultDB;
    }

    // loads settings
    m_settings->beginGroup(serviceName);
    QString uoaServiceName = m_settings->value("uoa-service", "").toString();
    m_settings->endGroup();

    QStringMultiMap config = session->getConfig("Google", true);
    config[""]["username"] = QString("uoa:%1,%2").arg(m_account->id()).arg(uoaServiceName);
    config[""]["password"] = QString();
    config["source/calendar"]["backend"] = QString("caldav");

    if (session->saveConfig(peerName, config, true)) {
        qDebug() << "Fetching remote databases " << serviceName << "(wait...)";
        dbs = session->getDatabases(serviceName);
        Q_FOREACH(const SyncDatabase &db, dbs) {
            qDebug() << "Name" << db.name << "Source" << db.source << "flag" << db.flag;
        }
        qDebug() << "Done.";
    } else {
        qWarning() << "Fail to save database query";
    }

    return dbs;
}

QString SyncConfigure::formatSourceName(const QString &name)
{
    QString sourceName;
    for(int i=0; i < name.length(); i++) {
        if (name.at(i).isLetterOrNumber()) {
            sourceName += name.at(i);
        }
    }
    return sourceName;
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

