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
    SyncJob(SyncAccount *account, const QString &serviceName, bool runOnPayedConnection);

    SyncAccount *account() const;
    QString serviceName() const;
    bool runOnPayedConnection() const;
    bool operator==(const SyncJob &other) const;
    bool isValid() const;

private:
    SyncAccount *m_account;
    QString m_serviceName;
    bool m_runOnPayedConnection;
};

class SyncQueue
{
public:
    SyncJob popNext();

    void push(const SyncQueue &other);
    void push(const SyncJob &job);
    void push(SyncAccount *account, const QString &serviceName, bool syncOnPayedConnection);
    void push(SyncAccount *account, const QStringList &serviceNames = QStringList(), bool syncOnPayedConnection = false);

    bool contains(const SyncJob &otherJob) const;
    bool contains(SyncAccount *account, const QString &serviceName) const;

    void remove(SyncAccount *account, const QString &serviceName = QString());

    int count() const;
    bool isEmpty() const;
    void clear();
    QList<SyncJob> jobs() const;

private:
    QList<SyncJob> m_jobs;
};



#endif

