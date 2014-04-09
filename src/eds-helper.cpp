#include "eds-helper.h"

#include <QtCore/QDebug>

#include <QtOrganizer/QOrganizerManager>
#include <QtOrganizer/QOrganizerCollection>

#include <QtContacts/QContactManager>
#include <QtContacts/QContactDisplayLabel>
#include <QtContacts/QContactDetailFilter>
#include <QtContacts/QContactFetchByIdRequest>
#include <QtContacts/QContactSyncTarget>

#include "config.h"

#define CHANGE_TIMEOUT      1000

using namespace QtOrganizer;
using namespace QtContacts;

EdsHelper::EdsHelper(QObject *parent)
    : QObject(parent)
{
    m_timeoutTimer.setSingleShot(true);
    m_contactEngine = new QContactManager("galera", QMap<QString, QString>());
    connect(m_contactEngine, &QContactManager::contactsAdded,
            this, &EdsHelper::contactChangedFilter);
    connect(m_contactEngine, &QContactManager::contactsChanged,
            this, &EdsHelper::contactChangedFilter);
    connect(m_contactEngine, &QContactManager::contactsRemoved,
            this, &EdsHelper::contactChanged);
    connect(m_contactEngine, &QContactManager::dataChanged,
            this, &EdsHelper::contactDataChanged);

    m_organizerEngine = new QOrganizerManager("eds", QMap<QString, QString>());
    connect(m_organizerEngine, &QOrganizerManager::itemsAdded,
            this, &EdsHelper::calendarChanged);
    connect(m_organizerEngine, &QOrganizerManager::itemsRemoved,
            this, &EdsHelper::calendarChanged);
    connect(m_organizerEngine, &QOrganizerManager::itemsChanged,
            this, &EdsHelper::calendarChanged);
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

void EdsHelper::contactChangedFilter(const QList<QContactId>& contactIds)
{
    QContactFetchByIdRequest *request = new QContactFetchByIdRequest(m_contactEngine);
    request->setManager(m_contactEngine);
    request->setIds(contactIds);
    connect(request, &QContactFetchByIdRequest::stateChanged,
            this, &EdsHelper::contactFetchStateChanged);
    request->start();
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

void EdsHelper::contactChanged()
{
    if (!m_timeoutTimer.isActive()) {
        Q_EMIT dataChanged(CONTACTS_SERVICE_NAME, "");
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
    QSet<QString> uniqueColletions;
    QList<QOrganizerCollection> collections = m_organizerEngine->collections();

    // eds item ids cotains the collection id we can use that instead of query for the full item
    Q_FOREACH(const QOrganizerItemId &id, itemIds) {
        QString collectionId = id.toString().split("/").first();
        uniqueColletions << collectionId;
    }

    Q_FOREACH(const QString &collectionId, uniqueColletions) {
        Q_FOREACH(const QOrganizerCollection &collection, collections) {
            if (collection.id().toString() == collectionId) {
                Q_EMIT dataChanged(CALENDAR_SERVICE_NAME, collection.metaData(QOrganizerCollection::KeyName).toString());
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
    Q_FOREACH(const QContact &contact, m_contactEngine->contacts(filter)) {
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

    if (!m_contactEngine->saveContact(&contact)) {
        qWarning() << "Fail to create contact source:" << sourceName;
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
