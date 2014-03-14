#include "eds-helper.h"

#include <QtCore/QDebug>

#include <QtOrganizer/QOrganizerManager>
#include <QtOrganizer/QOrganizerCollection>

#include <QtContacts/QContactManager>
#include <QtContacts/QContactDisplayLabel>
#include <QtContacts/QContactDetailFilter>

#include "config.h"

using namespace QtOrganizer;
using namespace QtContacts;

EdsHelper::EdsHelper(QObject *parent)
    : QObject(parent)
{
    m_contactEngine = new QContactManager("galera", QMap<QString, QString>());
    connect(m_contactEngine, &QContactManager::contactsAdded,
            this, &EdsHelper::contactChanged);
    connect(m_contactEngine, &QContactManager::contactsRemoved,
            this, &EdsHelper::contactChanged);
    connect(m_contactEngine, &QContactManager::contactsChanged,
            this, &EdsHelper::contactChanged);

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

void EdsHelper::contactChanged()
{
    Q_EMIT dataChanged(CONTACTS_SERVICE_NAME, "");
}

void EdsHelper::calendarChanged()
{
    Q_EMIT dataChanged(CALENDAR_SERVICE_NAME, "");
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
