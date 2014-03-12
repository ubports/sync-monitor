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

#include "sync-queue.h"
#include "sync-account.h"

SyncQueue::SyncQueue()
{
}

SyncQueue::~SyncQueue()
{
}

void SyncQueue::push(SyncAccount *account, const QString &serviceName)
{
    QSet<QString> services = m_queue.value(account);
    if (serviceName.isEmpty()) {
        // empty serviceName means all services
        services = account->availableServices().toSet();
    } else {
        services.insert(serviceName);
    }
    if (services.isEmpty()) {
        m_queue.remove(account);
    } else {
        m_queue.insert(account, services);
    }
}

bool SyncQueue::contains(SyncAccount *account, const QString &serviceName) const
{
    if (serviceName.isEmpty()) {
        return m_queue.contains(account);
    } else {
        QSet<QString> services = m_queue.value(account);
        return services.contains(serviceName);
    }
}

int SyncQueue::count() const
{
    return m_queue.values().size();
}

bool SyncQueue::isEmpty() const
{
    return m_queue.isEmpty();
}

QString SyncQueue::popNext(SyncAccount **account)
{
    if (m_queue.isEmpty()) {
        *account = 0;
        return QString();
    }

    *account = m_queue.keys().first();
    QSet<QString> services = m_queue.value(*    account);
    QString nextService = *(services.begin());
    if (services.size() == 1) {
        m_queue.remove(*account);
    } else {
        services.remove(nextService);
        m_queue.insert(*account, services);
    }
    return nextService;
}

SyncAccount *SyncQueue::popNext()
{
    SyncAccount *acc = m_queue.keys().first();
    m_queue.remove(acc);
    return acc;
}

void SyncQueue::remove(SyncAccount *account, const QString &serviceName)
{
    if (serviceName.isEmpty()) {
        m_queue.remove(account);
    } else {
        QSet<QString> services = m_queue.value(account);
        if (services.contains(serviceName)) {
            if (services.size() == 1) {
                m_queue.remove(account);
            } else {
                services.remove(serviceName);
                m_queue.insert(account, services);
            }
        }
    }
}
