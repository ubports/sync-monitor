/*
 * Copyright 2016 Canonical Ltd.
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

#include "powerd-proxy.h"

#include <QtCore/QDebug>
#include <QtCore/QUuid>
#include <QtDBus/QDBusReply>

#define POWERD_SERVICE_NAME     "com.canonical.powerd"
#define POWERD_IFACE_NAME       "com.canonical.powerd"
#define POWERD_OBJECT_PATH      "/com/canonical/powerd"

PowerdProxy::PowerdProxy(QObject *parent)
    : QObject(parent)
{
    m_iface = new QDBusInterface(POWERD_SERVICE_NAME,
                                 POWERD_OBJECT_PATH,
                                 POWERD_IFACE_NAME,
                                 QDBusConnection::systemBus());
}

PowerdProxy::~PowerdProxy()
{
    unlock();
}

QString PowerdProxy::requestWakelock(const QString &name) const
{
    QDBusReply<QString> reply = m_iface->call("requestSysState", name, 1);
    if (reply.error().isValid()) {
        qWarning() << "Fail to request wake lock" << reply.error().message();
        return QString();
    }

    return reply.value();
}

bool PowerdProxy::clearWakelock(const QString &cookie) const
{
    QDBusReply<void> reply = m_iface->call("clearSysState", cookie);
    if (reply.error().isValid()) {
        qWarning() << "Fail to clear wake lock" << reply.error().message();
        return false;
    }
    return true;
}

void PowerdProxy::lock()
{
    if (!m_currentLock.isEmpty()) {
        qDebug() << "Wake lock aready created for sync-monitor";
        return;
    }

    QString cookie = requestWakelock("sync-monitor-wakelock");
    if (!cookie.isEmpty()) {
        m_currentLock = cookie;
    }
}

void PowerdProxy::unlock()
{
    if (!m_currentLock.isEmpty() &&  clearWakelock(m_currentLock)) {
        m_currentLock.clear();
    }
}
