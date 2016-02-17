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

#include "dbustypes.h"


class SyncEvolutionSessionProxy;

class SyncConfigure : public QObject
{
    Q_OBJECT
public:
    SyncConfigure(Accounts::Account *account,
                  QSettings *settings,
                  QObject *parent = 0);
    ~SyncConfigure();

    Accounts::AccountId accountId() const;
    void configure();


    static QString accountSessionName(Accounts::Account *account);
    static void dumpMap(const QStringMultiMap &map);
    static void dumpMap(const QStringMap &map);

Q_SIGNALS:
    void done(const QStringList &services);
    void error(const QStringList &services);

private Q_SLOTS:
    void fetchRemoteCalendarsSessionDone(const QArrayOfDatabases &databases);

private:
    Accounts::Account *m_account;
    QMap<QString, QArrayOfDatabases> m_remoteDatabasesByService;
    QMap<SyncEvolutionSessionProxy*, QStringList> m_peers;
    QSettings *m_settings;

    void fetchRemoteCalendars();
    void fetchRemoteCalendarsFromSession(SyncEvolutionSessionProxy *session);
    void configurePeer(const QStringList &services);
    void continuePeerConfig(SyncEvolutionSessionProxy *session, const QStringList &services);
    void checkSyncConfig(SyncEvolutionSessionProxy *session,
                         const QString &peerName,
                         const QString &serviceName,
                         const QString &localDbId);
    bool createSyncConfig(SyncEvolutionSessionProxy *session, const QString &configName, const QString &peerName, const QString &serviceName, const QString &localDbId);
    QString registerDatabase(SyncEvolutionSessionProxy *session, const QString &localDatabaseName, const QString &localDatabaseId);

    static QString formatSourceName(const QString &name);
    static bool updateConfig(QStringMultiMap &config, const QString &source, const QString &key, const QString &value);
};

#endif
