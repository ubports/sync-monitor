/*
 * Copyright 2016 Canonical Ltd.
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

#include "3rd_party/syncevolution-qt/dbustypes.h"

#include <QObject>
#include <QtTest>
#include <QDebug>


class SyncEvolutionOutputParser : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void testParseEmptyString()
    {
        QArrayOfDatabases dbs;
        dbs << "";
        QCOMPARE(dbs.count(), 0);
    }

    void testParseInvalidString()
    {
        QArrayOfDatabases dbs;
        dbs << "<invalid>";
        QCOMPARE(dbs.count(), 0);
    }

    void testParseIncompleteString()
    {
        QArrayOfDatabases dbs;
        QStringList output;
        output  << QStringLiteral("syncURL='https://owncloud/remote.php/dav'")
                << QStringLiteral("caldav:")
                << QStringLiteral(" calendar (invalid line")
                << QStringLiteral(" calendar2");
        dbs << output.join('\n');
        QCOMPARE(dbs.count(), 0);
    }

    void testParseSingleCalendar()
    {
        QArrayOfDatabases dbs;
        QStringList output;
        output  << QStringLiteral("syncURL='https://owncloud/remote.php/dav'")
                << QStringLiteral("caldav:")
                << QStringLiteral(" calendar name (https://owncloud:444/remote.php/dav/calendar_1)");
        dbs << output.join('\n');
        QCOMPARE(dbs.count(), 1);
        SyncDatabase db = dbs.at(0);

        QCOMPARE(db.name, QStringLiteral("calendar name"));
        QCOMPARE(db.defaultCalendar, false);
        QCOMPARE(db.remoteId, QStringLiteral("calendar_1"));
        QCOMPARE(db.source, QStringLiteral("https://owncloud:444/remote.php/dav/calendar_1"));
        QVERIFY(db.title.isEmpty());
        QCOMPARE(db.writable, true);
    }

    void testParseCalendarWithParens()
    {
        QArrayOfDatabases dbs;
        QStringList output;
        output  << QStringLiteral("syncURL='https://owncloud/remote.php/dav'")
                << QStringLiteral("caldav:")
                << QStringLiteral(" calendar name (subtitle) (https://owncloud:444/remote.php/dav/calendar_1)");
        dbs << output.join('\n');
        QCOMPARE(dbs.count(), 1);
        SyncDatabase db = dbs.at(0);

        QCOMPARE(db.name, QStringLiteral("calendar name (subtitle)"));
        QCOMPARE(db.defaultCalendar, false);
        QCOMPARE(db.remoteId, QStringLiteral("calendar_1"));
        QCOMPARE(db.source, QStringLiteral("https://owncloud:444/remote.php/dav/calendar_1"));
        QVERIFY(db.title.isEmpty());
        QCOMPARE(db.writable, true);
    }


    void testParseMultipleCalendars()
    {
        QArrayOfDatabases dbs;
        QStringList output;
        output  << QStringLiteral("syncURL='https://owncloud/remote.php/dav'")
                << QStringLiteral("caldav:")
                << QStringLiteral(" calendar name (subtitle) (https://owncloud:444/remote.php/dav/calendar_1)")
                << QStringLiteral(" calendar não (https://owncloud:444/remote.php/dav/calendar_2)")
                << QStringLiteral(" calendar_name_* (https://owncloud:444/remote.php/dav/calendar_3)");
        dbs << output.join('\n');
        QCOMPARE(dbs.count(), 3);
        SyncDatabase db = dbs.at(0);

        QCOMPARE(db.name, QStringLiteral("calendar name (subtitle)"));
        QCOMPARE(db.defaultCalendar, false);
        QCOMPARE(db.remoteId, QStringLiteral("calendar_1"));
        QCOMPARE(db.source, QStringLiteral("https://owncloud:444/remote.php/dav/calendar_1"));
        QVERIFY(db.title.isEmpty());
        QCOMPARE(db.writable, true);

        db = dbs.at(1);
        QCOMPARE(db.name, QStringLiteral("calendar não"));
        QCOMPARE(db.defaultCalendar, false);
        QCOMPARE(db.remoteId, QStringLiteral("calendar_2"));
        QCOMPARE(db.source, QStringLiteral("https://owncloud:444/remote.php/dav/calendar_2"));
        QVERIFY(db.title.isEmpty());
        QCOMPARE(db.writable, true);

        db = dbs.at(2);
        QCOMPARE(db.name, QStringLiteral("calendar_name_*"));
        QCOMPARE(db.defaultCalendar, false);
        QCOMPARE(db.remoteId, QStringLiteral("calendar_3"));
        QCOMPARE(db.source, QStringLiteral("https://owncloud:444/remote.php/dav/calendar_3"));
        QVERIFY(db.title.isEmpty());
        QCOMPARE(db.writable, true);
    }

    void testParseDefaultCalendars()
    {
        QArrayOfDatabases dbs;
        QStringList output;
        output  << QStringLiteral("syncURL='https://owncloud/remote.php/dav'")
                << QStringLiteral("caldav:")
                << QStringLiteral(" calendar name (subtitle) (https://owncloud:444/remote.php/dav/calendar_1)")
                << QStringLiteral(" calendar não (https://owncloud:444/remote.php/dav/calendar_2) <default>")
                << QStringLiteral(" calendar_name_* (https://owncloud:444/remote.php/dav/calendar_3)");
        dbs << output.join('\n');
        QCOMPARE(dbs.count(), 3);
        SyncDatabase db = dbs.at(0);

        QCOMPARE(db.name, QStringLiteral("calendar name (subtitle)"));
        QCOMPARE(db.defaultCalendar, false);
        QCOMPARE(db.remoteId, QStringLiteral("calendar_1"));
        QCOMPARE(db.source, QStringLiteral("https://owncloud:444/remote.php/dav/calendar_1"));
        QVERIFY(db.title.isEmpty());
        QCOMPARE(db.writable, true);

        db = dbs.at(1);
        QCOMPARE(db.name, QStringLiteral("calendar não"));
        QCOMPARE(db.defaultCalendar, true);
        QCOMPARE(db.remoteId, QStringLiteral("calendar_2"));
        QCOMPARE(db.source, QStringLiteral("https://owncloud:444/remote.php/dav/calendar_2"));
        QVERIFY(db.title.isEmpty());
        QCOMPARE(db.writable, true);

        db = dbs.at(2);
        QCOMPARE(db.name, QStringLiteral("calendar_name_*"));
        QCOMPARE(db.defaultCalendar, false);
        QCOMPARE(db.remoteId, QStringLiteral("calendar_3"));
        QCOMPARE(db.source, QStringLiteral("https://owncloud:444/remote.php/dav/calendar_3"));
        QVERIFY(db.title.isEmpty());
        QCOMPARE(db.writable, true);
    }
};

QTEST_MAIN(SyncEvolutionOutputParser)
#include "syncevolution-output-parser.moc"
