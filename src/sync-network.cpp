#include "sync-network.h"

#include <QDebug>

SyncNetwork::SyncNetwork(QObject *parent)
    : QObject(parent),
      m_configManager(new QNetworkConfigurationManager(this)),
      m_isOnline(false)
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

bool SyncNetwork::isOnline() const
{
    return m_isOnline;
}

void SyncNetwork::refresh()
{
    // Check if is online
    QList<QNetworkConfiguration> activeConfigs = m_configManager->allConfigurations(QNetworkConfiguration::Active);
    bool isOnline = activeConfigs.size() > 0;
    if (isOnline) {
        // Check if the connection is wifi or ethernet
        QNetworkConfiguration defaultConfig = m_configManager->defaultConfiguration();
        if ((defaultConfig.bearerType() > 0) &&
            (defaultConfig.bearerType() <= QNetworkConfiguration::BearerWLAN)) {
            isOnline = true;
        } else {
            // if the connection is not wifi or ethernet it will consider it as offline
            isOnline = false;
            qDebug() << "Device is online but the current connection is not wifi:" << defaultConfig.bearerTypeName();
        }
    }

    if (m_isOnline != isOnline) {
        m_isOnline = isOnline;
        qDebug() << "Network state changed:" << (m_isOnline ? "Online" : "Offline");
        Q_EMIT onlineChanged(m_isOnline);
    }
}
