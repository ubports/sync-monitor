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
#define COLLECTION_REMOTE_ID_METADATA       "collection-metadata"
#define COLLECTION_SELECTED_METADATA        "collection-selected"

using namespace QtOrganizer;
using namespace QtContacts;

EdsHelper::EdsHelper(QObject *parent, const QString &organizerManager)
    : QObject(parent),
      m_freezed(false)
{
    qRegisterMetaType<QList<QOrganizerItemId> >("QList<QOrganizerItemId>");
    qRegisterMetaType<QList<QOrganizerItemDetail::DetailType>>("QList<QOrganizerItemDetail::DetailType>");

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
                                const QString &remoteId,
                                bool writable,
                                int accountId)
{
    if (!m_organizerEngine) {
        qWarning() << "Request to create an organizer source with a null organize engine";
        return QString();
    }

    EdsSource source = sourceByRemoteId(remoteId, accountId);
    if (!source.id.isEmpty()) {
        return source.id;
    }

    QOrganizerCollection collection;
    collection.setMetaData(QOrganizerCollection::KeyName, sourceName);
    collection.setMetaData(QOrganizerCollection::KeyColor, sourceColor);
    collection.setExtendedMetaData(COLLECTION_REMOTE_ID_METADATA, remoteId);
    collection.setExtendedMetaData(COLLECTION_ACCOUNT_ID_METADATA, accountId);
    collection.setExtendedMetaData(COLLECTION_SELECTED_METADATA, true);
    collection.setExtendedMetaData(COLLECTION_SYNC_READONLY_METADATA, !writable);

    if (!m_organizerEngine->saveCollection(&collection)) {
        qWarning() << "Fail to create collection" << sourceName << m_organizerEngine->error();
        return QString();
    } else {
        return sourceFromCollectionId(collection.id());
    }
}


void EdsHelper::removeSource(const QString &sourceId)
{
    if (!m_organizerEngine) {
        qWarning() << "Request to remove organizer source with a null organize engine";
        return;
    }

    QOrganizerCollectionId id(sourceToCollectionId(sourceId));

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
            return sourceFromCollectionId(c.id());
        }
    }
    return QString();
}

EdsSource EdsHelper::sourceByRemoteId(const QString &remoteId, uint account)
{
    Q_FOREACH(const QOrganizerCollection &c, m_organizerEngine->collections()) {
        if ((c.extendedMetaData(COLLECTION_ACCOUNT_ID_METADATA) == account) &&
            (c.extendedMetaData(COLLECTION_REMOTE_ID_METADATA).toString() == remoteId)) {
            EdsSource s;
            s.id = sourceFromCollectionId(c.id());
            s.name = c.metaData(QOrganizerCollection::KeyName).toString();
            s.account = account;
            s.remoteId = remoteId;
            return s;
        }
    }
    return EdsSource();
}

EdsSource EdsHelper::sourceById(const QString &id)
{
    Q_FOREACH(const QOrganizerCollection &c, m_organizerEngine->collections()) {
        if (c.id().localId() == id) {
            EdsSource s;
            s.id = sourceFromCollectionId(c.id());
            s.name = c.metaData(QOrganizerCollection::KeyName).toString();
            s.account = c.extendedMetaData(COLLECTION_ACCOUNT_ID_METADATA).toUInt();
            s.remoteId = c.extendedMetaData(COLLECTION_REMOTE_ID_METADATA).toString();
            return s;
        }
    }
    return EdsSource();
}

QString
EdsHelper::sourceFromCollectionId(const QOrganizerCollectionId &collectionId) const
{
    return QString::fromUtf8(collectionId.localId());
}

QOrganizerCollectionId
EdsHelper::sourceToCollectionId(const QString &sourceId) const
{
    return QOrganizerCollectionId(m_organizerEngine->managerUri(), sourceId.toUtf8());
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
            connect(m_organizerEngine, &QOrganizerManager::itemsAdded,
                    this, &EdsHelper::calendarChanged, Qt::QueuedConnection);
            connect(m_organizerEngine, &QOrganizerManager::itemsRemoved,
                    this, &EdsHelper::calendarChanged, Qt::QueuedConnection);
            connect(m_organizerEngine, &QOrganizerManager::itemsChanged,
                    this, &EdsHelper::calendarChanged, Qt::QueuedConnection);
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
        sources << sourceFromCollectionId(collection.id());
        result.insert(accountId, sources);
    }

    return result;
}

QString EdsHelper::getCollectionIdFromItemId(const QOrganizerItemId &itemId) const
{
    return QString::fromUtf8(itemId.localId().split('/').first());
}

void EdsHelper::calendarChanged(const QList<QOrganizerItemId> &itemIds)
{
    Q_ASSERT(m_organizerEngine);

    if (m_timeoutTimer.isActive()) {
        // ignore changes just after sync
        return;
    }

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
