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

#ifndef __EDS_HELPER_H__
#define __EDS_HELPER_H__

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtCore/QSet>

#include <QtOrganizer/QOrganizerManager>
#include <QtContacts/QContactManager>
#include <QtContacts/QContactAbstractRequest>

#include <QtDBus/QDBusInterface>

// necessary for singna/slot signatures;
using namespace QtContacts;
using namespace QtOrganizer;

class EdsSource
{
public:
    QString id;
    QString name;
    uint account;
    QString remoteId;

    bool isValid()
    {
        return !id.isEmpty();
    }
};

class EdsHelper : public QObject
{
    Q_OBJECT
public:
    EdsHelper(QObject *parent = 0, const QString &organizerManager = "eds");
    ~EdsHelper();
    QString createSource(const QString &sourceName,
                         const QString &sourceColor,
                         const QString &sourceRemoteUrl, bool writable,
                         int accountId);
    void removeSource(const QString &sourceId);
    QString sourceIdByName(const QString &sourceName, uint account);
    EdsSource sourceByRemoteId(const QString &remoteId, uint account);
    EdsSource sourceById(const QString &id);

    void freezeNotify();
    void unfreezeNotify();
    void flush();
    void setEnabled(bool enabled);
    QMap<int, QStringList> sources();

Q_SIGNALS:
    void dataChanged(const QString &sourceId);

private Q_SLOTS:
    void calendarChanged(const QList<QOrganizerItemId> &itemIds);

protected:
    QtOrganizer::QOrganizerManager *m_organizerEngine;

    virtual QString getCollectionIdFromItemId(const QtOrganizer::QOrganizerItemId &itemId) const;

private:
    QTimer m_timeoutTimer;
    bool m_freezed;

    // late notify
    QSet<QString> m_pendingCalendars;
};

#endif
