#ifndef __SYNC_ACCOUNT_H__
#define __SYNC_ACCOUNT_H__

#include <QtCore/QObject>
#include <QtCore/QHash>

#include <Accounts/Account>

#include "dbustypes.h"

class SyncEvolutionSessionProxy;
class SyncAccount : public QObject
{
    Q_OBJECT
public:
    static const QString GoogleCalendarService;
    static const QString GoogleContactService;

    enum AccountState {
        Empty,
        Configuring,
        Syncing,
        Idle,
        Invalid
    };

    SyncAccount(Accounts::Account *account, QObject *parent=0);
    ~SyncAccount();

    void setup();
    void cancel();
    void sync();
    void wait();
    void status() const;
    AccountState state() const;
    QDateTime lastSyncDate() const;
    bool enabled() const;
    QString displayName() const;
    int id() const;

Q_SIGNALS:
    void stateChanged(AccountState newState);
    void syncStarted();
    void syncFinished();
    void syncError(int);
    void enableChanged(bool enable);
    void configured();

private Q_SLOTS:
    void onAccountEnabledChanged(const QString &serviceName, bool enabled);
    void onSessionStatusChanged(const QString &newStatus);
    void onSessionProgressChanged(int progress);
    void onSessionError(uint error);

private:
    Accounts::Account *m_account;
    SyncEvolutionSessionProxy *m_currentSession;

    QString m_sessionName;
    QStringMap m_syncOperation;
    AccountState m_state;
    QList<QMetaObject::Connection> m_sessionConnections;

    void configure();
    void continueConfigure();
    bool configClientSide();
    bool configServerSide();
    void setState(AccountState state);
    void continueSync();
    void attachSession(SyncEvolutionSessionProxy *session);
    void releaseSession();
    QStringMap lastReport() const;
    QString lastSyncStatus() const;
};

#endif
