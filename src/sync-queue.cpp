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

int SyncQueue::count() const
{
    return m_jobs.count();
}

bool SyncQueue::isEmpty() const
{
    return m_jobs.isEmpty();
}

void SyncQueue::clear()
{
    m_jobs.clear();;
}

QList<SyncJob>  SyncQueue::jobs() const
{
    return m_jobs;
}

void SyncQueue::push(SyncAccount *account,
                     const QStringList &serviceNames,
                     bool syncOnPayedConnection)
{
    QStringList newServices;
    if (serviceNames.isEmpty()) {
        newServices = account->availableServices();
    } else {
        newServices = serviceNames;
    }

    Q_FOREACH(const QString &serviceName, newServices) {
        SyncJob job(account, serviceName, syncOnPayedConnection);
        push(job);
    }
}

void SyncQueue::push(SyncAccount *account,
                     const QString &serviceName,
                     bool syncOnPayedConnection)
{
    QStringList services;
    if (!serviceName.isEmpty()) {
        services << serviceName;
    }
    push(account, services, syncOnPayedConnection);
}

void SyncQueue::push(const SyncQueue &other)
{
    Q_FOREACH(const SyncJob &job, other.jobs()) {
        push(job);
    }
}

void SyncQueue::push(const SyncJob &job)
{
    if (!contains(job)) {
        m_jobs << job;
    }
}

bool SyncQueue::contains(const SyncJob &otherJob) const
{
    return contains(otherJob.account(), otherJob.serviceName());
}

bool SyncQueue::contains(SyncAccount *account, const QString &serviceName) const
{
    Q_FOREACH(const SyncJob &job, m_jobs) {
        if ((job.account() == account) &&
            (serviceName.isEmpty() || (job.serviceName() == serviceName))) {
            return true;
        }
    }
    return false;
}

SyncJob SyncQueue::popNext()
{
    if (m_jobs.isEmpty()) {
        return SyncJob();
    }
    return m_jobs.takeFirst();
}

void SyncQueue::remove(SyncAccount *account, const QString &serviceName)
{
    QList<SyncJob> newList = m_jobs;
    for(int i=0; i < m_jobs.count(); i++) {
        const SyncJob &job =  m_jobs[i];
        if ((job.account() == account) &&
            (serviceName.isEmpty() || (job.serviceName() == serviceName))) {
                newList.removeOne(job);
        }
    }

    m_jobs = newList;
}


SyncJob::SyncJob()
    : m_account(0),
      m_runOnPayedConnection(false)
{
}

SyncJob::SyncJob(SyncAccount *account, const QString &serviceName, bool runOnPayedConnection)
    : m_account(account), m_serviceName(serviceName), m_runOnPayedConnection(runOnPayedConnection)
{
}

SyncAccount *SyncJob::account() const
{
    return m_account;
}

QString SyncJob::serviceName() const
{
    return m_serviceName;
}

bool SyncJob::runOnPayedConnection() const
{
    return m_runOnPayedConnection;
}

bool SyncJob::operator==(const SyncJob &other) const
{
    return (m_account == other.account()) && (m_serviceName == other.serviceName());
}

bool SyncJob::isValid() const
{
    return ((m_account != 0) && !m_serviceName.isEmpty());
}
