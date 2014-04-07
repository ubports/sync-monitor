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

#include <QtOrganizer/QOrganizerManager>
#include <QtContacts/QContactManager>
#include <QtContacts/QContactAbstractRequest>

#include <QtDBus/QDBusInterface>

class EdsHelper : public QObject
{
    Q_OBJECT
public:
    EdsHelper(QObject *parent = 0);
    ~EdsHelper();
    void createSource(const QString &serviceName, const QString &sourceName);

Q_SIGNALS:
    void dataChanged(const QString &serviceName, const QString &sourceName);

private Q_SLOTS:
    void contactChangedFilter(const QList<QtContacts::QContactId>& contactIds);
    void contactChanged();
    void contactDataChanged();
    void calendarChanged();
    void contactFetchStateChanged(QtContacts::QContactAbstractRequest::State newState);

private:
    QtOrganizer::QOrganizerManager *m_organizerEngine;
    QtContacts::QContactManager *m_contactEngine;
    QTimer m_timeoutTimer;

    void createOrganizerSource(const QString &sourceName);
    void createContactsSource(const QString &sourceName);
};

#endif
