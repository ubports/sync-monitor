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

#ifndef __SYNC_NETWORK_H__
#define __SYNC_NETWORK_H__

#include <QtCore/QScopedPointer>
#include <QtNetwork/QNetworkConfigurationManager>


class SyncNetwork : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool online READ isOnline NOTIFY onlineChanged)

public:
    SyncNetwork(QObject *parent=0);
    ~SyncNetwork();

    bool isOnline() const;

Q_SIGNALS:
    void onlineChanged(bool isOnline);

private Q_SLOTS:
    void refresh();

private:
    QScopedPointer<QNetworkConfigurationManager> m_configManager;
    bool m_isOnline;
};

#endif
