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

#define CHANGE_TIMEOUT      1000

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

    m_contactEngine = new QContactManager(contactManager, QMap<QString, QString>());
    m_organizerEngine = new QOrganizerManager(organizerManager, QMap<QString, QString>());
}

EdsHelper::~EdsHelper()
{
    delete m_contactEngine;
    delete m_organizerEngine;
}

void EdsHelper::createSource(const QString &serviceName, const QString &sourceName)
{
    if (serviceName == CONTACTS_SERVICE_NAME) {
        createContactsSource(sourceName);
    } else if (serviceName == CALENDAR_SERVICE_NAME) {
        createOrganizerSource(sourceName);
    } else {
        qWarning() << "Service not supported:" << serviceName;
    }
}


void EdsHelper::removeSource(const QString &serviceName, const QString &sourceName)
{
    if (serviceName.isEmpty() || (serviceName == CONTACTS_SERVICE_NAME)) {
        removeContactsSource(sourceName);
    }

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
    m_freezed = false;
}

void EdsHelper::flush()
{
    m_freezed = false;
    contactChangedFilter(m_pendingContacts.toList());
    m_pendingContacts.clear();

    Q_FOREACH(const QString &calendar, m_pendingCalendars) {
        Q_EMIT dataChanged(CALENDAR_SERVICE_NAME, calendar);
    }
    m_pendingCalendars.clear();
}

void EdsHelper::setEnabled(bool enabled)
{
    if (enabled) {
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
        m_contactEngine->disconnect(this);
        m_organizerEngine->disconnect(this);
    }
}

void EdsHelper::contactChangedFilter(const QList<QContactId>& contactIds)
{
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
            Q_EMIT dataChanged(CONTACTS_SERVICE_NAME, source);
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

void EdsHelper::contactChanged()
{
    Q_EMIT dataChanged(CONTACTS_SERVICE_NAME, "");
}

void EdsHelper::contactDataChanged()
{
    // nothing
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

void EdsHelper::createContactsSource(const QString &sourceName)
{
    // filter all contact groups/addressbook
    QContactDetailFilter filter;
    filter.setDetailType(QContactDetail::TypeType, QContactType::FieldType);
    filter.setValue(QContactType::TypeGroup);

    // check if the source already exists
    QList<QContact> sources = m_contactEngine->contacts(filter);
    Q_FOREACH(const QContact &contact, sources) {
        if (contact.detail<QContactDisplayLabel>().label() == sourceName) {
            return;
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
    }
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

void EdsHelper::removeContactsSource(const QString &sourceName)
{
    QContactId sourceId = QContactId::fromString(QString("qtcontacts:galera::%1").arg(sourceName));
    if (!m_contactEngine->removeContact(sourceId)) {
        qWarning() << "Fail to remove contact source:" << sourceName;
    }
}

void EdsHelper::createOrganizerSource(const QString &sourceName)
{
    QList<QOrganizerCollection> result = m_organizerEngine->collections();
    Q_FOREACH(const QOrganizerCollection &collection, result) {
        if (collection.metaData(QOrganizerCollection::KeyName).toString() == sourceName) {
            return;
        }
    }

    QOrganizerCollection collection;
    collection.setMetaData(QOrganizerCollection::KeyName, sourceName);
    if (!m_organizerEngine->saveCollection(&collection)) {
        qWarning() << "Fail to create collection" << sourceName;
    }
}
