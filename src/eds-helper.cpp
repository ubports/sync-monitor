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
#define COLLECTION_ACCOUNT_ID_METADATA  "collection-account-id"

using namespace QtOrganizer;
using namespace QtContacts;

EdsHelper::EdsHelper(QObject *parent,
                     const QString &contactManager,
                     const QString &organizerManager)
    : QObject(parent),
      m_freezed(false)
{
    qRegisterMetaType<QList<QOrganizerItemId> >("QList<QOrganizerItemId>");
    qRegisterMetaType<QList<QContactId> >("QList<QContactId>");

    if (!contactManager.isEmpty()) {
        m_contactEngine = new QContactManager(contactManager, QMap<QString, QString>());
    } else {
        m_contactEngine = 0;
    }
    if (!organizerManager.isEmpty()) {
        m_organizerEngine = new QOrganizerManager(organizerManager, QMap<QString, QString>());
    } else {
        m_organizerEngine = 0;
    }

    m_timeoutTimer.setSingleShot(true);
}

EdsHelper::~EdsHelper()
{
    delete m_contactEngine;
    delete m_organizerEngine;
}

QString EdsHelper::createSource(const QString &serviceName, const QString &sourceName, int accountId)
{
    if (serviceName == CONTACTS_SERVICE_NAME) {
        return createContactsSource(sourceName);
    } else if (serviceName == CALENDAR_SERVICE_NAME) {
        return createOrganizerSource(sourceName, accountId);
    } else {
        qWarning() << "Service not supported:" << serviceName;
        return QString();
    }
}


void EdsHelper::removeSource(const QString &serviceName, const QString &sourceName, int accountId)
{
    if (serviceName.isEmpty() || (serviceName == CONTACTS_SERVICE_NAME)) {
        removeContactsSource(sourceName);
    }

    if (serviceName.isEmpty() || (serviceName == CALENDAR_SERVICE_NAME)) {
        removeOrganizerSource(sourceName, accountId);
    }
}

void EdsHelper::removeSource(const QString &serviceName, const QString &sourceId)
{
    if (serviceName == CONTACTS_SERVICE_NAME) {
        removeContactsSource(sourceId);
    }

    if (serviceName == CALENDAR_SERVICE_NAME) {
        removeOrganizerSource(sourceId);
    }
}

QString EdsHelper::sourceId(const QString &serviceName, const QString &sourceName, int accountId)
{
    if (serviceName == CONTACTS_SERVICE_NAME) {
        return contactsSourceId(sourceName, accountId);
    } else if (serviceName == CALENDAR_SERVICE_NAME) {
        return organizerSourceId(sourceName, accountId);
    } else {
        qWarning() << "Service not supported:" << serviceName;
        return QString();
    }
}

void EdsHelper::freezeNotify()
{
    m_freezed = true;
}

void EdsHelper::unfreezeNotify()
{
    m_pendingContacts.clear();
    m_pendingCalendars.clear();
    m_freezed = false;
    m_timeoutTimer.start(CHANGE_TIMEOUT);
}

void EdsHelper::flush()
{
    m_freezed = false;

    if (m_contactEngine) {
        contactChangedFilter(m_pendingContacts.toList());
    }
    m_pendingContacts.clear();

    if (m_organizerEngine) {
        Q_FOREACH(const QString &calendar, m_pendingCalendars) {
            Q_EMIT dataChanged(CALENDAR_SERVICE_NAME, calendar);
        }
    }
    m_pendingCalendars.clear();
}

void EdsHelper::setEnabled(bool enabled)
{
    if (enabled) {
        if (m_contactEngine) {
            connect(m_contactEngine,
                    SIGNAL(contactsAdded(QList<QContactId>)),
                    SLOT(contactChangedFilter(QList<QContactId>)),
                    Qt::QueuedConnection);
            connect(m_contactEngine,
                    SIGNAL(contactsChanged(QList<QContactId>)),
                    SLOT(contactChangedFilter(QList<QContactId>)),
                    Qt::QueuedConnection);
            connect(m_contactEngine,
                    SIGNAL(contactsRemoved(QList<QContactId>)),
                    SLOT(contactChanged()),
                    Qt::QueuedConnection);
            connect(m_contactEngine,
                    SIGNAL(dataChanged()),
                    SLOT(contactDataChanged()),
                    Qt::QueuedConnection);
        }

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
            connect(m_organizerEngine,
                    SIGNAL(collectionsModified(QList<QPair<QOrganizerCollectionId,QOrganizerManager::Operation> >)),
                    SLOT(calendarCollectionsChanged()));
        }
    } else {
        if (m_contactEngine) {
            m_contactEngine->disconnect(this);
        }
        if (m_organizerEngine) {
            m_organizerEngine->disconnect(this);
        }
    }
}

QMap<int, QStringList> EdsHelper::sources(const QString &serviceName)
{
    if (serviceName == CONTACTS_SERVICE_NAME) {
        return contactsSources();
    }

    if (serviceName == CALENDAR_SERVICE_NAME) {
        return organizerSources();
    }

    return QMap<int, QStringList>();
}

void EdsHelper::contactChangedFilter(const QList<QContactId>& contactIds)
{
    Q_ASSERT(m_contactEngine);

    if (m_freezed) {
        m_pendingContacts += contactIds.toSet();
    } else {
        QContactFetchByIdRequest *request = new QContactFetchByIdRequest(m_contactEngine);
        request->setManager(m_contactEngine);
        request->setIds(contactIds);

        QContactFetchHint hint;
        hint.setDetailTypesHint(QList<QContactDetail::DetailType>() << QContactDetail::TypeSyncTarget);
        request->setFetchHint(hint);
        connect(request, SIGNAL(stateChanged(QContactAbstractRequest::State)),
                SLOT(contactFetchStateChanged(QContactAbstractRequest::State)));
        request->start();
    }
}

void EdsHelper::contactFetchStateChanged(QContactAbstractRequest::State newState)
{
    Q_ASSERT(m_contactEngine);

    if ((newState == QContactAbstractRequest::ActiveState) ||
        (newState == QContactAbstractRequest::InactiveState)) {
        return;
    }

    QContactFetchByIdRequest *request = qobject_cast<QContactFetchByIdRequest*>(QObject::sender());
    if (newState == QContactAbstractRequest::FinishedState) {
        QSet<QString> sources;
        Q_FOREACH(const QContact &contact, request->contacts()) {
            QContactSyncTarget syncTarget = contact.detail<QContactSyncTarget>();
            if (!syncTarget.syncTarget().isEmpty()) {
                sources << syncTarget.syncTarget();
            }
        }

        Q_FOREACH(const QString &source, sources) {
            contactChanged(source);
        }
    }

    request->deleteLater();
}

void EdsHelper::calendarCollectionsChanged()
{
    m_calendarCollections.clear();
}

QString EdsHelper::getCollectionIdFromItemId(const QOrganizerItemId &itemId) const
{
    return itemId.toString().split("/").first();
}

void EdsHelper::contactChanged(const QString& sourceName)
{
    if (!m_timeoutTimer.isActive()) {
        Q_EMIT dataChanged(CONTACTS_SERVICE_NAME, sourceName);
    } else {
        qDebug() << "Ignore contact changed:" << sourceName;
    }
}

void EdsHelper::contactDataChanged()
{
    // The dataChanged signal is fired during the server startup.
    // Some contact data is loaded async like Avatar, a signal with contactChanged will be fired
    // late during the server startup. Because of that We will wait for some time before start to
    // accept contact changes signals, to avoid unnecessary syncs.
    m_timeoutTimer.start(CHANGE_TIMEOUT);
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

QString EdsHelper::createContactsSource(const QString &sourceName)
{
    if (!m_contactEngine) {
        qWarning() << "Request to create contact source with null engine";
        return QString();
    }

    // filter all contact groups/addressbook
    QContactDetailFilter filter;
    filter.setDetailType(QContactDetail::TypeType, QContactType::FieldType);
    filter.setValue(QContactType::TypeGroup);

    // check if the source already exists
    QList<QContact> sources = m_contactEngine->contacts(filter);
    Q_FOREACH(const QContact &contact, sources) {
        if (contact.detail<QContactDisplayLabel>().label() == sourceName) {
            return QString();
        }
    }

    // create a new source
    QContact contact;
    contact.setType(QContactType::TypeGroup);

    QContactDisplayLabel label;
    label.setLabel(sourceName);
    contact.saveDetail(&label);

    // set the new source as default if there is only the local source
    if (sources.size() == 1) {
        QContactExtendedDetail isDefault;
        isDefault.setName("IS-PRIMARY");
        isDefault.setData(true);
        contact.saveDetail(&isDefault);
    }

    if (!m_contactEngine->saveContact(&contact)) {
        qWarning() << "Fail to create contact source:" << sourceName;
        return QString();
    } else {
        contact.id().toString();
    }
}
// use empty sourceName to remove all sources
void EdsHelper::removeOrganizerSource(const QString &sourceName, int accountId)
{
    if (!m_organizerEngine) {
        qWarning() << "Request to remove organizer source with a null organize engine";
        return;
    }

    QList<QPair<QOrganizerCollectionId, QString> > collectionsToRemove;
    QList<QOrganizerCollection> result = m_organizerEngine->collections();
    Q_FOREACH(const QOrganizerCollection &collection, result) {
        if ((sourceName.isEmpty() || (collection.metaData(QOrganizerCollection::KeyName).toString() == sourceName)) &&
            (collection.extendedMetaData(COLLECTION_ACCOUNT_ID_METADATA).toInt() == accountId)) {
            collectionsToRemove << qMakePair(collection.id(), collection.metaData(QOrganizerCollection::KeyName).toString());
        }
    }

    while (!collectionsToRemove.isEmpty()) {
        QPair<QOrganizerCollectionId, QString> collection = collectionsToRemove.takeFirst();
        if (!m_organizerEngine->removeCollection(collection.first)) {
            qWarning() << "Fail to remove calendar source" <<  collection.second;
        } else {
            qDebug() << "Collection removed:" << collection.second;
        }
    }
}

void EdsHelper::removeContactsSource(const QString &sourceName)
{
    if (!m_contactEngine) {
        qWarning() << "Request to remove contact source with a null contact engine";
        return;
    }

    // check source Id
    QContactId sourceId;
    QContactDetailFilter filter;
    filter.setDetailType(QContactDetail::TypeType, QContactType::FieldType);
    filter.setValue(QContactType::TypeGroup);

    // check if the source already exists
    QList<QContact> sources = m_contactEngine->contacts(filter);
    Q_FOREACH(const QContact &contact, sources) {
        if (contact.detail<QContactDisplayLabel>().label() == sourceName) {
            sourceId = contact.id();
            break;
        }
    }

    if (sourceId.isNull() || !m_contactEngine->removeContact(sourceId)) {
        qWarning() << "Fail to remove contact source:" << sourceName;
    }
}

void EdsHelper::removeOrganizerSource(const QString &sourceId)
{
    QOrganizerCollectionId id = QOrganizerCollectionId::fromString(sourceId);
    if (!m_organizerEngine->removeCollection(id)) {
        qWarning() << "Fail to remove calendar source" <<  id;
    } else {
        qDebug() << "Collection removed:" << id;
    }
}

QMap<int, QStringList> EdsHelper::contactsSources()
{
    qWarning() << "Not supported";
    return QMap<int, QStringList>();
}

QMap<int, QStringList> EdsHelper::organizerSources()
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

QString EdsHelper::contactsSourceId(const QString &sourceName, int accountId)
{
    qWarning() << "Not supported";
    return QString();
}

QString EdsHelper::organizerSourceId(const QString &sourceName, int accountId)
{
    QList<QOrganizerCollection> collections = m_organizerEngine->collections();
    Q_FOREACH(const QOrganizerCollection &collection, collections) {
        if ((collection.metaData(QOrganizerCollection::KeyName).toString() == sourceName) &&
            ((accountId == -1) || (collection.extendedMetaData(COLLECTION_ACCOUNT_ID_METADATA) == accountId))) {
            return collection.id().toString();
        }
    }

    return QString();
}

QString EdsHelper::createOrganizerSource(const QString &sourceName, int accountId)
{
    if (!m_organizerEngine) {
        qWarning() << "Request to create an organizer source with a null organize engine";
        return QString();
    }

    QString sourceId = organizerSourceId(sourceName, accountId);
    if (!sourceId.isEmpty()) {
        return sourceId;
    }

    QOrganizerCollection collection;
    collection.setMetaData(QOrganizerCollection::KeyName, sourceName);
    collection.setExtendedMetaData(COLLECTION_ACCOUNT_ID_METADATA, accountId);
    collection.setExtendedMetaData("collection-selected", true);
    if (!m_organizerEngine->saveCollection(&collection)) {
        qWarning() << "Fail to create collection" << sourceName;
        return QString();
    } else {
        return collection.id().toString();
    }
}
