#include "sync-network.h"

#include <QDebug>

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
}

SyncNetwork::~SyncNetwork()
{
}

SyncNetwork::NetworkState SyncNetwork::state() const
{
    return m_state;
}

void SyncNetwork::refresh()
{
    // Check if is online
    QList<QNetworkConfiguration> activeConfigs = m_configManager->allConfigurations(QNetworkConfiguration::Active);
    bool isOnline = activeConfigs.size() > 0;
    SyncNetwork::NetworkState newState = SyncNetwork::NetworkOffline;
    if (isOnline) {
        // Check if the connection is wifi or ethernet
        QNetworkConfiguration defaultConfig = m_configManager->defaultConfiguration();
        if ((defaultConfig.bearerType() > 0) &&
            (defaultConfig.bearerType() <= QNetworkConfiguration::BearerWLAN)) {
            newState = SyncNetwork::NetworkOnline;
        } else {
            // if the connection is not wifi or ethernet it will consider it as offline
            newState = SyncNetwork::NetworkPartialOnline;
            qDebug() << "Device is online but the current connection is not wifi:" << defaultConfig.bearerTypeName();
        }
    }

    if (m_state != newState) {
        m_state = newState;
        qDebug() << "Network state changed:" << (newState == SyncNetwork::NetworkOffline ? "Offline" :
                                                 newState == SyncNetwork::NetworkPartialOnline ? "Partial online" : "Online");
        Q_EMIT stateChanged(m_state);
    }
}
