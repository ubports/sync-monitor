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

#ifndef __SYNC_DBUS_H__
#define __SYNC_DBUS_H__

#include "dbustypes.h"

#include <QtCore/QObject>
#include <QtDBus/QDBusAbstractAdaptor>
#include <QtDBus/QDBusConnection>

#define SYNCMONITOR_SERVICE_NAME    "com.canonical.SyncMonitor"
#define SYNCMONITOR_OBJECT_PATH     "/com/canonical/SyncMonitor"

class SyncDaemon;
class SyncAccount;

class SyncDBus : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", SYNCMONITOR_SERVICE_NAME)
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"com.canonical.SyncMonitor\">\n"
"    <property name=\"state\" type=\"s\" access=\"read\"/>\n"
"    <property name=\"enabledServices\" type=\"as\" access=\"read\"/>\n"
"    <property name=\"syncOnMobileConnection\" type=\"b\" access=\"readwrite\"/>\n"
"    <signal name=\"syncStarted\">\n"
"      <arg direction=\"out\" type=\"s\" name=\"account\"/>\n"
"      <arg direction=\"out\" type=\"s\" name=\"service\"/>\n"
"    </signal>\n"
"    <signal name=\"syncFinished\">\n"
"      <arg direction=\"out\" type=\"s\" name=\"account\"/>\n"
"      <arg direction=\"out\" type=\"s\" name=\"service\"/>\n"
"    </signal>\n"
"    <signal name=\"syncError\">\n"
"      <arg direction=\"out\" type=\"s\" name=\"account\"/>\n"
"      <arg direction=\"out\" type=\"s\" name=\"service\"/>\n"
"      <arg direction=\"out\" type=\"s\" name=\"error\"/>\n"
"    </signal>\n"
"    <signal name=\"stateChanged\"/>\n"
"    <signal name=\"enabledServicesChanged\"/>\n"
"    <method name=\"servicesAvailable\">\n"
"      <arg direction=\"out\" type=\"as\" name=\"services\"/>\n"
"    </method>\n"
"    <method name=\"sync\">\n"
"      <arg direction=\"in\" type=\"as\"/>\n"
"    </method>\n"
"    <method name=\"syncAccount\">\n"
"      <arg direction=\"in\" type=\"u\"/>\n"
"      <arg direction=\"in\" type=\"s\"/>\n"
"      <arg direction=\"in\" type=\"as\"/>\n"
"    </method>\n"
"    <method name=\"listCalendarsByAccount\">\n"
"      <arg direction=\"in\" type=\"u\"/>\n"
"      <arg direction=\"out\" type=\"as\" name=\"calendars\"/>\n"
"    </method>\n"
"    <method name=\"lastSuccessfulSyncDate\">\n"
"      <arg direction=\"in\" type=\"u\"/>\n"
"      <arg direction=\"in\" type=\"s\"/>\n"
"      <arg direction=\"out\" type=\"s\" name=\"date\"/>\n"
"    </method>\n"
"    <method name=\"cancel\">\n"
"      <arg direction=\"in\" type=\"as\"/>\n"
"    </method>\n"
"    <method name=\"attach\"/>\n"
"    <method name=\"detach\"/>\n"
"    <method name=\"state\" >\n"
"      <arg direction=\"out\" type=\"s\"/>\n"
"    </method>\n"
"  </interface>\n"
        "")
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QStringList enabledServices READ enabledServices NOTIFY enabledServicesChanged)
    Q_PROPERTY(bool syncOnMobileConnection READ syncOnMobileConnection WRITE setSyncOnMobileConnection)

public:
    SyncDBus(const QDBusConnection &connection, SyncDaemon *parent);
    bool start();
    bool syncOnMobileConnection() const;
    void setSyncOnMobileConnection(bool flag);

Q_SIGNALS:
    void syncStarted(const QString &account, const QString &service);
    void syncFinished(const QString &account, const QString &service);
    void syncError(const QString &account, const QString &service, const QString &error);
    void stateChanged();
    void enabledServicesChanged();
    void clientAttached(int count);
    void clientDeattached(int count);

public Q_SLOTS:
    void sync(QStringList service);
    void syncAccount(quint32 accountId, const QString &service, const QStringList &sources);
    QString lastSuccessfulSyncDate(quint32 accountId, const QString &source, const QDBusMessage &message);
    QStringList listCalendarsByAccount(quint32 accountId, const QDBusMessage &message);
    void cancel(QStringList services);
    QString state() const;
    QStringList enabledServices() const;
    QStringList servicesAvailable();
    void attach();
    void detach();

private Q_SLOTS:
    void onSyncStarted(SyncAccount *syncAcc, const QString &serviceName);
    void onSyncFinished(SyncAccount *syncAcc, const QString &serviceName);
    void onSyncError(SyncAccount *syncAcc, const QString &serviceName, const QString &error);
    void updateState();

private:
    SyncDaemon *m_parent;
    QDBusConnection m_connection;
    QString m_state;
    int m_clientCount;
};

#endif
