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

#include "syncevolution-session-proxy.h"
#include "dbustypes.h"

#include <QtCore/QDebug>
#include <QtDBus/QDBusReply>

#define SYNCEVOLUTION_SERVICE_NAME          "org.syncevolution"
#define SYNCEVOLUTIOON_SESSION_IFACE_NAME   "org.syncevolution.Session"

SyncEvolutionSessionProxy::SyncEvolutionSessionProxy(const QString &sessionName,
                                                     const QDBusObjectPath &objectPath,
                                                     QObject *parent)
    : QObject(parent),
      m_sessionName(sessionName)
{
    m_iface = new QDBusInterface(SYNCEVOLUTION_SERVICE_NAME,
                                 objectPath.path(),
                                 SYNCEVOLUTIOON_SESSION_IFACE_NAME);

    m_iface->connection().connect(SYNCEVOLUTION_SERVICE_NAME,
                                  objectPath.path(),
                                  SYNCEVOLUTIOON_SESSION_IFACE_NAME,
                                  "StatusChanged",
                                  this,
                                  SIGNAL(statusChanged(QString,uint,QSyncStatusMap)));

    m_iface->connection().connect(SYNCEVOLUTION_SERVICE_NAME,
                                  objectPath.path(),
                                  SYNCEVOLUTIOON_SESSION_IFACE_NAME,
                                  "ProgressChanged",
                                  this,
                                  SLOT(onSessionProgressChanged(int, QSyncProgressMap)));
}

QString SyncEvolutionSessionProxy::sessionName() const
{
    return m_sessionName;
}

QString SyncEvolutionSessionProxy::id() const
{
    Q_ASSERT(isValid());
    return m_iface->path();
}

void SyncEvolutionSessionProxy::destroy()
{
    // abort any operation
    m_iface->call("Abort");

    // notify server to close the session
    m_iface->call("Detach");

    // delete interface
    m_iface->deleteLater();

    m_iface = 0;
}

QString SyncEvolutionSessionProxy::status() const
{
    Q_ASSERT(isValid());
    QDBusReply<QString> reply = m_iface->call("GetStatus");
    if (reply.error().isValid()) {
        qWarning() << "Fail to get session status" << reply.error().message();
        return QString();
    } else {
        return reply.value();
    }
}

bool SyncEvolutionSessionProxy::hasConfig(const QString &configName)
{
    Q_ASSERT(isValid());
    QDBusReply<QStringMultiMap> reply = m_iface->call("GetNamedConfig", configName, false);
    if (reply.error().isValid()) {
        return false;
    }
    return (reply.value().size() > 0);
}

QStringMultiMap SyncEvolutionSessionProxy::getConfig(const QString &configName,
                                                     bool isTemplate)
{
    Q_ASSERT(isValid());
    QDBusReply<QStringMultiMap> reply;

    if (configName.isEmpty()) {
        reply = m_iface->call("GetConfig",
                              isTemplate);
    } else {
        reply = m_iface->call("GetNamedConfig",
                              configName,
                              isTemplate);
    }
    if (reply.error().isValid()) {
        qWarning() << "Fail to get session named config" << reply.error().message();
        return QStringMultiMap();
    }

    return reply.value();
}

bool SyncEvolutionSessionProxy::saveConfig(const QString &configName,
                                           QStringMultiMap config,
                                           bool temporary)
{
    Q_ASSERT(isValid());
    QDBusReply<void> reply;
    if (configName.isEmpty()) {
        reply = m_iface->call("SetConfig",
                              false,
                              temporary,
                              QVariant::fromValue(config));
    } else {
        reply = m_iface->call("SetNamedConfig",
                              configName,
                              false,
                              temporary,
                              QVariant::fromValue(config));
    }
    if (reply.error().isValid()) {
        qWarning() << "Fail to save named config" << reply.error().message();
        return false;
    }
    return true;
}

bool SyncEvolutionSessionProxy::isValid() const
{
    return (m_iface != 0);
}

void SyncEvolutionSessionProxy::sync(const QString &mode, QStringMap services)
{
    Q_ASSERT(isValid());
    QDBusReply<void> reply = m_iface->call("Sync", mode, QVariant::fromValue(services));
    if (reply.error().isValid()) {
        qWarning() << "Fail to sync account" << reply.error().message();
    }
}

QArrayOfStringMap SyncEvolutionSessionProxy::reports(uint start, uint maxCount)
{
    QDBusReply<QArrayOfStringMap> reply = m_iface->call("GetReports", start, maxCount);
    if (reply.error().isValid()) {
        qWarning() << "Fail to get sync reports" << reply.error().message();
        return QArrayOfStringMap();
    } else {
        return reply.value();
    }
}

QArrayOfDatabases SyncEvolutionSessionProxy::getDatabases(const QString &configName)
{
    QDBusReply<QArrayOfDatabases> reply = m_iface->call("GetDatabases", configName);
    if (reply.error().isValid()) {
        qWarning() << "Fail to get databases" << reply.error().message();
        return QArrayOfDatabases();
    } else {
        return reply.value();
    }
}

void SyncEvolutionSessionProxy::execute(const QStringList &args)
{
    QDBusReply<void> reply = m_iface->call("Execute", args);
    if (reply.error().isValid()) {
        qWarning() << "Fail to execute command" << reply.error().message();
    }
}

void SyncEvolutionSessionProxy::onSessionProgressChanged(int progress, QSyncProgressMap sources)
{
    Q_UNUSED(sources);
    Q_EMIT progressChanged(progress);
}
