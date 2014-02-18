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

#ifndef __SYNCEVOLUTION_SERVER_PROXY_H__
#define __SYNCEVOLUTION_SERVER_PROXY_H__

#include "dbustypes.h"

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QHash>

#include <QtDBus/QDBusInterface>

class SyncEvolutionSessionProxy;

class SyncEvolutionServerProxy : public QObject
{
    Q_OBJECT
public:
    static SyncEvolutionServerProxy *instance();
    static void destroy();

    SyncEvolutionSessionProxy* openSession(const QString &sessionName, QStringList flags);

private:
    static SyncEvolutionServerProxy *m_instance;
    QDBusInterface *m_iface;

    SyncEvolutionServerProxy(QObject *parent = 0);
    ~SyncEvolutionServerProxy();
};

#endif
