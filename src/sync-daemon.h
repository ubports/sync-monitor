#ifndef __SYNC_DAEMON_H__
#define __SYNC_DAEMON_H__

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QQueue>
#include <QtCore/QTimer>

#include <Accounts/Manager>

class SyncAccount;

class SyncDaemon : public QObject
{
    Q_OBJECT
public:
    SyncDaemon();
    ~SyncDaemon();
    void run();

public Q_SLOTS:
    void quit();

private Q_SLOTS:
    void syncAll();
    void continueSync();
    void addAccount(const Accounts::AccountId &accountId, bool startSync=true);
    void removeAccount(const Accounts::AccountId &accountId);

    void onAccountSyncStarted();
    void onAccountSyncFinished();
    void onAccountSyncError(int errorCode);
    void onAccountEnableChanged(bool enabled);

private:
    Accounts::Manager *m_manager;
    QTimer *m_timeout;
    QHash<Accounts::AccountId, SyncAccount*> m_accounts;
    QQueue<SyncAccount*> m_syncQueue;
    bool m_syncing;
    bool m_aboutToQuit;

    void setupAccounts();
    void setupTriggers();
    void sync(SyncAccount *syncAcc);
    void cancel(SyncAccount *syncAcc);
    void setup();
    void sync();
};

#endif
