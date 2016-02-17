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

#include "syncevolution-server-proxy.h"
#include "syncevolution-session-proxy.h"

#include <QtCore/QDebug>
#include <QtDBus/QDBusReply>

#define SYNCEVOLUTION_SERVICE_NAME          "org.syncevolution"
#define SYNCEVOLUTION_OBJECT_PATH           "/org/syncevolution/Server"
#define SYNCEVOLUTION_IFACE_NAME            "org.syncevolution.Server"

SyncEvolutionServerProxy *SyncEvolutionServerProxy::m_instance = 0;

SyncEvolutionServerProxy::SyncEvolutionServerProxy(QObject *parent)
    : QObject(parent)
{
    m_iface = new QDBusInterface(SYNCEVOLUTION_SERVICE_NAME,
                                 SYNCEVOLUTION_OBJECT_PATH,
                                 SYNCEVOLUTION_IFACE_NAME);
}

SyncEvolutionServerProxy::~SyncEvolutionServerProxy()
{
    if (m_iface) {
        m_iface->call("Detach");
        delete m_iface;
        m_iface = 0;
    }
}

void SyncEvolutionServerProxy::killServer()
{
    QProcess p;
    p.execute("pkill syncevo-dbus-server");
    p.waitForFinished();
}

SyncEvolutionServerProxy *SyncEvolutionServerProxy::instance()
{
    if (!m_instance) {
        m_instance = new SyncEvolutionServerProxy();
    }
    return m_instance;
}

void SyncEvolutionServerProxy::destroy()
{
    if (m_instance) {
        delete m_instance;
        m_instance = 0;
    }
}

SyncEvolutionSessionProxy* SyncEvolutionServerProxy::openSession(const QString &sessionName,
                                                                 QStringList flags)
{
    QDBusReply<QDBusObjectPath> reply;
    if (flags.isEmpty()) {
        reply = m_iface->call("StartSession", sessionName);
    } else {
        reply = m_iface->call("StartSessionWithFlags", sessionName, flags);
    }

    if (m_iface->lastError().isValid()) {
        qWarning() << "Fail to start session" << m_iface->lastError().message();
        return 0;
    }

    return new SyncEvolutionSessionProxy(sessionName, reply.value(), this);
}

QStringList SyncEvolutionServerProxy::configs(bool templates) const
{
    QDBusReply<QStringList> reply = m_iface->call("GetConfigs", templates);
    return reply.value();
}

void SyncEvolutionServerProxy::getDatabases(const QString &sourceName)
{
    QDBusPendingCall pcall =  m_iface->asyncCall("GetDatabases", sourceName);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, this);

    QObject::connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
                     this, SLOT(getDatabasesFinished(QDBusPendingCallWatcher*)));
}

void SyncEvolutionServerProxy::getDatabasesFinished(QDBusPendingCallWatcher *call)
{
    QDBusPendingReply<QArrayOfDatabases> reply = *call;
    if (reply.isError()) {
        qWarning() << "Fail to fetch databases" << reply.error().message();
        Q_EMIT databasesReceived(QArrayOfDatabases());
    } else {
         Q_EMIT databasesReceived(reply.value());
    }
    call->deleteLater();
}
