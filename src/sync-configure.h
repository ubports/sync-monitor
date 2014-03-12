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

#ifndef __SYNC_CONFIGURE_H__
#define __SYNC_CONFIGURE_H__

#include <QtCore/QObject>
#include <QtCore/QSettings>

#include <Accounts/Account>

class SyncEvolutionSessionProxy;

class SyncConfigure : public QObject
{
    Q_OBJECT
public:
    SyncConfigure(Accounts::Account *account,
                  QSettings *settings,
                  QObject *parent = 0);
    ~SyncConfigure();

    void configure(const QString &serviceName);
    void configureAll();
    QString serviceName() const;

Q_SIGNALS:
    void configured(const QString &serviceName);
    void done();
    void error();

public Q_SLOTS:
    void onSessionStatusChanged(const QString &newStatus);
    void onSessionError(uint error);

private:
    Accounts::Account *m_account;
    QMap<QString, SyncEvolutionSessionProxy*> m_sessions;
    QSettings *m_settings;
    QStringList m_services;
    QString m_originalServiceName;

    void continueConfigure();
    void configureServices();
    void configureService(const QString &serviceName);
    void removeService(const QString &serviceName);
    bool configTarget(const QString &targetName, const QString &serviceName);
    bool configSync(const QString &targetName, const QString &serviceName);
};

#endif
