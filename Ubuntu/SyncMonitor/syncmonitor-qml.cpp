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

#include <QDebug>

#include "syncmonitor-qml.h"

#define SYNCMONITOR_DBUS_SERVICE_NAME   "com.canonical.SyncMonitor"
#define SYNCMONITOR_DBUS_OBJECT_PATH    "/com/canonical/SyncMonitor"
#define SYNCMONITOR_DBUS_INTERFACE      "com.canonical.SyncMonitor"


/*!
    \qmltype SyncMonitor
    \inqmlmodule Ubuntu.SyncMonitor 0.1
    \brief The SyncMonitor is a helper class to get access to SyncMonitor Daemon

    \b{This component is under heavy development.}

    Example:
    \qml
    Item {
        SyncMonitor {
            id: syncMonitor
        }
        Button {
            text: "sync"
            onClick: syncMonitor.sync(["calendar", ["contacts"])
        }
    }
    \endqml
*/

SyncMonitorQml::SyncMonitorQml(QObject *parent)
    : QObject(parent),
      m_iface(0)
{
}

SyncMonitorQml::~SyncMonitorQml()
{
    if (m_iface) {
        delete m_iface;
        m_iface = 0;
    }
}



/*!
  Specifies the current sync monitor state

  \list
  \li "idle"         - Service is in idle state
  \li "syncing"      - Service is running a sync
  \endlist

  \qmlproperty string state
*/
QString SyncMonitorQml::state() const
{
    if (m_iface) {
        return m_iface->property("state").toString();
    } else {
        return "";
    }
}

/*!
  Specifies the enabled services to sync

  \qmlproperty list enabledServices
*/
QStringList SyncMonitorQml::enabledServices() const
{
    if (m_iface) {
        return m_iface->property("enabledServices").toStringList();
    } else {
        return QStringList();
    }
}

void SyncMonitorQml::classBegin()
{
}

void SyncMonitorQml::componentComplete()
{
    m_iface = new QDBusInterface(SYNCMONITOR_DBUS_SERVICE_NAME,
                                 SYNCMONITOR_DBUS_OBJECT_PATH,
                                 SYNCMONITOR_DBUS_INTERFACE);
    if (m_iface->lastError().isValid()) {
        qWarning() << "Fail to connect with sync monitor:" << m_iface->lastError();
        return;
    }

    connect(m_iface, SIGNAL(stateChanged()), SIGNAL(stateChanged()));
    connect(m_iface, SIGNAL(enabledServicesChanged()), SIGNAL(enabledServicesChanged()));
    Q_EMIT stateChanged();
    Q_EMIT enabledServicesChanged();
}

/*!
  Start a new sync for specified services
*/
void SyncMonitorQml::sync(const QStringList &services)
{
    if (m_iface) {
        m_iface->call("sync", services);
    }
}

/*!
  Cancel current sync for specified services
*/
void SyncMonitorQml::cancel(const QStringList &services)
{
    if (m_iface) {
        m_iface->call("cancel", services);
    }
}

/*!
  Chek if a specific service is enabled or not
*/
bool SyncMonitorQml::serviceIsEnabled(const QString &service)
{
    return enabledServices().contains(service);
}
