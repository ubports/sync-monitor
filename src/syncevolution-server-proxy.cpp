#include "syncevolution-server-proxy.h"
#include "syncevolution-session-proxy.h"

#include <QtCore/QDebug>
#include <QtDBus/QDBusReply>

#define SYNCEVOLUTION_SERVICE_NAME          "org.syncevolution"
#define SYNCEVOLUTION_OBJECT_PATH           "/org/syncevolution/Server"
#define SYNCEVOLUTION_IFACE_NAME            "org.syncevolution.Server"

SyncEvolutionServerProxy *SyncEvolutionServerProxy::m_instance = 0;

SyncEvolutionServerProxy::SyncEvolutionServerProxy(QObject *parent)
    : QObject(parent)
{
    m_iface = new QDBusInterface(SYNCEVOLUTION_SERVICE_NAME,
                                 SYNCEVOLUTION_OBJECT_PATH,
                                 SYNCEVOLUTION_IFACE_NAME);
    if (m_iface->lastError().isValid()) {
        qWarning() << "Fail to connect with syncevolution service" << m_iface->lastError().message();
    }
}

SyncEvolutionServerProxy::~SyncEvolutionServerProxy()
{
    if (m_iface) {
        m_iface->call("Detach");
        m_iface->deleteLater();
        m_iface = 0;
    }
}

SyncEvolutionServerProxy *SyncEvolutionServerProxy::instance()
{
    if (!m_instance) {
        m_instance = new SyncEvolutionServerProxy();
    }
    return m_instance;
}

void SyncEvolutionServerProxy::destroy()
{
    if (m_instance) {
        delete m_instance;
        m_instance = 0;
    }
}

SyncEvolutionSessionProxy* SyncEvolutionServerProxy::openSession(const QString &sessionName, QStringList flags)
{
    QDBusReply<QDBusObjectPath> reply;
    if (flags.isEmpty()) {
        reply = m_iface->call("StartSession", sessionName);
    } else {
        reply = m_iface->call("StartSessionWithFlags", sessionName, flags);
    }
    if (m_iface->lastError().isValid()) {
        qWarning() << "Fail to start session" << m_iface->lastError().message();
        return 0;
    }

    return new SyncEvolutionSessionProxy(reply.value(), this);
}
