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
      m_iface(0),
      m_watcher(0)
{
}

SyncMonitorQml::~SyncMonitorQml()
{
    if (m_watcher) {
        delete m_watcher;
        m_watcher = 0;
    }
    if (m_iface) {
        m_iface->call("detach");
        disconnectFromServer();
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
    connectToServer();
    m_watcher = new QDBusServiceWatcher(QString(SYNCMONITOR_DBUS_SERVICE_NAME),
                                        QDBusConnection::sessionBus(),
                                        QDBusServiceWatcher::WatchForOwnerChange,
                                        this);
    connect(m_watcher, SIGNAL(serviceRegistered(QString)), SLOT(connectToServer()));
    connect(m_watcher, SIGNAL(serviceUnregistered(QString)), SLOT(disconnectFromServer()));
}

/*!
  Start a new sync for specified services
*/
void SyncMonitorQml::sync()
{
    if (m_iface) {
        m_iface->call("syncAll");
    }
}

/*!
  Cancel current sync for specified services
*/
void SyncMonitorQml::cancel()
{
    if (m_iface) {
        m_iface->call("cancelAll");
    }
}

/*!
  Chek if a specific service is enabled or not
*/
bool SyncMonitorQml::serviceIsEnabled(const QString &service)
{
    return enabledServices().contains(service);
}

void SyncMonitorQml::connectToServer()
{
    if (m_iface) {
        delete m_iface;
    }

    m_iface = new QDBusInterface(SYNCMONITOR_DBUS_SERVICE_NAME,
                                 SYNCMONITOR_DBUS_OBJECT_PATH,
                                 SYNCMONITOR_DBUS_INTERFACE);
    if (m_iface->lastError().isValid()) {
        qWarning() << "Fail to connect with sync monitor:" << m_iface->lastError();
        delete m_iface;
        m_iface = 0;
    } else {
        connect(m_iface, SIGNAL(stateChanged()), SIGNAL(stateChanged()));
        connect(m_iface, SIGNAL(enabledServicesChanged()), SIGNAL(enabledServicesChanged()));
        m_iface->call("attach");
        Q_EMIT enabledServicesChanged();
    }
    Q_EMIT stateChanged();
}

void SyncMonitorQml::disconnectFromServer()
{
    if (m_iface) {
        delete m_iface;
        m_iface = 0;
    }
    Q_EMIT enabledServicesChanged();
    Q_EMIT stateChanged();
}
