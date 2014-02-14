#include "sync-daemon.h"
#include "sync-account.h"
#include "address-book-trigger.h"

#include <QtCore/QDebug>
#include <QtCore/QTimer>

using namespace Accounts;

#define GOOGLE_PROVIDER_NAME    "google"
#define DAEMON_SYNC_TIMEOUT     10000

SyncDaemon::SyncDaemon()
    : QObject(0),
      m_manager(0),
      m_syncing(false),
      m_aboutToQuit(false)
{
    m_timeout = new QTimer(this);
    m_timeout->setInterval(DAEMON_SYNC_TIMEOUT);
    m_timeout->setSingleShot(true);
    connect(m_timeout, SIGNAL(timeout()), SLOT(continueSync()));
}

void SyncDaemon::setupAccounts()
{
    if (m_manager) {
        return;
    }
    m_manager = new Manager(this);
    Q_FOREACH(const AccountId &accountId, m_manager->accountList()) {
        addAccount(accountId, false);
    }
    connect(m_manager,
            SIGNAL(accountCreated(Accounts::AccountId)),
            SLOT(addAccount(Accounts::AccountId)));
    connect(m_manager,
            SIGNAL(accountRemoved(Accounts::AccountId)),
            SLOT(removeAccount(Accounts::AccountId)));
}

void SyncDaemon::setupTriggers()
{
    AddressBookTrigger *trigger = new AddressBookTrigger(this);
    connect(trigger, SIGNAL(contactsUpdated()), SLOT(syncAll()));
}

SyncDaemon::~SyncDaemon()
{
    quit();
}

void SyncDaemon::syncAll()
{
    qDebug() << "sync all" << m_syncing;
    Q_FOREACH(SyncAccount *acc, m_accounts.values()) {
        sync(acc);
    }
}

void SyncDaemon::sync()
{
    // wait some time for new sync requests
    m_timeout->start();
}

void SyncDaemon::continueSync()
{
    qDebug() << "continue to sync" << m_syncing;
    // sync one account by time
    if (!m_aboutToQuit && m_syncQueue.size()) {
        m_syncing = true;
        SyncAccount *syncAcc = m_syncQueue.takeFirst();
        syncAcc->sync();
    } else {
        qDebug() << "no account to sync";
        m_syncing = false;
    }
}

void SyncDaemon::run()
{
    setupAccounts();
    setupTriggers();
    sync();
}

void SyncDaemon::addAccount(const AccountId &accountId, bool startSync)
{
    Q_ASSERT(!m_accounts.contains(accountId));
    Account *acc = m_manager->account(accountId);
    if (!acc) {
        qWarning() << "Fail to retrieve accounts:" << m_manager->lastError().message();
    } else if (acc->providerName() == GOOGLE_PROVIDER_NAME) {
        SyncAccount *syncAcc = new SyncAccount(acc, this);
        m_accounts.insert(accountId, syncAcc);
        connect(syncAcc, SIGNAL(syncStarted()), SLOT(onAccountSyncStarted()));
        connect(syncAcc, SIGNAL(syncFinished()), SLOT(onAccountSyncFinished()));
        connect(syncAcc, SIGNAL(syncError(int)), SLOT(onAccountSyncError(int)));
        connect(syncAcc, SIGNAL(enableChanged(bool)), SLOT(onAccountEnableChanged(bool)));
        if (startSync) {
            sync(syncAcc);
        }
    }
}

void SyncDaemon::sync(SyncAccount *syncAcc)
{
    qDebug() << "sync account" << syncAcc << m_syncing;
    if (!m_syncQueue.contains(syncAcc)) {
        m_syncQueue.push_back(syncAcc);
        if (!m_syncing) {
            sync();
        }
    }
}

void SyncDaemon::cancel(SyncAccount *syncAcc)
{
    qDebug() << "cancel sync for account" << syncAcc;
    m_syncQueue.removeOne(syncAcc);
    syncAcc->cancel();
}

void SyncDaemon::removeAccount(const AccountId &accountId)
{
    SyncAccount *syncAcc = m_accounts.take(accountId);
    if (syncAcc) {
        cancel(syncAcc);
        syncAcc->deleteLater();
    }
}

void SyncDaemon::onAccountSyncStarted()
{
    //TODO
}

void SyncDaemon::onAccountSyncFinished()
{
    // sync next account
    continueSync();
}

void SyncDaemon::onAccountSyncError(int errorCode)
{
    qWarning() << "Fail to sync account" << errorCode;
    // sync next account
    continueSync();
}

void SyncDaemon::onAccountEnableChanged(bool enabled)
{
    SyncAccount *acc = qobject_cast<SyncAccount*>(QObject::sender());
    qDebug() << "account enabled changed" << enabled;
    if (enabled) {
        sync(acc);
    } else {
        cancel(acc);
    }
}

void SyncDaemon::quit()
{
    m_aboutToQuit = true;

    // cancel all sync operation
    while(m_syncQueue.size()) {
        SyncAccount *acc = m_syncQueue.takeFirst();
        acc->cancel();
        acc->wait();
        delete acc;
    }

    if (m_manager) {
        delete m_manager;
        m_manager = 0;
    }
}
