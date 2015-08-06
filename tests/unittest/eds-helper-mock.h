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

#ifndef __EDS_HELPER_MOCK__
#define __EDS_HELPER_MOCK__

#include <gmock/gmock.h>

#include <QtCore/QList>
#include <QtCore/QUrl>

#include <QtContacts/QContactId>
#include <QtContacts/QContactAbstractRequest>

#include <QtOrganizer/QOrganizerItemId>

#include "src/eds-helper.h"

class EdsHelperMock : public EdsHelper
{
    Q_OBJECT
public:
    EdsHelperMock(QObject *parent = 0)
        : EdsHelper(parent, "memory", "memory")
    {
        setEnabled(true);
    }


    QtOrganizer::QOrganizerManager *organizerEngine()
    { return m_organizerEngine; }

    QtContacts::QContactManager *contactEngine()
    { return m_contactEngine; }

    void trackCollectionFromItem(QtOrganizer::QOrganizerItem *item)
    { m_trackedItem = item; }

    virtual QString getCollectionIdFromItemId(const QtOrganizer::QOrganizerItemId&) const
    { return m_trackedItem->collectionId().toString(); }

private:
    QtOrganizer::QOrganizerItem *m_trackedItem;
};

#endif
