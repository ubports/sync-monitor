#ifndef __SYNCEVOLUTION_SESSION_PROXY_H__
#define __SYNCEVOLUTION_SESSION_PROXY_H__

#include "dbustypes.h"

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QHash>

#include <QtDBus/QDBusInterface>



class SyncEvolutionSessionProxy : public QObject
{
    Q_OBJECT
public:
    QString id() const;
    void destroy();
    QString status() const;
    bool hasConfig(const QString &configName);
    QStringMultiMap getConfig(const QString &configName, bool isTemplate);
    bool saveConfig(const QString &configName, QStringMultiMap config);
    void detach();
    bool isValid() const;
    void sync(QStringMap services);
    QArrayOfStringMap reports(uint start, uint maxCount);

Q_SIGNALS:
    void statusChanged(const QString &status);
    void progressChanged(int progress);
    void error(uint error);

private Q_SLOTS:
    void onSessionStatusChanged(const QString &status, uint error, QSyncStatusMap source);
    void onSessionProgressChanged(int progress, QSyncProgressMap sources);

private:
    QDBusInterface *m_iface;

    SyncEvolutionSessionProxy(const QDBusObjectPath &objectPath, QObject *parent=0);
    friend class SyncEvolutionServerProxy;
};

#endif
