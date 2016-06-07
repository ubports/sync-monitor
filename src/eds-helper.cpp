#include "eds-helper.h"

#include <QtCore/QDebug>

#include <QtOrganizer/QOrganizerManager>
#include <QtOrganizer/QOrganizerCollection>

#include <QtContacts/QContactManager>
#include <QtContacts/QContactDisplayLabel>
#include <QtContacts/QContactDetailFilter>
#include <QtContacts/QContactFetchByIdRequest>
#include <QtContacts/QContactSyncTarget>
#include <QtContacts/QContactExtendedDetail>

#include "config.h"

#define CHANGE_TIMEOUT      3000
#define COLLECTION_READONLY_METADATA        "collection-readonly"
#define COLLECTION_SYNC_READONLY_METADATA   "collection-sync-readonly"
#define COLLECTION_ACCOUNT_ID_METADATA      "collection-account-id"
#define COLLECTION_REMOTE_URL_METADATA      "collection-metadata"
#define COLLECTION_SELECTED_METADATA        "collection-selected"

using namespace QtOrganizer;
using namespace QtContacts;

EdsHelper::EdsHelper(QObject *parent, const QString &organizerManager)
    : QObject(parent),
      m_freezed(false)
{
    qRegisterMetaType<QList<QOrganizerItemId> >("QList<QOrganizerItemId>");

    if (!organizerManager.isEmpty()) {
        m_organizerEngine = new QOrganizerManager(organizerManager, QMap<QString, QString>());
    } else {
        m_organizerEngine = 0;
    }

    m_timeoutTimer.setSingleShot(true);
}

EdsHelper::~EdsHelper()
{
    delete m_organizerEngine;
    m_organizerEngine = 0;
}

QString EdsHelper::createSource(const QString &sourceName,
                                const QString &sourceColor,
                                const QString &sourceRemoteUrl,
                                int accountId)
{
    if (!m_organizerEngine) {
        qWarning() << "Request to create an organizer source with a null organize engine";
        return QString();
    }

    QString sourceId = sourceIdByRemoteUrl(sourceRemoteUrl, accountId);
    if (!sourceId.isEmpty()) {
        return sourceId;
    }

    QOrganizerCollection collection;
    collection.setMetaData(QOrganizerCollection::KeyName, sourceName);
    collection.setMetaData(QOrganizerCollection::KeyColor, sourceColor);
    collection.setExtendedMetaData(COLLECTION_REMOTE_URL_METADATA, sourceRemoteUrl);
    collection.setExtendedMetaData(COLLECTION_ACCOUNT_ID_METADATA, accountId);
    collection.setExtendedMetaData(COLLECTION_SELECTED_METADATA, true);

    if (!m_organizerEngine->saveCollection(&collection)) {
        qWarning() << "Fail to create collection" << sourceName;
        return QString();
    } else {
        return collection.id().toString();
    }
}


void EdsHelper::removeSource(const QString &sourceId)
{
    if (!m_organizerEngine) {
        qWarning() << "Request to remove organizer source with a null organize engine";
        return;
    }

    QOrganizerCollectionId id = QOrganizerCollectionId::fromString(sourceId);

    if (!m_organizerEngine->removeCollection(id)) {
        qWarning() << "Fail to remove source" << id;
    }
}

QString EdsHelper::sourceIdByName(const QString &sourceName, uint account)
{
    if (!m_organizerEngine) {
        qWarning() << "sourceIdByName: organizer engine is null";
        return QString();
    }

    Q_FOREACH(const QOrganizerCollection &c, m_organizerEngine->collections()) {
        if ((c.extendedMetaData(COLLECTION_ACCOUNT_ID_METADATA) == account) &&
            (c.metaData(QOrganizerCollection::KeyName).toString() == sourceName)) {
            return c.id().toString();
        }
    }
    return QString();
}

QString EdsHelper::sourceIdByRemoteUrl(const QString &url, uint account)
{
    Q_FOREACH(const QOrganizerCollection &c, m_organizerEngine->collections()) {
        if ((c.extendedMetaData(COLLECTION_ACCOUNT_ID_METADATA) == account) &&
            (c.extendedMetaData(COLLECTION_REMOTE_URL_METADATA).toString() == url)) {
            return c.id().toString();
        }
    }
    return QString();
}

QPair<uint, QString> EdsHelper::sourceAccountAndNameFromId(const QString &sourceId)
{
    QPair<uint, QString> result;
    QOrganizerCollectionId id = QOrganizerCollectionId::fromString("qtorganizer:eds::" + sourceId);
    QOrganizerCollection collection = m_organizerEngine->collection(id);
    if (collection.id().isNull()) {
        qWarning() << "Collection not found:" << sourceId;
    } else {
        bool ok = false;
        uint accountId = collection.extendedMetaData(COLLECTION_ACCOUNT_ID_METADATA).toUInt(&ok);
        if (!ok) {
            accountId = 0;
        }
        result = qMakePair(accountId, collection.metaData(QOrganizerCollection::KeyName).toString());
    }
    return result;
}

void EdsHelper::freezeNotify()
{
    m_freezed = true;
}

void EdsHelper::unfreezeNotify()
{
    m_pendingCalendars.clear();
    m_freezed = false;
    m_timeoutTimer.start(CHANGE_TIMEOUT);
}

void EdsHelper::flush()
{
    m_freezed = false;

    if (m_organizerEngine) {
        Q_FOREACH(const QString &calendar, m_pendingCalendars) {
            Q_EMIT dataChanged(calendar);
        }
    }
    m_pendingCalendars.clear();
}

void EdsHelper::setEnabled(bool enabled)
{
    if (enabled) {
        if (m_organizerEngine) {
            connect(m_organizerEngine,
                    SIGNAL(itemsAdded(QList<QOrganizerItemId>)),
                    SLOT(calendarChanged(QList<QOrganizerItemId>)), Qt::QueuedConnection);
            connect(m_organizerEngine,
                    SIGNAL(itemsRemoved(QList<QOrganizerItemId>)),
                    SLOT(calendarChanged(QList<QOrganizerItemId>)), Qt::QueuedConnection);
            connect(m_organizerEngine,
                    SIGNAL(itemsChanged(QList<QOrganizerItemId>)),
                    SLOT(calendarChanged(QList<QOrganizerItemId>)), Qt::QueuedConnection);
        }
    } else {
        if (m_organizerEngine) {
            m_organizerEngine->disconnect(this);
        }
    }
}

QMap<int, QStringList> EdsHelper::sources()
{
    QMap<int, QStringList> result;
    QList<QOrganizerCollection> collections = m_organizerEngine->collections();
    Q_FOREACH(const QOrganizerCollection &collection, collections) {
        bool ok = false;
        int accountId = collection.extendedMetaData(COLLECTION_ACCOUNT_ID_METADATA).toInt(&ok);
        if (!ok) {
            accountId = -1;
        }
        QStringList sources  = result.value(accountId);
        sources << collection.id().toString();
        result.insert(accountId, sources);
    }

    return result;
}

QString EdsHelper::getCollectionIdFromItemId(const QOrganizerItemId &itemId) const
{
    return itemId.toString().split("/").first();
}

void EdsHelper::calendarChanged(const QList<QOrganizerItemId> &itemIds)
{
    Q_ASSERT(m_organizerEngine);

    QSet<QString> uniqueColletions;

    // eds item ids cotains the collection id we can use that instead of query for the full item
    Q_FOREACH(const QOrganizerItemId &id, itemIds) {
        uniqueColletions << getCollectionIdFromItemId(id);
    }

    if (uniqueColletions.isEmpty()) {
        return;
    }

    Q_FOREACH(const QString &collectionId, uniqueColletions) {
        if (m_freezed) {
            m_pendingCalendars << collectionId;
        } else {
            Q_EMIT dataChanged(collectionId);
        }
    }
}
