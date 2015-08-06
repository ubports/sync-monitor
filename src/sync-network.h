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
#include <QtCore/QTimer>
#include <QtNetwork/QNetworkConfigurationManager>


class SyncNetwork : public QObject
{
    Q_OBJECT
    Q_PROPERTY(NetworkState state READ state NOTIFY stateChanged)

public:
    enum NetworkState {
        NetworkOffline = 0,
        NetworkPartialOnline,
        NetworkOnline
    };
    SyncNetwork(QObject *parent=0);
    ~SyncNetwork();

    NetworkState state() const;
    void setState(NetworkState newState);

Q_SIGNALS:
    void stateChanged(SyncNetwork::NetworkState state);

private Q_SLOTS:
    void refresh();
    void idleRefresh();

private:
    QScopedPointer<QNetworkConfigurationManager> m_configManager;
    NetworkState m_state;
    QTimer m_idleRefresh;
};

#endif
