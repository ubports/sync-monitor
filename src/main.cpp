#include "sync-daemon.h"
#include "dbustypes.h"

#include <QtCore/QCoreApplication>

int main(int argc, char** argv)
{
    // register all syncevolution dbus types
    syncevolution_qt_dbus_register_types();

    QCoreApplication app(argc, argv);
    app.setApplicationName("Synq");
    SyncDaemon *daemon = new SyncDaemon();
    daemon->connect(&app, SIGNAL(aboutToQuit()), SLOT(quit()));
    daemon->run();
    return app.exec();
}
