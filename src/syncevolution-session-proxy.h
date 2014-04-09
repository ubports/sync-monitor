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
    QString id() const;
    void destroy();
    QString status() const;
    bool hasConfig(const QString &configName);
    QStringMultiMap getConfig(const QString &configName, bool isTemplate);
    bool saveConfig(const QString &configName, QStringMultiMap config);
    void detach();
    bool isValid() const;
    void sync(QString mode, QStringMap services);
    QArrayOfStringMap reports(uint start, uint maxCount);

Q_SIGNALS:
    void statusChanged(const QString &status);
    void progressChanged(int progress);
    void error(uint error);

private Q_SLOTS:
    void onSessionStatusChanged(const QString &status, uint error, QSyncStatusMap source);
    void onSessionProgressChanged(int progress, QSyncProgressMap sources);

private:
    QDBusInterface *m_iface;

    SyncEvolutionSessionProxy(const QDBusObjectPath &objectPath, QObject *parent=0);
    friend class SyncEvolutionServerProxy;
};

#endif
