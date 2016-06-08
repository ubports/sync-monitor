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

#ifndef SYNCMONITOR_QML_H
#define SYNCMONITOR_QML_H

#include <QObject>
#include <QQmlParserStatus>
#include <QDBusInterface>
#include <QDBusServiceWatcher>

class SyncMonitorQml : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(QStringList enabledServices READ enabledServices NOTIFY enabledServicesChanged)

public:
    SyncMonitorQml(QObject *parent = 0);
    ~SyncMonitorQml();

    QString state() const;
    QStringList enabledServices() const;

    void classBegin();
    void componentComplete();

Q_SIGNALS:
    void syncStarted(const QString &account, const QString &service);
    void syncFinished(const QString &account, const QString &service);
    void syncError(const QString &account, const QString &service, const QString &error);
    void stateChanged();
    void enabledServicesChanged();

public Q_SLOTS:
    void sync(const QStringList &services);
    void cancel(const QStringList &services);
    bool serviceIsEnabled(const QString &service);

    void connectToServer();
    void disconnectFromServer();

private:
    QDBusInterface *m_iface;
    QDBusServiceWatcher *m_watcher;
};

#endif
