#include "sync-network.h"

#include <QDebug>
#include <QTimer>

SyncNetwork::SyncNetwork(QObject *parent)
    : QObject(parent),
      m_configManager(new QNetworkConfigurationManager(this)),
      m_state(SyncNetwork::NetworkOffline)
{
    refresh();

    connect(m_configManager.data(),
            SIGNAL(onlineStateChanged(bool)),
            SLOT(refresh()));
    connect(m_configManager.data(),
            SIGNAL(configurationAdded(QNetworkConfiguration)),
            SLOT(refresh()));
    connect(m_configManager.data(),
            SIGNAL(configurationChanged(QNetworkConfiguration)),
            SLOT(refresh()));
    connect(m_configManager.data(),
            SIGNAL(configurationRemoved(QNetworkConfiguration)),
            SLOT(refresh()));
    connect(m_configManager.data(),
            SIGNAL(updateCompleted()),
            SLOT(refresh()));

    m_idleRefresh.setSingleShot(true);
    connect(&m_idleRefresh,
            SIGNAL(timeout()),
            SLOT(idleRefresh()));
}

SyncNetwork::~SyncNetwork()
{
}

SyncNetwork::NetworkState SyncNetwork::state() const
{
    return m_state;
}

void SyncNetwork::setState(SyncNetwork::NetworkState newState)
{
    if (m_state != newState) {
        m_state = newState;
        qDebug() << "Network state changed:" << (m_state == SyncNetwork::NetworkOffline ? "Offline" :
                                                 m_state == SyncNetwork::NetworkPartialOnline ? "Partial online" : "Online");
        Q_EMIT stateChanged(m_state);
    }
}

void SyncNetwork::refresh()
{
    m_idleRefresh.start(3000);
}

void SyncNetwork::idleRefresh()
{
    // Check if is online
    QList<QNetworkConfiguration> activeConfigs = m_configManager->allConfigurations(QNetworkConfiguration::Active);
    SyncNetwork::NetworkState newState = SyncNetwork::NetworkOffline;
    bool isOnline = activeConfigs.size() > 0;
    if (isOnline) {
        // Check if the connection is wifi or ethernet
        QNetworkConfiguration defaultConfig = m_configManager->defaultConfiguration();
        if (defaultConfig.isValid()) {
            if ((defaultConfig.bearerType() == QNetworkConfiguration::BearerWLAN) ||
                (defaultConfig.bearerType() == QNetworkConfiguration::BearerEthernet)) {
                newState = SyncNetwork::NetworkOnline;
            } else {
                // if the connection is not wifi or ethernet it will consider it as offline
                newState = SyncNetwork::NetworkPartialOnline;
            }
        }
        qDebug() << "New connection type:" << defaultConfig.bearerTypeName();
    } else {
        qDebug() << "Network is offline";
    }
    setState(newState);
}
