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

#ifndef __SYNC_QUEUE_H__
#define __SYNC_QUEUE_H__

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMap>

class SyncAccount;

template <class AccountClass>
class SyncQueue
{
public:
    void push(AccountClass *account, const QString &serviceName = QString());
    QString popNext(AccountClass **account);
    AccountClass *popNext();
    void remove(AccountClass *account, const QString &serviceName = QString());

    bool contains(AccountClass *account, const QString &serviceName) const;
    int count() const;
    bool isEmpty() const;

private:
    QMap<AccountClass*, QStringList> m_queue;
};

template<class AccountClass>
int SyncQueue<AccountClass>::count() const
{
    QList<QStringList > values = m_queue.values();
    int size = 0;
    for(int i=0; i < values.size(); i++) {
        size += values[i].size();
    }
    return size;
}

template<class AccountClass>
bool SyncQueue<AccountClass>::isEmpty() const
{
    return m_queue.isEmpty();
}

template<class AccountClass>
void SyncQueue<AccountClass>::push(AccountClass *account, const QString &serviceName)
{
    QStringList services = m_queue.value(account);
    if (serviceName.isEmpty()) {
        // empty serviceName means all services
        services = account->availableServices();
    } else if (!services.contains(serviceName)) {
        services.push_back(serviceName);
    }

    if (services.isEmpty()) {
        m_queue.remove(account);
    } else {
        m_queue.insert(account, services);
    }
}

template<class AccountClass>
bool SyncQueue<AccountClass>::contains(AccountClass *account, const QString &serviceName) const
{
    if (serviceName.isEmpty()) {
        return m_queue.contains(account);
    } else {
        QStringList services = m_queue.value(account);
        return services.contains(serviceName);
    }
}

template<class AccountClass>
QString SyncQueue<AccountClass>::popNext(AccountClass **account)
{
    if (m_queue.isEmpty()) {
        *account = 0;
        return QString();
    }

    *account = m_queue.keys().first();
    QStringList services = m_queue.value(*account);
    QString nextService = *(services.begin());
    if (services.size() == 1) {
        m_queue.remove(*account);
    } else {
        services.removeOne(nextService);
        m_queue.insert(*account, services);
    }
    return nextService;
}

template<class AccountClass>
AccountClass *SyncQueue<AccountClass>::popNext()
{
    AccountClass *acc = m_queue.keys().first();
    m_queue.remove(acc);
    return acc;
}

template<class AccountClass>
void SyncQueue<AccountClass>::remove(AccountClass *account, const QString &serviceName)
{
    if (serviceName.isEmpty()) {
        m_queue.remove(account);
    } else {
        QStringList services = m_queue.value(account);
        if (services.contains(serviceName)) {
            if (services.size() == 1) {
                m_queue.remove(account);
            } else {
                services.removeOne(serviceName);
                m_queue.insert(account, services);
            }
        }
    }
}

#endif

