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
      m_connection(connection),
      m_clientCount(0)
{
    connect(m_parent, SIGNAL(syncStarted(SyncAccount*,QString)), SLOT(onSyncStarted(SyncAccount*,QString)));
    connect(m_parent, SIGNAL(syncFinished(SyncAccount*,QString)), SLOT(onSyncFinished(SyncAccount*,QString)));
    connect(m_parent, SIGNAL(syncError(SyncAccount*,QString,QString)), SLOT(onSyncError(SyncAccount*,QString,QString)));
    connect(m_parent, SIGNAL(syncAboutToStart()), SLOT(updateState()));
    connect(m_parent, SIGNAL(done()), SLOT(updateState()));
    connect(m_parent, SIGNAL(accountsChanged()), SIGNAL(enabledServicesChanged()));
    connect(m_parent, SIGNAL(isOnlineChanged(bool)), SIGNAL(enabledServicesChanged()));
    updateState();
}

bool SyncDBus::syncOnMobileConnection() const
{
    return m_parent->syncOnMobileConnection();
}

void SyncDBus::setSyncOnMobileConnection(bool flag)
{
    m_parent->setSyncOnMobileConnection(flag);
}

void SyncDBus::syncAll()
{
    m_parent->syncAll(true, true);
}

void SyncDBus::syncAccount(quint32 accountId, const QStringList &sources)
{
    m_parent->syncAccount(accountId, sources);
}

void SyncDBus::cancelAll()
{
    m_parent->cancel();
}

QString SyncDBus::state() const
{
    return m_state;
}

QStringList SyncDBus::enabledServices() const
{
    // return enabled sercives only in online mode
    if (m_parent->isOnline()) {
        return m_parent->enabledServices();
    } else {
        return QStringList();
    }
}

QStringList SyncDBus::servicesAvailable()
{
    return m_parent->availableServices();
}

void SyncDBus::attach()
{
    m_clientCount++;
    Q_EMIT clientAttached(m_clientCount);
}

void SyncDBus::detach()
{
    m_clientCount--;
    Q_EMIT clientDeattached(m_clientCount);
}

QString SyncDBus::lastSuccessfulSyncDate(quint32 accountId, const QString &remoteId, const QDBusMessage &message)
{
    message.setDelayedReply(true);

    QString result = m_parent->lastSuccessfulSyncDate(accountId, remoteId);
    QDBusMessage reply = message.createReply(QVariant::fromValue<QString>(result));
    QDBusConnection::sessionBus().send(reply);
    return result;
}

QMap<QString, QString> SyncDBus::listCalendarsByAccount(quint32 accountId, const QDBusMessage &message)
{
    QMap<QString, QString> result;
    message.setDelayedReply(true);
    SyncAccount *acc = m_parent->accountById(accountId);
    if (acc) {
        connect(acc, &SyncAccount::remoteSourcesAvailable, [message] (const QArrayOfDatabases &sources, int error) {
            QMap<QString, QString> dbs;
            Q_FOREACH(const SyncDatabase &db, sources) {
                dbs.insert(db.remoteId, db.source);
            }
            QDBusMessage reply = message.createReply(QVariant::fromValue(dbs));
            QDBusConnection::sessionBus().send(reply);
        });
        acc->fetchRemoteSources(acc->calendarServiceName());
    } else {
        qWarning() << "Invalid account id" << accountId;
        QDBusMessage reply = message.createReply(QVariant::fromValue(result));
        QDBusConnection::sessionBus().send(reply);
    }

    return result;
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
    QString newState = "idle";
    if (m_parent->isSyncing()) {
        newState = "syncing";
    } else if (m_parent->isPending()) {
        newState = "pending";
    }

    if (newState != m_state) {
        m_state = newState;
        Q_EMIT stateChanged();
    }
}


