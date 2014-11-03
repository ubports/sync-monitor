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

#include "sync-configure.h"
#include "syncevolution-server-proxy.h"
#include "syncevolution-session-proxy.h"

#include "config.h"

using namespace Accounts;

SyncConfigure::SyncConfigure(Accounts::Account *account,
                             QSettings *settings,
                             QObject *parent)
    : QObject(parent),
      m_account(account),
      m_settings(settings)
{
}

SyncConfigure::~SyncConfigure()
{
    Q_ASSERT(m_sessions.size() == 0);
}

void SyncConfigure::configure(const QString &serviceName, const QString &syncMode)
{
    m_syncMode = syncMode;
    m_originalServiceName = serviceName;
    if (serviceName.isEmpty()) {
        configureAll(syncMode);
    } else {
        m_services << serviceName;
        configureServices(syncMode);
    }

}

void SyncConfigure::configureAll(const QString &syncMode)
{
    Q_FOREACH(Service service, m_account->services()) {
        m_services << service.serviceType();
    }
    configureServices(syncMode);
}

QString SyncConfigure::serviceName() const
{
    return m_originalServiceName;
}

void SyncConfigure::configureServices(const QString &syncMode)
{
    SyncEvolutionServerProxy *proxy = SyncEvolutionServerProxy::instance();
    qDebug() << "start configure for services" << m_services;

    QStringList pendingServices = m_services;
    Q_FOREACH(QString serviceName, pendingServices) {
        QString sessionName = QString("%1-%2-%3")
                .arg(m_account->providerName())
                .arg(serviceName)
                .arg(m_account->id());

        SyncEvolutionSessionProxy *session = proxy->openSession(sessionName,
                                                                QStringList() << "all-configs");
        m_sessions.insert(serviceName, session);

        connect(session, &SyncEvolutionSessionProxy::statusChanged,
            this, &SyncConfigure::onSessionStatusChanged);
        connect(session, &SyncEvolutionSessionProxy::error,
            this, &SyncConfigure::onSessionError);

        qDebug() << "\tconfig session created" << sessionName << session->status();
        if (session->status() != "queueing") {
            configureService(serviceName, syncMode);
        }
    }
}

void SyncConfigure::configureService(const QString &serviceName, const QString &syncMode)
{
    SyncEvolutionServerProxy *proxy = SyncEvolutionServerProxy::instance();
    QStringList configs = proxy->configs();

    QString targetSuffix = QString("%1-%2-%3")
            .arg(m_account->providerName())
            .arg(serviceName)
            .arg(m_account->id());
    QString targetConfigName = QString("target-config@%1").arg(targetSuffix);

    bool isConfigured = true;
    if (!configs.contains(targetConfigName)) {
        qDebug() << "\tCreate target:" << targetConfigName;
        isConfigured = configTarget(targetConfigName, serviceName);
    }

    if (isConfigured && !configs.contains(targetSuffix)) {
        qDebug() << "\tCreate sync config:" << targetSuffix;
        isConfigured = configSync(targetSuffix, serviceName, syncMode);
    } else if (isConfigured) {
        isConfigured = changeSyncMode(targetSuffix, serviceName, syncMode);
    }

    if (!isConfigured) {
        qWarning() << "Fail to configure account:" << m_account->displayName() << m_account->id() << serviceName;
    }

    removeService(serviceName);
}

void SyncConfigure::removeService(const QString &serviceName)
{
    m_services.removeOne(serviceName);

    SyncEvolutionSessionProxy *session = m_sessions.take(serviceName);
    session->destroy();
    delete session;

    if (m_services.isEmpty()) {
        qDebug() << "\taccount config done" << m_account->displayName() << serviceName;
        Q_EMIT done();
    }
}

bool SyncConfigure::configTarget(const QString &targetName, const QString &serviceName)
{
    AccountId accountId = m_account->id();
    SyncEvolutionSessionProxy *session = m_sessions.value(serviceName, 0);

    // loas settings
    m_settings->beginGroup(serviceName);
    QString templateName = m_settings->value("template", "SyncEvolution").toString();
    QString syncUrl = m_settings->value("syncURL", QString(QString::null)).toString();
    QString uoaServiceName = m_settings->value("uoa-service", "").toString();
    m_settings->endGroup();

    // config server side
    Q_ASSERT(!templateName.isEmpty());
    QStringMultiMap config = session->getConfig(templateName, true);
    if (!syncUrl.isNull()) {
        config[""]["syncURL"] = syncUrl;
    }
    config[""]["username"] = QString("uoa:%1,%2").arg(accountId).arg(uoaServiceName);
    config[""]["consumerReady"] = "0";
    config[""]["dumpData"] = "0";
    config[""]["printChanges"] = "0";
    config[""]["maxlogdirs"] = "2";

    QString expectedSource;
    if (serviceName == CONTACTS_SERVICE_NAME) {
        expectedSource = QString("source/addressbook");
    } else if (serviceName == CALENDAR_SERVICE_NAME) {
        expectedSource = QString("source/calendar");
    }

    // remove any extra source
    QStringList keys = config.keys();
    Q_FOREACH(const QString &key, keys) {
        if ((key != expectedSource) && key.startsWith("source/")) {
            config.remove(key);
        }
    }

    bool result = session->saveConfig(targetName, config);
    if (!result) {
        qWarning() << "Fail to save account client config";
        return false;
    }
    return true;
}

bool SyncConfigure::changeSyncMode(const QString &targetName, const QString &serviceName, const QString &syncMode)
{
    SyncEvolutionSessionProxy *session = m_sessions.value(serviceName, 0);
    QStringMultiMap config = session->getConfig(targetName, false);
    Q_ASSERT(!config.isEmpty());

    AccountId accountId = m_account->id();
    QString sourceName = QString("%1_uoa_%2").arg(serviceName).arg(accountId);
    QString sourceFullName = QString("source/%1").arg(sourceName);

    config[sourceFullName]["sync"] = syncMode;

    bool result = session->saveConfig(targetName, config);
    if (!result) {
        qWarning() << "Fail to save account client config";
        return false;
    }
    return result;
}

bool SyncConfigure::configSync(const QString &targetName, const QString &serviceName, const QString &syncMode)
{
    AccountId accountId = m_account->id();
    SyncEvolutionSessionProxy *session = m_sessions.value(serviceName, 0);

    m_settings->beginGroup(serviceName);
    QString clientBackend = m_settings->value("sync-backend", QString(QString::null)).toString();
    QString clientUri = m_settings->value("sync-uri", QString(QString::null)).toString();
    m_settings->endGroup();

    QStringMultiMap config = session->getConfig("SyncEvolution_Client", true);
    Q_ASSERT(!config.isEmpty());
    config[""]["syncURL"] = QString("local://@%1").arg(targetName);
    config[""]["username"] = QString();
    config[""]["password"] = QString();
    config[""]["dumpData"] = "0";
    config[""]["printChanges"] = "0";
    config[""]["maxlogdirs"] = "2";


    QString expectedSource;
    if (serviceName == CONTACTS_SERVICE_NAME) {
        expectedSource = QString("source/addressbook");
    } else if (serviceName == CALENDAR_SERVICE_NAME) {
        expectedSource = QString("source/calendar");
    }

    QStringMap newConfig = config.take(expectedSource);

    // remove any extra source
    QStringList keys = config.keys();
    Q_FOREACH(const QString &key, keys) {
        if (key.startsWith("source/")) {
            config.remove(key);
        }
    }

    // database
    newConfig["database"] = m_account->displayName();
    if (!clientBackend.isNull()) {
        newConfig["backend"] = clientBackend;
    }
    //TODO: create one for each database
    if (!clientUri.isNull()) {
        newConfig["uri"] = clientUri;
    }
    newConfig["sync"] = syncMode;

    // insert new source
    QString sourceName = QString("source/%1_uoa_%2").arg(serviceName).arg(accountId);
    config.insert(sourceName, newConfig);

    bool result = session->saveConfig(targetName, config);
    if (!result) {
        qWarning() << "Fail to save account client config";
        return false;
    }
    return result;
}

void SyncConfigure::onSessionStatusChanged(const QString &newStatus)
{
    SyncEvolutionSessionProxy *session = qobject_cast<SyncEvolutionSessionProxy*>(QObject::sender());
    if (newStatus != "queueing") {
        configureService(m_sessions.key(session), m_syncMode);
    }
}

void SyncConfigure::onSessionError(uint errorCode)
{
    SyncEvolutionSessionProxy *session = qobject_cast<SyncEvolutionSessionProxy*>(QObject::sender());

    QString serviceName = m_sessions.key(session);
    removeService(serviceName);

    Q_UNUSED(errorCode);
    Q_EMIT error();
}
