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

const QString SyncJob::SyncAllKeyword = QString("ALL");

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

const QList<SyncJob> SyncQueue::jobs() const
{
    return m_jobs;
}

void SyncQueue::push(SyncAccount *account,
                     const QStringList &sources,
                     bool syncOnPayedConnection)
{
    // check if there is job for this account already
    for(int i=0; i < m_jobs.size(); i++) {
        SyncJob &job = m_jobs[i];
        if (job.account()->id() == account->id()) {
            job.appendSources(sources);
            return;
        }
    }

    // there is no job for this account, create
    SyncJob job(account, sources, syncOnPayedConnection);
    push(job);
}

void SyncQueue::push(SyncAccount *account,
                     const QString &sourceName,
                     bool syncOnPayedConnection)
{
    QStringList sources;
    if (!sourceName.isEmpty()) {
        sources << sourceName;
    }
    push(account, sources, syncOnPayedConnection);
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
    return contains(otherJob.account(), otherJob.sources());
}

bool SyncQueue::contains(SyncAccount *account, const QString &sourceName) const
{
    return contains(account, QStringList() << sourceName);
}

bool SyncQueue::contains(SyncAccount *account, const QStringList &sources) const
{
    Q_FOREACH(const SyncJob &job, m_jobs) {
        if (job.account() == account) {
            return job.contains(sources);
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

void SyncQueue::remove(const SyncJob &job)
{
    remove(job.account(), QStringList());
}

void SyncQueue::remove(SyncAccount *account, const QString &source)
{
    remove(account, QStringList() << source);
}

void SyncQueue::remove(SyncAccount *account, const QStringList &sources)
{
    QList<SyncJob> newList = m_jobs;
    for(int i=0; i < m_jobs.count(); i++) {
        SyncJob &job =  m_jobs[i];
        if (job.account()->id() == account->id()) {
            job.removeSources(sources);
            if (job.isEmpty()) {
                newList.removeOne(job);
            }
        }
    }

    m_jobs = newList;
}

SyncJob::SyncJob()
    : m_account(0),
      m_runOnPayedConnection(false)
{
}

SyncJob::SyncJob(SyncAccount *account, const QStringList &sources, bool runOnPayedConnection)
    : m_account(account), m_runOnPayedConnection(runOnPayedConnection)
{
    if (sources.isEmpty()) {
        m_sources << SyncJob::SyncAllKeyword;
    } else {
        m_sources << sources;
    }
}

SyncAccount *SyncJob::account() const
{
    return m_account;
}

QStringList SyncJob::sources() const
{
    if (m_sources.contains(SyncJob::SyncAllKeyword)) {
        return QStringList();
    } else {
        return m_sources;
    }
}

void SyncJob::appendSources(const QStringList &sources)
{
    if (m_sources.contains(SyncJob::SyncAllKeyword)) {
        return;
    }

    Q_FOREACH(const QString &source, sources) {
        if (!m_sources.contains(source)) {
            m_sources.append(source);
        }
    }
}

void SyncJob::removeSources(const QStringList &sources)
{
    if (sources.isEmpty()) {
        m_sources.clear();
    } else {
        Q_FOREACH(const QString &source, sources) {
            m_sources.removeOne(source);
        }
    }
}

bool SyncJob::runOnPayedConnection() const
{
    return m_runOnPayedConnection;
}

bool SyncJob::operator==(const SyncJob &other) const
{
    return (m_account->id() == other.account()->id()) && compareSources(m_sources, other.sources());
}

bool SyncJob::isValid() const
{
    return ((m_account != 0) && !m_sources.isEmpty());
}

bool SyncJob::isEmpty()
{
    return m_sources.isEmpty();
}

bool SyncJob::contains(const QStringList &sources) const
{
    if (m_sources.contains(SyncJob::SyncAllKeyword)) {
        return true;
    }

    Q_FOREACH(const QString &source, sources) {
        if (!m_sources.contains(source)) {
            return false;
        }
    }

    return true;
}

bool SyncJob::contains(const QString &source) const
{
    return (m_sources.contains(SyncJob::SyncAllKeyword) ||
            m_sources.contains(source));
}

void SyncJob::clear()
{
    m_account = 0;
    m_sources.clear();
}

bool SyncJob::compareSources(const QStringList &listA, const QStringList &listB)
{
    QStringList newListA(listA);
    if (newListA.isEmpty()) {
        newListA << SyncJob::SyncAllKeyword;
    }

    QStringList newListB(listB);
    if (newListB.isEmpty()) {
        newListB << SyncJob::SyncAllKeyword;
    }

    if (newListA.size() != newListB.size())
        return false;

    Q_FOREACH(const QString &valueA, newListA) {
        if (!newListB.contains(valueA))
            return false;
    }
    return true;
}
