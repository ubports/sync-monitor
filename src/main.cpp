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

#include "sync-daemon.h"
#include "dbustypes.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QTimer>

int main(int argc, char** argv)
{
    // register all syncevolution dbus types
    syncevolution_qt_dbus_register_types();

    QCoreApplication app(argc, argv);
    app.setApplicationName("Synq");
    SyncDaemon *daemon = new SyncDaemon();
    qputenv("QORGANIZER_EDS_DEBUG", "on");
    daemon->connect(&app, SIGNAL(aboutToQuit()), SLOT(quit()));
    daemon->run();

    if ((argc == 2) && (strcmp(argv[1], "--sync") == 0)) {
        qDebug() << "Start manual sync";
        QTimer::singleShot(1000, daemon, SLOT(syncAll()));
    }
    return app.exec();
}
