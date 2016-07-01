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

class SyncJob
{
public:
    SyncJob();
    SyncJob(SyncAccount *account, const QStringList &sources, bool runOnPayedConnection);

    SyncAccount *account() const;
    QStringList sources() const;
    void appendSources(const QStringList &sources);
    void removeSources(const QStringList &sources);
    bool runOnPayedConnection() const;
    bool operator==(const SyncJob &other) const;
    bool isValid() const;
    bool isEmpty();
    bool contains(const QString &source) const;
    bool contains(const QStringList &sources) const;
    bool contains(SyncAccount *account, const QStringList &sources) const;
    void clear();

private:
    static const QString SyncAllKeyword;

    SyncAccount *m_account;
    QStringList m_sources;
    bool m_runOnPayedConnection;

    static bool compareSources(const QStringList &listA, const QStringList &listB);
};

class SyncQueue
{
public:
    SyncJob popNext();

    void push(const SyncQueue &other);
    void push(const SyncJob &job);
    void push(SyncAccount *account, const QString &sourceName, bool syncOnPayedConnection);
    void push(SyncAccount *account, const QStringList &sources = QStringList(), bool syncOnPayedConnection = false);

    bool contains(const SyncJob &otherJob) const;
    bool contains(SyncAccount *account, const QString &sourceName) const;
    bool contains(SyncAccount *account, const QStringList &sources) const;

    void remove(const SyncJob &job);
    void remove(SyncAccount *account, const QString &source);
    void remove(SyncAccount *account, const QStringList &sources = QStringList());

    int count() const;
    bool isEmpty() const;
    void clear();
    const QList<SyncJob> jobs() const;

private:
    QList<SyncJob> m_jobs;
};



#endif

