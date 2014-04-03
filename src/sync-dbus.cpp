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

#include "sync-dbus.h"
#include "sync-daemon.h"
#include "sync-account.h"

SyncDBus::SyncDBus(const QDBusConnection &connection, SyncDaemon *parent)
    : QDBusAbstractAdaptor(parent),
      m_parent(parent),
      m_connection(connection)
{
    connect(m_parent, SIGNAL(syncStarted(SyncAccount*,QString)), SLOT(onSyncStarted(SyncAccount*,QString)));
    connect(m_parent, SIGNAL(syncFinished(SyncAccount*,QString)), SLOT(onSyncFinished(SyncAccount*,QString)));
    connect(m_parent, SIGNAL(syncError(SyncAccount*,QString,QString)), SLOT(onSyncError(SyncAccount*,QString,QString)));
    connect(m_parent, SIGNAL(syncAboutToStart()), SLOT(updateState()));
    connect(m_parent, SIGNAL(done()), SLOT(updateState()));
    updateState();
}

void SyncDBus::sync(QStringList services)
{
    if (services.isEmpty()) {
        m_parent->syncAll();
    } else {
        Q_FOREACH(const QString &service, services) {
            m_parent->syncAll(service);
        }
    }
}

void SyncDBus::cancel(QStringList services)
{
    if (services.isEmpty()) {
        m_parent->cancel();
    } else {
        Q_FOREACH(const QString &service, services) {
            m_parent->cancel(service);
        }
    }
}

QString SyncDBus::state()
{
    return m_state;
}

QStringList SyncDBus::servicesAvailable()
{
    return m_parent->availableServices();
}

void SyncDBus::onSyncStarted(SyncAccount *syncAcc, const QString &serviceName)
{
    updateState();
    Q_EMIT syncStarted(syncAcc->displayName(), serviceName);
}

void SyncDBus::onSyncFinished(SyncAccount *syncAcc, const QString &serviceName)
{
    Q_EMIT syncFinished(syncAcc->displayName(), serviceName);
}

void SyncDBus::onSyncError(SyncAccount *syncAcc, const QString &serviceName, const QString &error)
{
    qDebug() << "Sync error" << syncAcc->displayName() << serviceName << error;
    Q_EMIT syncError(syncAcc->displayName(), serviceName, error);
}

void SyncDBus::updateState()
{
    QString newState = (m_parent->isSyncing() ? "syncing" : "idle");
    if (newState != m_state) {
        m_state = newState;
        Q_EMIT stateChanged();
    }
}


