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

using namespace QtOrganizer;
using namespace QtContacts;

EdsHelper::EdsHelper(QObject *parent,
                     const QString &contactManager,
                     const QString &organizerManager)
    : QObject(parent),
      m_freezed(false)
{
    qRegisterMetaType<QList<QOrganizerItemId> >("QList<QOrganizerItemId>");

    m_organizerEngine = new QOrganizerManager(organizerManager, QMap<QString, QString>());

    m_timeoutTimer.setSingleShot(true);
}

EdsHelper::~EdsHelper()
{
    delete m_organizerEngine;
}

QString EdsHelper::createSource(const QString &serviceName, const QString &sourceName)
{
    if (serviceName == CALENDAR_SERVICE_NAME) {
        return createOrganizerSource(sourceName);
    } else {
        qWarning() << "Service not supported:" << serviceName;
    }
    return QString();
}

QStringList EdsHelper::sources(const QString &serviceName) const
{
    if (serviceName == CALENDAR_SERVICE_NAME) {
        return organizerSources();
    } else {
        qWarning() << "Service not supported:" << serviceName;
    }
    return QStringList();
}

void EdsHelper::removeSource(const QString &serviceName, const QString &sourceName)
{
    if (serviceName.isEmpty() || (serviceName == CALENDAR_SERVICE_NAME)) {
        removeOrganizerSource(sourceName);
    }
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

    Q_FOREACH(const QString &calendar, m_pendingCalendars) {
        Q_EMIT dataChanged(CALENDAR_SERVICE_NAME, calendar);
    }
    m_pendingCalendars.clear();
}

void EdsHelper::setEnabled(bool enabled)
{
    if (enabled) {
        connect(m_organizerEngine,
                SIGNAL(itemsAdded(QList<QOrganizerItemId>)),
                SLOT(calendarChanged(QList<QOrganizerItemId>)), Qt::QueuedConnection);
        connect(m_organizerEngine,
                SIGNAL(itemsRemoved(QList<QOrganizerItemId>)),
                SLOT(calendarChanged(QList<QOrganizerItemId>)), Qt::QueuedConnection);
        connect(m_organizerEngine,
                SIGNAL(itemsChanged(QList<QOrganizerItemId>)),
                SLOT(calendarChanged(QList<QOrganizerItemId>)), Qt::QueuedConnection);
        connect(m_organizerEngine,
                SIGNAL(collectionsModified(QList<QPair<QOrganizerCollectionId,QOrganizerManager::Operation> >)),
                SLOT(calendarCollectionsChanged()));
    } else {
        m_organizerEngine->disconnect(this);
    }
}

void EdsHelper::calendarCollectionsChanged()
{
    m_calendarCollections.clear();
}

QString EdsHelper::getCollectionIdFromItemId(const QOrganizerItemId &itemId) const
{
    return itemId.toString().split("/").first();
}

void EdsHelper::calendarChanged(const QList<QOrganizerItemId> &itemIds)
{
    QSet<QString> uniqueColletions;

    // eds item ids cotains the collection id we can use that instead of query for the full item
    Q_FOREACH(const QOrganizerItemId &id, itemIds) {
        uniqueColletions << getCollectionIdFromItemId(id);
    }

    if (uniqueColletions.isEmpty()) {
        return;
    }

    if (m_calendarCollections.isEmpty()) {
        m_calendarCollections = m_organizerEngine->collections();
    }

    Q_FOREACH(const QString &collectionId, uniqueColletions) {
        Q_FOREACH(const QOrganizerCollection &collection, m_calendarCollections) {
            if (collection.id().toString() == collectionId) {
                QString collectionName = collection.metaData(QOrganizerCollection::KeyName).toString();
                if (m_freezed) {
                    m_pendingCalendars << collectionName;
                } else {
                    Q_EMIT dataChanged(CALENDAR_SERVICE_NAME, collectionName);
                }
                break;
            }
        }
    }
}

QStringList EdsHelper::organizerSources() const
{
    QStringList ids;

    QList<QOrganizerCollection> collections = m_organizerEngine->collections();
    Q_FOREACH(const QOrganizerCollection &collection, collections) {
        ids << collection.id().toString();
    }

    return ids;
}

void EdsHelper::removeOrganizerSource(const QString &sourceName)
{
    QOrganizerCollectionId collectionId;
    QList<QOrganizerCollection> result = m_organizerEngine->collections();
    Q_FOREACH(const QOrganizerCollection &collection, result) {
        if (collection.metaData(QOrganizerCollection::KeyName).toString() == sourceName) {
            collectionId = collection.id();
            break;
        }
    }

    if (!collectionId.isNull()) {
        if (!m_organizerEngine->removeCollection(collectionId)) {
            qWarning() << "Fail to remove calendar source" <<  sourceName;
        }
    } else {
        qWarning() << "Calendar source not found" << sourceName;
    }
}

QString EdsHelper::createOrganizerSource(const QString &sourceName)
{
    QList<QOrganizerCollection> result = m_organizerEngine->collections();
    Q_FOREACH(const QOrganizerCollection &collection, result) {
        if (collection.metaData(QOrganizerCollection::KeyName).toString() == sourceName) {
            return collection.id().toString();
        }
    }

    QOrganizerCollection collection;
    collection.setMetaData(QOrganizerCollection::KeyName, sourceName);
    if (!m_organizerEngine->saveCollection(&collection)) {
        qWarning() << "Fail to create collection" << sourceName;
        return QString();
    } else {
        return collection.id().toString();
    }
}
