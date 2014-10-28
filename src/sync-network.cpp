#include "sync-network.h"

#include <QDebug>

SyncNetwork::SyncNetwork(QObject *parent)
    : QObject(parent),
      m_configManager(new QNetworkConfigurationManager(this)),
      m_isOnline(false)
{
    refresh();
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
    bool isOnline = m_configManager->isOnline();
    qDebug() << "Refresh" << isOnline;

    // Check if is connected in a wifi or network
    QList<QNetworkConfiguration> activeConfigs = m_configManager->allConfigurations(QNetworkConfiguration::Active);
    Q_FOREACH(const QNetworkConfiguration &config, activeConfigs) {
        qDebug() << "Active beareType:" << config.bearerType();
        if ((config.bearerType() > 0) && (config.bearerType() <= QNetworkConfiguration::BearerWLAN)) {
            isOnline = true;
        } else {
            // if the connection is not wifi or ethernet it will consider it as offline
            isOnline = false;
        }
    }

    if (m_isOnline != isOnline) {
        m_isOnline = isOnline;
        qDebug() << "Network is online" << m_isOnline;
        Q_EMIT onlineChanged(m_isOnline);
    }
}
