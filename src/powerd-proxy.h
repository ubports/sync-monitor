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

#ifndef __POWERD_PROXY_H__
#define __POWERD_PROXY_H__

#include "dbustypes.h"

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QHash>

#include <QtDBus/QDBusInterface>

class SyncEvolutionSessionProxy;

class PowerdProxy : public QObject
{
    Q_OBJECT
public:
    PowerdProxy(QObject *parent=0);
    ~PowerdProxy();

    QString requestWakelock(const QString &name) const;
    bool clearWakelock(const QString &cookie) const;

public Q_SLOTS:
    void lock();
    void unlock();

private:
    QDBusInterface *m_iface;
    QString m_currentLock;
};

#endif
