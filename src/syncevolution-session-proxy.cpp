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

SyncEvolutionSessionProxy::SyncEvolutionSessionProxy(const QDBusObjectPath &objectPath, QObject *parent)
    : QObject(parent)
{
    m_iface = new QDBusInterface(SYNCEVOLUTION_SERVICE_NAME,
                             objectPath.path(),
                             SYNCEVOLUTIOON_SESSION_IFACE_NAME);
    if (m_iface->lastError().isValid()) {
        qWarning() << "Fail to create session:" << objectPath.path() << m_iface->lastError().message();
    } else {
        m_iface->connection().connect(SYNCEVOLUTION_SERVICE_NAME,
                                      objectPath.path(),
                                      SYNCEVOLUTIOON_SESSION_IFACE_NAME,
                                      "StatusChanged",
                                      this,
                                      SLOT(onSessionStatusChanged(QString,uint, QSyncStatusMap)));

        m_iface->connection().connect(SYNCEVOLUTION_SERVICE_NAME,
                                      objectPath.path(),
                                      SYNCEVOLUTIOON_SESSION_IFACE_NAME,
                                      "ProgressChanged",
                                      this,
                                      SLOT(onSessionProgressChanged(int, QSyncProgressMap)));
    }
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
        qWarning() << "Fail to get session named config" << reply.error().message();
        return false;
    }
    return (reply.value().size() > 0);
}

QStringMultiMap SyncEvolutionSessionProxy::getConfig(const QString &configName,
                                                             bool isTemplate)
{
    Q_ASSERT(isValid());
    QDBusReply<QStringMultiMap> reply = m_iface->call("GetNamedConfig",
                                                              configName,
                                                              isTemplate);
    if (reply.error().isValid()) {
        qWarning() << "Fail to get session named config" << reply.error().message();
        return QStringMultiMap();
    }

    return reply.value();
}

bool SyncEvolutionSessionProxy::saveConfig(const QString &configName,
                                           QStringMultiMap config)
{
    Q_ASSERT(isValid());
    QDBusReply<void> reply;
    if (configName.isEmpty()) {
        reply = m_iface->call("SetConfig",
                              false,
                              false,
                              QVariant::fromValue(config));
    } else {
        reply = m_iface->call("SetNamedConfig",
                              configName,
                              false,
                              false,
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

void SyncEvolutionSessionProxy::sync(QString mode, QStringMap services)
{
    Q_ASSERT(isValid());
    QDBusReply<void> reply = m_iface->call("Sync", QString(), QVariant::fromValue(services));
    if (reply.error().isValid()) {
        qWarning() << "Fail to sync account" << reply.error().message();
        Q_EMIT this->error(0);
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

void SyncEvolutionSessionProxy::onSessionStatusChanged(const QString &status, uint error, QSyncStatusMap source)
{
    Q_UNUSED(source);
    Q_EMIT statusChanged(status);
    if (error != 0) {
        Q_EMIT this->error(error);
    }
}

void SyncEvolutionSessionProxy::onSessionProgressChanged(int progress, QSyncProgressMap sources)
{
    Q_UNUSED(sources);
    Q_EMIT progressChanged(progress);
}
