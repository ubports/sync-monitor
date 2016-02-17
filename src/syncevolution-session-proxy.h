#ifndef __SYNCEVOLUTION_SESSION_PROXY_H__
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

#define __SYNCEVOLUTION_SESSION_PROXY_H__

#include "dbustypes.h"

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QHash>

#include <QtDBus/QDBusInterface>

class SyncEvolutionSessionProxy : public QObject
{
    Q_OBJECT
public:
    QString sessionName() const;
    QString id() const;
    void destroy();
    QString status() const;
    bool hasConfig(const QString &configName);
    QStringMultiMap getConfig(const QString &configName, bool isTemplate);
    bool saveConfig(const QString &configName, QStringMultiMap config, bool temporary = false);
    void detach();
    bool isValid() const;
    void sync(const QString &mode, QStringMap services);
    QArrayOfStringMap reports(uint start, uint maxCount);
    void getDatabases(const QString &sourceName);
    void execute(const QStringList &args);

Q_SIGNALS:
    void statusChanged(const QString &status, uint errorNuber, QSyncStatusMap source);
    void progressChanged(int progress);
    void databasesReceived(const QArrayOfDatabases &databases);

private Q_SLOTS:
    void onSessionProgressChanged(int progress, QSyncProgressMap sources);
    void getDatabasesFinished(QDBusPendingCallWatcher *call);

private:
    QString m_sessionName;
    QDBusInterface *m_iface;
    static uint m_count;

    SyncEvolutionSessionProxy(const QString &sessionName, const QDBusObjectPath &objectPath, QObject *parent=0);
    ~SyncEvolutionSessionProxy();

    friend class SyncEvolutionServerProxy;
};

#endif
