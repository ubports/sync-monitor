#include "sync-account.h"
#include "syncevolution-server-proxy.h"
#include "syncevolution-session-proxy.h"

using namespace Accounts;

const QString SyncAccount::GoogleCalendarService = QStringLiteral("google-carddav");
const QString SyncAccount::GoogleContactService = QStringLiteral("google-carddav");

SyncAccount::SyncAccount(Account *account, QObject *parent)
    : QObject(parent),
      m_currentSession(0),
      m_account(account),
      m_isNew(false),
      m_state(SyncAccount::Empty)
{
    m_sessionName = QString("ubuntu-google-contacts-%1").arg(account->id());
    m_syncOperation.insert(GoogleCalendarService, QStringLiteral("disabled"));
    m_syncOperation.insert(GoogleContactService, QStringLiteral("disabled"));
    setup();
}

SyncAccount::~SyncAccount()
{
    cancel();
}

void SyncAccount::setup()
{
    // enable sync for enabled service on account
    Q_FOREACH(Service service, m_account->enabledServices()) {
        qDebug() << "Service enabled" << service.name();
        if (m_syncOperation.contains(service.name())) {
            m_syncOperation[service.name()] = QStringLiteral("two-way");
        }
    }

    connect(m_account,
            SIGNAL(enabledChanged(QString,bool)),
            SLOT(onAccountEnabledChanged(QString,bool)));
}

void SyncAccount::cancel()
{
    if (m_currentSession) {
        m_currentSession->destroy();
        m_currentSession = 0;
    }
}

void SyncAccount::sync()
{
    switch(m_state) {
    case SyncAccount::Empty:
        configure();
        break;
    case SyncAccount::Idle:
        continueSync();
        break;
    default:
        break;
    }
}

void SyncAccount::continueSync()
{
    setState(SyncAccount::Syncing);
    SyncEvolutionServerProxy *proxy = SyncEvolutionServerProxy::instance();
    SyncEvolutionSessionProxy *session = proxy->openSession(QString("google-contacts-%1").arg(m_account->id()),
                                                            QStringList());
    if (session) {
        attachSession(session);

        QStringMap syncFlags;
        syncFlags.insert("addressbook", m_isNew ? "slow" : m_syncOperation[GoogleContactService]);
        session->sync(syncFlags);
    } else {
        setState(SyncAccount::Invalid);
    }
}

void SyncAccount::wait()
{
    //TODO
}

SyncAccount::AccountState SyncAccount::state() const
{
    return m_state;
}

QDateTime SyncAccount::lastSync() const
{
    uint pageSize = 100;
    uint index = 0;
    QStringMap lastReport;

    // load all reports
    QArrayOfStringMap reports = m_currentSession->reports(index, pageSize);
    while (reports.size() == pageSize) {
        lastReport = reports.last();
        index += pageSize;
        reports = m_currentSession->reports(index, pageSize);
    }

    if (reports.size()) {
        lastReport = reports.last();
    }

    if (lastReport.contains("start")) {
        QString lastSync = lastReport.value("start", "0");
        return QDateTime::fromTime_t(lastSync.toInt());
    } else {
        return QDateTime();
    }
}

void SyncAccount::onAccountEnabledChanged(const QString &serviceName, bool enabled)
{
    if (m_syncOperation.contains(serviceName)) {
        m_syncOperation[serviceName] = (enabled ? QStringLiteral("two-way") : QStringLiteral("disabled"));
        Q_EMIT enableChanged(enabled);
    }
}

bool SyncAccount::configServerSide()
{
    AccountId accountId = m_account->id();
    // config server side
    QStringMultiMap config = m_currentSession->getConfig("SyncEvolution", true);
    Q_ASSERT(!config.isEmpty());
    config[""]["syncURL"] = QStringLiteral("https://www.googleapis.com/.well-known/carddav");
    config[""]["username"] = QString("uoa:%1,google-carddav").arg(accountId);
    config[""]["consumerReady"] = "0";
    config["source/addressbook"]["backend"] = "CardDAV";
    config.remove("source/todo");
    config.remove("source/memo");
    config.remove("source/calendar");
    return m_currentSession->saveConfig(QString("target-config@google-%1").arg(accountId), config);
}

bool SyncAccount::configClientSide()
{
    Q_ASSERT(m_currentSession);
    AccountId accountId = m_account->id();
    // config server side
    QStringMultiMap config = m_currentSession->getConfig("SyncEvolution_Client", true);
    Q_ASSERT(!config.isEmpty());
    config[""]["syncURL"] = QString("local://@google-%1").arg(accountId);
    config[""]["username"] = QString();
    config[""]["password"] = QString();
    return m_currentSession->saveConfig(QString("google-contacts-%1").arg(accountId), config);
}

void SyncAccount::continueConfigure()
{
    Q_ASSERT(m_currentSession);
    AccountId accountId = m_account->id();
    bool isConfigured = m_currentSession->hasConfig(QString("target-config@google-%1").arg(accountId));
    if (isConfigured) {
        qDebug() << "Account already configured";
    } else if (configServerSide() && configClientSide()) {
        qDebug() << "Account configured";
    } else {
        qWarning() << "Fail to configure account" << accountId;
        setState(SyncAccount::Invalid);
    }
    releaseSession();
    if (state() != SyncAccount::Invalid) {
        setState(SyncAccount::Idle);
        continueSync();
    }
}

void SyncAccount::onSessionStatusChanged(const QString &newStatus)
{
    qDebug() << "Session status changed" << newStatus;
    switch (m_state) {
    case SyncAccount::Idle:
        if (newStatus == "running") {
            setState(SyncAccount::Syncing);
            Q_EMIT syncStarted();
        }
        break;
    case SyncAccount::Configuring:
        if (newStatus != "queueing") {
            continueConfigure();
        }
        break;
    case SyncAccount::Syncing:
        if (newStatus == "done") {
            releaseSession();
            setState(SyncAccount::Idle);
            Q_EMIT syncFinished();
        }
        break;
    default:
        break;
    }
}

void SyncAccount::onSessionProgressChanged(int progress)
{
    qDebug() << "Progress" << progress;
}

void SyncAccount::onSessionError(int error)
{
    qWarning() << "Session error" << error;
    setState(SyncAccount::Invalid);
}

void SyncAccount::configure()
{
    qDebug() << "Configure account";
    if (m_state == SyncAccount::Empty) {
        setState(SyncAccount::Configuring);
        m_isNew = false;
        SyncEvolutionServerProxy *proxy = SyncEvolutionServerProxy::instance();
        SyncEvolutionSessionProxy *session = proxy->openSession(m_sessionName, QStringList() << "all-configs");
        if (session) {
            attachSession(session);
            qDebug() << "Configure account status" << session->status();
            if (session->status() != "queueing") {
                continueConfigure();
            }
        } else {
            setState(SyncAccount::Invalid);
            qWarning() << "Fail to configure account" << m_account->id() << m_account->displayName();
        }
    } else {
        qWarning() << "Called configure for a account with invalid state" << m_state;
    }
}

void SyncAccount::setState(SyncAccount::AccountState state)
{
    if (m_state != state) {
        m_state = state;
        Q_EMIT stateChanged(m_state);
    }
}

void SyncAccount::attachSession(SyncEvolutionSessionProxy *session)
{
    Q_ASSERT(m_currentSession == 0);
    m_currentSession = session;
    m_sessionConnections << connect(m_currentSession,
                                    SIGNAL(statusChanged(QString)),
                                    SLOT(onSessionStatusChanged(QString)));
    m_sessionConnections << connect(m_currentSession,
                                    SIGNAL(progressChanged(int)),
                                    SLOT(onSessionProgressChanged(int)));
}

void SyncAccount::releaseSession()
{
    Q_ASSERT(m_currentSession != 0);
    Q_FOREACH(QMetaObject::Connection conn, m_sessionConnections) {
        disconnect(conn);
    }
    m_sessionConnections.clear();
    m_currentSession->destroy();
    m_currentSession->deleteLater();
    m_currentSession = 0;
}

