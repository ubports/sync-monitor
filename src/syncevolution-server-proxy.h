#ifndef __SYNCEVOLUTION_SERVER_PROXY_H__
#define __SYNCEVOLUTION_SERVER_PROXY_H__

#include "dbustypes.h"

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QHash>

#include <QtDBus/QDBusInterface>

class SyncEvolutionSessionProxy;

class SyncEvolutionServerProxy : public QObject
{
    Q_OBJECT
public:
    static SyncEvolutionServerProxy *instance();
    static void destroy();

    SyncEvolutionSessionProxy* openSession(const QString &sessionName, QStringList flags);

private:
    static SyncEvolutionServerProxy *m_instance;
    QDBusInterface *m_iface;

    SyncEvolutionServerProxy(QObject *parent = 0);
    ~SyncEvolutionServerProxy();
};

#endif
