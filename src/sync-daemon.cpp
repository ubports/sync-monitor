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

#include "config.h"
#include "sync-configure.h"
#include "sync-daemon.h"
#include "sync-account.h"
#include "sync-queue.h"
#include "sync-dbus.h"
#include "sync-i18n.h"
#include "eds-helper.h"
#include "notify-message.h"
#include "provider-template.h"
#include "sync-network.h"
#include "syncevolution-server-proxy.h"
#include "powerd-proxy.h"

#include <QtCore/QDebug>
#include <QtCore/QTimer>

#include <url-dispatcher.h>

using namespace Accounts;


#define DAEMON_SYNC_TIMEOUT         1000 * 60 // one minute
#define SYNC_MONITOR_ICON_PATH      "/usr/share/icons/ubuntu-mobile/actions/scalable/reload.svg"
#define SYNC_ON_MOBILE_CONFIG_KEY   "sync-on-mobile-connection"


SyncDaemon::SyncDaemon()
    : QObject(0),
      m_manager(0),
      m_eds(0),
      m_dbusAddaptor(0),
      m_syncing(false),
      m_aboutToQuit(false),
      m_firstClient(true)
{
    m_provider = new ProviderTemplate();
    m_provider->load();

    m_syncQueue = new SyncQueue();
    m_offlineQueue = new SyncQueue();
    m_networkStatus = new SyncNetwork(this);
    connect(m_networkStatus, SIGNAL(stateChanged(SyncNetwork::NetworkState)), SLOT(onOnlineStatusChanged(SyncNetwork::NetworkState)));

    m_powerd = new PowerdProxy(this);
    connect(this, SIGNAL(syncAboutToStart()), m_powerd, SLOT(lock()));
    connect(this, SIGNAL(done()), m_powerd, SLOT(unlock()));

    m_timeout = new QTimer(this);
    m_timeout->setInterval(DAEMON_SYNC_TIMEOUT);
    m_timeout->setSingleShot(true);
    connect(m_timeout, SIGNAL(timeout()), SLOT(continueSync()));
}

SyncDaemon::~SyncDaemon()
{
    quit();
    delete m_timeout;
    delete m_syncQueue;
    delete m_offlineQueue;
    delete m_networkStatus;
    delete m_powerd;
}

void SyncDaemon::setupAccounts()
{
    if (m_manager) {
        return;
    }
    qDebug() << "Loading accounts...";

    m_manager = new Manager(this);
    Q_FOREACH(const AccountId &accountId, m_manager->accountList()) {
        addAccount(accountId, false);
    }

    qDebug() << "Remove old config...";
    QSettings settings;
    const QString configVersionKey("config/version");
    int configVersion = settings.value(configVersionKey, 0).toInt();
    if (configVersion < 1) {
        Q_FOREACH(const SyncAccount *acc, m_accounts.values()) {
            qDebug() << "\tTry to remove old account config" << acc->displayName();
            acc->removeOldConfig();
        }
        settings.setValue(configVersionKey, 1);
        settings.sync();
    }

    connect(m_manager,
            SIGNAL(accountCreated(Accounts::AccountId)),
            SLOT(addAccount(Accounts::AccountId)));
    connect(m_manager,
            SIGNAL(accountRemoved(Accounts::AccountId)),
            SLOT(removeAccount(Accounts::AccountId)));
    Q_EMIT accountsChanged();
}

void SyncDaemon::setupTriggers()
{
    m_eds = new EdsHelper(this);
    connect(m_eds, &EdsHelper::dataChanged,
            this, &SyncDaemon::onDataChanged);
}

void SyncDaemon::cleanupConfig()
{
    QList<int> accountIds;
    Q_FOREACH(const SyncAccount *acc, m_accounts) {
        accountIds << acc->id();
    }

    QString configPath = QString("%1/")
            .arg(QStandardPaths::locate(QStandardPaths::ConfigLocation,
                                        QStringLiteral("syncevolution"),
                                        QStandardPaths::LocateDirectory));
    QDir configDir(configPath);
    configDir.setNameFilters(QStringList() << "*-*");
    Q_FOREACH(const QString &dir, configDir.entryList()) {
        QString accountId = dir.split("-").last();
        bool ok = false;
        uint id = accountId.toUInt(&ok);
        if (ok) {
            if (!accountIds.contains(id)) {
                SyncConfigure::removeAccountConfig(id);
            }
        }
    }
}

void SyncDaemon::onDataChanged(const QString &sourceId)
{
    if (sourceId.isEmpty()) {
        syncAll(false, false);
    } else {
        EdsSource eSource = m_eds->sourceById(sourceId);

        // LEGACY: sources has the same display name and account id = 0
        if (eSource.account == 0 && !eSource.name.isEmpty()) {
            Q_FOREACH(SyncAccount *acc, m_accounts.values()) {
                if (acc->displayName() == eSource.name) {
                    eSource.account = acc->id();
                    break;
                }
            }
        }

        if (!eSource.remoteId.isEmpty()) {
            syncAccount(eSource.account, QStringList() << eSource.remoteId, false, false);
        }
    }
}

void SyncDaemon::onClientAttached()
{
    if (m_firstClient) {
        m_firstClient = false;
        // accept eds changes
        qDebug() << "First client connected, will auto-sync on next EDS change";
        m_eds->setEnabled(true);
     }
}

void SyncDaemon::onOnlineStatusChanged(SyncNetwork::NetworkState state)
{
    Q_EMIT isOnlineChanged(state != SyncNetwork::NetworkOffline);
    if (state == SyncNetwork::NetworkOnline) {
        qDebug() << "Device is online sync pending changes" << m_offlineQueue->count();
        m_syncQueue->push(*m_offlineQueue);
        m_offlineQueue->clear();
        if (!m_syncing && !m_syncQueue->isEmpty()) {
            qDebug() << "Will sync in" << DAEMON_SYNC_TIMEOUT / 1000 << "secs;";
            m_syncing = true;
            m_timeout->start();
        } else {
            qDebug() << "No change to sync";
        }
    } else if (state == SyncNetwork::NetworkOffline) {
        qDebug() << "Device is offline cancel active syncs. There is a sync in progress?" << (m_currentJob.isValid() ? "Yes" : "No");
        if (m_currentJob.isValid()) {
            if (m_currentJob.account()->retrySync()) {
                qDebug() << "Push sync to later sync";
                m_offlineQueue->push(m_currentJob);
            } else {
                 qDebug() << "Do not try re-sync the account";
            }
            m_currentJob.account()->cancel();
            m_currentJob = SyncJob();
        }
        if (m_timeout->isActive()) {
            m_timeout->stop();
        }
        continueSync();
    }
    // make accounts available or not based on online status
    Q_EMIT accountsChanged();
}

void SyncDaemon::syncAll(bool runNow, bool syncOnMobile)
{
    Q_FOREACH(SyncAccount *acc, m_accounts.values()) {
        sync(acc, QStringList(), runNow, syncOnMobile);
    }
}

void SyncDaemon::syncAccount(quint32 accountId, const QStringList &calendars, bool runNow, bool syncOnMobile)
{
    SyncAccount *acc = m_accounts.value(accountId);
    if (acc) {
        sync(acc, calendars, runNow, syncOnMobile);
    } else {
        qWarning() << "Sync account requested with invalid account id:" << accountId;
    }
}

void SyncDaemon::cancel(quint32 accountId, const QStringList &sourceNames)
{
    SyncAccount *acc = m_accounts.value(accountId);
    if ((accountId == 0) || acc) {
        cancel(acc, sourceNames);
    } else {
        qWarning() << "Cancel sync requested with invalid account id:" << accountId;
    }
}

void SyncDaemon::continueSync()
{
    SyncJob newJob = m_syncQueue->popNext();
    SyncNetwork::NetworkState netState = m_networkStatus->state();
    const bool isOnLine = (netState == SyncNetwork::NetworkOnline) ||
                          (netState != SyncNetwork::NetworkOffline && newJob.runOnPayedConnection());
    const bool continueSync = newJob.isValid() && isOnLine;
    if (!continueSync) {
        if (!isOnLine) {
            qDebug() << "Device is offline we will skip the sync.";
            Q_FOREACH(const SyncJob &j, m_syncQueue->jobs()) {
                if (j.account() && j.account()->retrySync()) {
                    qDebug() << "Push account to later sync";
                    m_offlineQueue->push(j);
                }
            }
            m_syncQueue->clear();
        } else {
            Q_ASSERT(m_syncQueue->count() == 0);
            qDebug() << "No more job to sync.";
        }
        syncFinishedImpl();
        return;
    }
    m_syncing = true;

    // flush any change in EDS
    m_eds->flush();

    // freeze notifications during the sync, to save some CPU
    m_eds->freezeNotify();

    // sync the next service on the queue
    if (!m_aboutToQuit && newJob.isValid()) {
        m_currentJob = newJob;
    } else {
        m_currentJob = SyncJob();
    }

    if (m_currentJob.isValid()) {
        // remove sync reqeust from offline queue
        m_offlineQueue->remove(m_currentJob);
        Q_EMIT syncAboutToStart();
        m_currentJob.account()->sync(m_currentJob.sources());
    } else {
        syncFinishedImpl();
    }
}

bool SyncDaemon::registerService()
{
    if (!m_dbusAddaptor) {
        QDBusConnection connection = QDBusConnection::sessionBus();
        if (connection.interface()->isServiceRegistered(SYNCMONITOR_SERVICE_NAME)) {
            qWarning() << "SyncMonitor service already registered";
            return false;
        } else if (!connection.registerService(SYNCMONITOR_SERVICE_NAME)) {
            qWarning() << "Could not register service!" << SYNCMONITOR_SERVICE_NAME;
            return false;
        }

        m_dbusAddaptor = new SyncDBus(connection, this);
        if (!connection.registerObject(SYNCMONITOR_OBJECT_PATH, this)) {
            qWarning() << "Could not register object!" << SYNCMONITOR_OBJECT_PATH;
            delete m_dbusAddaptor;
            m_dbusAddaptor = 0;
            return false;
        }
        connect(m_dbusAddaptor, SIGNAL(clientAttached(int)), SLOT(onClientAttached()));
    }
    return true;
}

void SyncDaemon::syncFinishedImpl()
{
    // The sync has done, unblock notifications
    m_eds->unfreezeNotify();

    m_timeout->stop();
    m_currentJob.clear();
    m_syncing = false;
    Q_EMIT done();
}

void SyncDaemon::saveSyncResult(uint accountId, const QString &sourceName, const QString &result, const QString &date)
{
    static QStringList okStatus;

     if (okStatus.isEmpty()) {
         okStatus << "0"
                  << "200"
                  << "204"
                  << "207";
     }

    const QString logKey = QString(ACCOUNT_LOG_GROUP_FORMAT).arg(accountId).arg(sourceName);
    m_settings.setValue(logKey + ACCOUNT_LOG_LAST_SYNC_RESULT, result);
    m_settings.setValue(logKey + ACCOUNT_LOG_LAST_SYNC_DATE, date);
    if (okStatus.contains(result)) {
        m_settings.setValue(logKey + ACCOUNT_LOG_LAST_SUCCESSFUL_DATE, date);
    }
    m_settings.sync();
}

void SyncDaemon::clearResultForSource(uint accountId, const QString &sourceName)
{
    const QString logKey = QString(ACCOUNT_LOG_GROUP_FORMAT).arg(accountId).arg(sourceName);
    m_settings.remove(logKey);
    m_settings.sync();
}

QString SyncDaemon::loadSyncResult(uint accountId, const QString &sourceName)
{
    const QString logKey = QString(ACCOUNT_LOG_GROUP_FORMAT).arg(accountId).arg(sourceName);
    return m_settings.value(logKey + ACCOUNT_LOG_LAST_SYNC_RESULT).toString();
}

bool SyncDaemon::isFirstSync(uint accountId)
{
    // check if there is a sync log for this account before
    const QString accountGroupPrefix = QString("account_%1").arg(accountId);
    Q_FOREACH(const QString &group, m_settings.childGroups()) {
        if (group.startsWith(accountGroupPrefix)) {
            return false;
        }
    }
    return true;
}

QString SyncDaemon::lastSuccessfulSyncDate(quint32 accountId, const QString &calendarId)
{
    const QString sourceName = SyncConfigure::formatSourceName(accountId, calendarId);
    const QString configKey = QString(ACCOUNT_LOG_GROUP_FORMAT).arg(accountId).arg(sourceName);

    return m_settings.value(configKey + ACCOUNT_LOG_LAST_SUCCESSFUL_DATE).toString();
}

void SyncDaemon::cleanupLogs()
{
    QList<int> accountIds;
    Q_FOREACH(const SyncAccount *acc, m_accounts) {
        accountIds << acc->id();
    }

    Q_FOREACH(const QString &group, m_settings.childGroups()) {
        QString strId = group.split("_").value(1, "");
        if (strId.isEmpty())
            continue;

        bool ok = true;
        int id = strId.toInt(&ok);
        if (!ok)
            continue;

        if (!accountIds.contains(id)) {
            qDebug() << "Clean log entry" << group << "from account:" << id;
            m_settings.remove(group);
            SyncConfigure::removeAccountConfig(id);
        }
    }
    m_settings.sync();
}

void SyncDaemon::run()
{
    setupAccounts();
    setupTriggers();
    cleanupLogs();
    cleanupConfig();

    // export dbus interface
    registerService();
}

bool SyncDaemon::isPending() const
{
    // there is a sync request on the buffer
    return (m_syncQueue && (m_syncQueue->count() > 0));
}

bool SyncDaemon::isSyncing() const
{
    // the sync is happening right now
    return (m_syncing && m_currentJob.isValid());
}

QStringList SyncDaemon::availableServices() const
{
    // TODO: check for all providers
    return m_provider->supportedServices("google");
}

QStringList SyncDaemon::enabledServices() const
{
    QSet<QString> services;
    QStringList available = availableServices();
    Q_FOREACH(SyncAccount *syncAcc, m_accounts) {
        Q_FOREACH(const QString &service, syncAcc->enabledServices()) {
            if (available.contains(service)) {
                services << service;
            }
        }
    }
    return services.toList();
}

bool SyncDaemon::isOnline() const
{
    return m_networkStatus->state() != SyncNetwork::NetworkOffline;
}

bool SyncDaemon::syncOnMobileConnection() const
{
    return m_settings.value(SYNC_ON_MOBILE_CONFIG_KEY, false).toBool();
}

void SyncDaemon::setSyncOnMobileConnection(bool flag)
{
    m_settings.setValue(SYNC_ON_MOBILE_CONFIG_KEY, flag);
    m_settings.sync();
}

SyncAccount *SyncDaemon::accountById(quint32 accountId)
{
    return m_accounts.value(accountId);
}

void SyncDaemon::addAccount(const AccountId &accountId, bool startSync)
{
    Account *acc = m_manager->account(accountId);
    if (!acc) {
        qWarning() << "Fail to retrieve accounts:" << m_manager->lastError().message();
    } else if (m_provider->contains(acc->providerName())) {
        qDebug() << "Found account:" << acc->displayName();
        SyncAccount *syncAcc = new SyncAccount(acc,
                                               m_provider->settings(acc->providerName()),
                                               this);
        m_accounts.insert(accountId, syncAcc);
        connect(syncAcc, SIGNAL(syncStarted()),
                         SLOT(onAccountSyncStart()));
        connect(syncAcc, SIGNAL(syncSourceStarted(QString,QString,bool)),
                         SLOT(onAccountSourceSyncStarted(QString, QString, bool)));
        connect(syncAcc, SIGNAL(syncSourceFinished(QString,QString,bool,QString,QString)),
                         SLOT(onAccountSourceSyncFinished(QString,QString,bool,QString,QString)));
        connect(syncAcc, SIGNAL(syncFinished(QString,QMap<QString,QString>)),
                         SLOT(onAccountSyncFinished(QString,QMap<QString,QString>)));
        connect(syncAcc, SIGNAL(enableChanged(QString, bool)),
                         SLOT(onAccountEnableChanged(QString, bool)));
        connect(syncAcc, SIGNAL(syncError(QString,QString)),
                         SLOT(onAccountSyncError(QString, QString)));
        connect(syncAcc, SIGNAL(sourceRemoved(QString)),
                         SLOT(onAccountSourceRemoved(QString)));

        const bool accountEnabled = syncAcc->isEnabled();
        if (startSync && accountEnabled) {
            sync(syncAcc, QStringList(), true, true);
        }
        Q_EMIT accountsChanged();
    }
}

void SyncDaemon::sync(bool runNow)
{
    m_syncing = true;
    if (runNow) {
        m_timeout->stop();
        continueSync();
    } else {
        // wait some time for new sync requests
        m_timeout->start();
    }
}

void SyncDaemon::sync(SyncAccount *syncAcc, const QStringList &sources, bool runNow, bool syncOnMobile)
{
    qDebug() << "syn requested for account:" << syncAcc->displayName() << sources;

    // check if the account is enabled
    if (!syncAcc->isEnabled()) {
        qDebug() << "Account not enabled. Skip sync.";
        return;
    }

    // check if the request is the current sync
    if (m_currentJob.contains(syncAcc, sources)) {
        qDebug() << "Syncing the requested account and sources. Ignore request!";
        return;
    }

    // check if the request is already in the queue
    QStringList newSources(sources);
    Q_FOREACH(const QString &source, sources) {
        if (m_syncQueue->contains(syncAcc, source)) {
            newSources.removeOne(source);
        }
    }

    if (!sources.isEmpty() && newSources.isEmpty()) {
        qDebug() << "Sources already in the queue. Ignore request!";
        return;
    }

    qDebug() << "Pushed into queue with immediately sync?" << runNow << "Sync is running" << m_syncing;
    m_syncQueue->push(syncAcc, newSources, syncOnMobile || syncOnMobileConnection());
    // if not syncing start a full sync
    if (!m_syncing) {
        qDebug() << "Request sync";
        Q_EMIT syncAboutToStart();
        sync(runNow);
        return;
    }

    // immediately request, force sync to start
    if (runNow && !isSyncing()) {
        Q_EMIT syncAboutToStart();
        sync(runNow);
    }
}

void SyncDaemon::cancel(SyncAccount *syncAcc, const QStringList &sources)
{
    QList<SyncAccount*> accounts;
    if (syncAcc == 0) {
        accounts << m_accounts.values();
    } else {
        accounts << syncAcc;
    }

    Q_FOREACH(const SyncAccount *acc, accounts) {
        m_syncQueue->remove(syncAcc, sources);
        syncAcc->cancel();
        if (m_currentJob.account() == syncAcc) {
            qDebug() << "Current sync canceled";
            m_currentJob.clear();
        } else if (m_syncQueue->isEmpty()) {
            syncFinishedImpl();
        }
        Q_FOREACH(const QString &source, sources) {
            Q_EMIT syncError(syncAcc, source, "canceled");
        }
    }
}

void SyncDaemon::removeAccount(const AccountId &accountId)
{
    SyncAccount *syncAcc = m_accounts.take(accountId);
    if (syncAcc) {
        cancel(syncAcc, QStringList());
        // Remove legacy source if necessary
        QString sourceId = m_eds->sourceIdByName(syncAcc->displayName(), 0);
        if (!sourceId.isEmpty()) {
            m_eds->removeSource(sourceId);
        }
    }
    Q_EMIT accountsChanged();
}

void SyncDaemon::destroyAccount()
{
    QObject *sender = QObject::sender();
    SyncAccount *acc =  qobject_cast<SyncAccount*>(sender->property("ACCOUNT").value<QObject*>());
    Q_ASSERT(acc);
    acc->removeConfig();
    acc->deleteLater();
}

void SyncDaemon::authenticateAccount(const SyncAccount *account, const QString &serviceName)
{
    NotifyMessage *notify = new NotifyMessage(true, this);
    notify->setProperty("ACCOUNT", QVariant::fromValue<AccountId>(account->id()));
    notify->setProperty("SERVICE", QVariant::fromValue<QString>(account->serviceId(serviceName)));
    connect(notify, SIGNAL(questionAccepted()), SLOT(runAuthentication()));
    notify->askYesOrNo(_("Synchronization"),
                       QString(_("Your access key is not valid anymore. Do you want to re-authenticate it?.")),
                       account->iconName(serviceName));

}

void SyncDaemon::runAuthentication()
{
    QObject *sender = QObject::sender();
    AccountId accountId = sender->property("ACCOUNT").value<AccountId>();
    QString serviceName = sender->property("SERVICE").value<QString>();

    QString appCommand = QString("syncmonitorhelper:///authenticate?id=%1&service=%2").arg(accountId).arg(serviceName);
    url_dispatch_send(appCommand.toUtf8().constData(), NULL, NULL);
}

void SyncDaemon::onAccountSyncStart()
{
    SyncAccount *acc = qobject_cast<SyncAccount*>(QObject::sender());
    // notification only appears on first sync
    if (isFirstSync(acc->id()) && (acc->lastError() == 0)) {
        NotifyMessage *notify = new NotifyMessage(true, this);
        notify->show(_("Synchronization"),
                     QString(_("Start sync: %1 (Calendar)")).arg(acc->displayName()),
                     acc->iconName(CALENDAR_SERVICE_NAME));
    }
}


void SyncDaemon::onAccountSourceSyncStarted(const QString &serviceName,
                                            const QString &sourceName,
                                            bool firstSync)
{
    SyncAccount *acc = qobject_cast<SyncAccount*>(QObject::sender());
    m_syncElapsedTime.restart();
    qDebug() << QString("[%3] Start sync: %1 (%2)")
                .arg(acc->displayName())
                .arg(serviceName + "/" + sourceName)
                .arg(QDateTime::currentDateTime().toString(Qt::SystemLocaleShortDate));
    Q_EMIT syncStarted(acc, serviceName);
}

void SyncDaemon::onAccountSyncError(const QString &serviceName, const QString &error)
{
    Q_EMIT syncError(qobject_cast<SyncAccount*>(QObject::sender()), serviceName, error);
    syncFinishedImpl();
}

void SyncDaemon::onAccountSourceSyncFinished(const QString &serviceName,
                                             const QString &sourceName,
                                             const bool firstSync,
                                             const QString &status,
                                             const QString &mode)
{
    Q_UNUSED(mode);
    Q_UNUSED(firstSync);

    SyncAccount *acc = qobject_cast<SyncAccount*>(QObject::sender());
    QString errorMessage = SyncAccount::statusDescription(status);

    qDebug() << QString("[%6] Sync done: %1 (%2) Status: %3 Error: %4 Duration: %5s")
                .arg(acc->displayName())
                .arg(serviceName + "/" + sourceName)
                .arg(status)
                .arg(errorMessage.isEmpty() ? "None" : errorMessage)
                .arg((m_syncElapsedTime.elapsed() < 1000 ? 1  : m_syncElapsedTime.elapsed() / 1000))
                .arg(QDateTime::currentDateTime().toString(Qt::SystemLocaleShortDate));

}

void SyncDaemon::onAccountSyncFinished(const QString &serviceName,
                                       const QMap<QString, QString> &statusList)
{
    // error on that list will trigger a new sync
    static QStringList whiteListStatus;

    SyncAccount *acc = qobject_cast<SyncAccount*>(QObject::sender());
    // check fisrt sync before store the log information
    const bool firstSync = isFirstSync(acc->id());
    const bool accountEnabled = acc->isEnabled();

    Q_EMIT syncFinished(acc, serviceName);

    // populate white list erros
    if (whiteListStatus.isEmpty()) {
        // "error code from SyncEvolution access denied (remote, status 403): could not obtain OAuth2 token:
        // this can happen if the network goes off during the sync, or syc started before the network stabilished
        whiteListStatus << QStringLiteral("10403");
        whiteListStatus << QStringLiteral("403");

        // error code from SyncEvolution fatal error (local, status 10500): no sources active, check configuration"
        // this is a bug on SyncEvolution sometimes it fail to read the correct address book
        // FIXME: we should fix that on SyncEvolution
        whiteListStatus << QStringLiteral("10500");
    }

    // check if we are going re-sync due a know problem
    uint errorCode = 0;
    bool fail = false;
    Q_FOREACH(const QString &source, statusList.keys()) {
        const QString status = statusList.value(source);
        QString errorMessage = SyncAccount::statusDescription(status);
        errorCode = status.toUInt();
        bool saveLog = accountEnabled;

        if ((acc->lastError() == 0) && !errorMessage.isEmpty() && whiteListStatus.contains(status)) {
            fail = true;
            saveLog = false;
            // white list error retry the sync
            qDebug() << "Trying a second sync due error:" << errorMessage;
            m_syncQueue->push(acc, QStringList(), false);
            break;
        } else if (!errorMessage.isEmpty()) {
            fail = true;
            errorCode = 0;
            NotifyMessage *notify = new NotifyMessage(true, this);
            notify->show(_("Synchronization"),
                         QString(_("Fail to sync calendar %1 from account %2.\n%3"))
                             .arg(source)
                             .arg(acc->displayName())
                             .arg(errorMessage),
                         acc->iconName(CALENDAR_SERVICE_NAME));
        }

        if (saveLog && !source.isEmpty()) {
            saveSyncResult((uint) acc->id(), source, status, QDateTime::currentDateTime().toUTC().toString(Qt::ISODate));
        }
    }

    if (!fail) {
        errorCode = 0;
        // avoid to show sync done message for disabled accounts.
        if (accountEnabled && firstSync) {
            NotifyMessage *notify = new NotifyMessage(true, this);
            notify->show(_("Synchronization"),
                         QString(_("Sync done: %1 (Calendar)")).arg(acc->displayName()),
                         acc->iconName(CALENDAR_SERVICE_NAME));
        }
    }

    acc->setLastError(errorCode);

    SyncEvolutionServerProxy::destroy();
    // sync next account
    continueSync();
}

void SyncDaemon::onAccountEnableChanged(const QString &serviceName, bool enabled)
{
    SyncAccount *acc = qobject_cast<SyncAccount*>(QObject::sender());
    if (serviceName.isEmpty()) {
        if (!acc->enabledServices().contains(CALENDAR_SERVICE_NAME)) {
            qDebug() << "Account enabled but calendar service disabled!";
            return;
        }
    } else if (serviceName != CALENDAR_SERVICE_NAME) {
        qDebug() << "Account service enable changed:" << serviceName << ". Ignore it.";
        return;
    }


    if (enabled) {
        sync(acc, QStringList(), true, true);
    } else {
        cancel(acc, QStringList());
    }
    Q_EMIT accountsChanged();
}

void SyncDaemon::onAccountSourceRemoved(const QString &source)
{
    SyncAccount *acc = qobject_cast<SyncAccount*>(QObject::sender());
    clearResultForSource(acc->id(), source.mid(source.indexOf("/") + 1));
}

void SyncDaemon::quit()
{
    m_aboutToQuit = true;

    if (m_dbusAddaptor) {
        delete m_dbusAddaptor;
        m_dbusAddaptor = 0;
    }

    if (m_eds) {
        delete m_eds;
        m_eds = 0;
    }

    // cancel all sync operation
    while(m_syncQueue->count()) {
        SyncJob job = m_syncQueue->popNext();
        SyncAccount *acc = job.account();
        acc->cancel();
        acc->wait();
        delete acc;
    }

    while(m_offlineQueue->count()) {
        SyncJob job = m_offlineQueue->popNext();
        SyncAccount *acc = job.account();
        acc->cancel();
        acc->wait();
        delete acc;
    }

    if (m_manager) {
        delete m_manager;
        m_manager = 0;
    }

    if (m_networkStatus) {
        delete m_networkStatus;
    }
}
