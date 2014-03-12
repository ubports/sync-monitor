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
#include <QtCore/QSet>
#include <QtCore/QMap>

class SyncAccount;

class SyncQueue
{
public:
    SyncQueue();
    ~SyncQueue();

    void push(SyncAccount *account, const QString &serviceName = QString());
    QString popNext(SyncAccount **account);
    SyncAccount *popNext();
    void remove(SyncAccount *account, const QString &serviceName = QString());

    bool contains(SyncAccount *account, const QString &serviceName) const;
    int count() const;
    bool isEmpty() const;

private:
    QMap<SyncAccount*, QSet<QString> > m_queue;
};

#endif

