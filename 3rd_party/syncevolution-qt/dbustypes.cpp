/*
 * Copyright (C) 2010 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "dbustypes.h"


// Marshall the SyncDatabase data into a D-BUS argument
QDBusArgument &operator<<(QDBusArgument &argument, const SyncDatabase &d)
{
    argument.beginStructure();
    argument << d.name << d.source << d.defaultCalendar << d.writable << d.color << d.title << d.remoteId;
    argument.endStructure();
    return argument;
}

// Retrieve the SyncDatabase data from the D-BUS argument
const QDBusArgument &operator>>(const QDBusArgument &argument, SyncDatabase &d)
{
    argument.beginStructure();
    argument >> d.name >> d.source >> d.defaultCalendar >> d.writable >> d.color >> d.title >> d.remoteId;
    argument.endStructure();
    return argument;
}

// Marshall the SyncProgress data into a D-BUS argument
QDBusArgument &operator<<(QDBusArgument &argument, const SyncProgress &p)
{
    argument.beginStructure();
    argument << p.prepareCount << p.prepareTotal << p.sendCount << p.sendTotal << p.recieveCount \
             << p.recieveTotal;
    argument.endStructure();
    return argument;
}

// Retrieve the SyncProgress data from the D-BUS argument
const QDBusArgument &operator>>(const QDBusArgument &argument, SyncProgress &p)
{
    argument.beginStructure();
    argument >> p.prepareCount >> p.prepareTotal >> p.sendCount >> p.sendTotal >> p.recieveCount \
             >> p.recieveTotal;
    argument.endStructure();
    return argument;
}

// Marshall the SyncStatus data into a D-BUS argument
QDBusArgument &operator<<(QDBusArgument &argument, const SyncStatus &s)
{
    argument.beginStructure();
    argument << s.mode << s.status << s.error;
    argument.endStructure();
    return argument;
}

// Retrieve the SyncStatus data from the D-BUS argument
const QDBusArgument &operator>>(const QDBusArgument &argument, SyncStatus &s)
{
    argument.beginStructure();
    argument >> s.mode >> s.status >> s.error;
    argument.endStructure();
    return argument;
}

// Marshall the SessionStatus data into a D-BUS argument
QDBusArgument &operator<<(QDBusArgument &argument, const SessionStatus &s)
{
    argument.beginStructure();
    argument << s.status << s.error << s.sources;
    argument.endStructure();
    return argument;
}

// Retrieve the SessionStatus data from the D-BUS argument
const QDBusArgument &operator>>(const QDBusArgument &argument, SessionStatus &s)
{
    if (argument.currentSignature() == "s") {
        argument >> s.status;
    } else {
        argument.beginStructure();
        argument >> s.status >> s.error >> s.sources;
        argument.endStructure();
    }
    return argument;
}

//Parse syncevolution output command into the list of databases
QArrayOfDatabases &operator<<(QArrayOfDatabases &databases, const QString &output)
{
    static QStringList colorNames;
    if (colorNames.isEmpty()) {
        colorNames << "#2C001E"
                   << "#333333"
                   << "#DD4814"
                   << "#DF382C"
                   << "#EFB73E"
                   << "#19B6EE"
                   << "#38B44A"
                   << "#001F5C";
        qsrand(colorNames.size());
    }


    QStringList lines = output.split("\n");
    while (lines.count() > 0) {
        if (lines.first().startsWith("caldav:")) {
            lines.takeFirst();
            break;
        }
        lines.takeFirst();
    }

    while (lines.count() > 0) {
        QString line = lines.takeFirst();
        if (line.isEmpty()) {
            continue;
        }

        int calendarNameEndIndex = line.lastIndexOf('(');
        int calendarUrlEndIndex = line.lastIndexOf(')');
        if ((calendarNameEndIndex == -1) ||
            (calendarUrlEndIndex == -1)) {
            continue;
        }

        SyncDatabase db;
        db.name = line.left(calendarNameEndIndex).trimmed();
        db.source = line.mid(calendarNameEndIndex + 1, calendarUrlEndIndex - calendarNameEndIndex - 1);
        db.remoteId = QUrl::fromPercentEncoding(db.source.split("/", QString::SkipEmptyParts).last().toLatin1());
        db.defaultCalendar = (line.indexOf("<default>", calendarUrlEndIndex + 1) != -1);

        //TODO: get calendar original color
        const int index = (rand() % (colorNames.size() - 1));
        db.color = colorNames.value(index, 0);

        //TODO: get calendar permissions
        db.writable = true;
        databases << db;
    }

    return databases;
}
