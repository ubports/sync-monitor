#include "address-book-trigger.h"

#include <QtCore/QDebug>

#define ADDRESS_BOOK_SERVICE_NAME       "com.canonical.pim"
#define ADDRESS_BOOK_OBJECT_PATH        "/com/canonical/pim/AddressBook"
#define ADDRESS_BOOK_IFACE_NAME         "com.canonical.pim.AddressBook"

AddressBookTrigger::AddressBookTrigger(QObject *parent)
    : QObject(parent)
{
    QDBusConnection session = QDBusConnection::sessionBus();

    session.connect(ADDRESS_BOOK_SERVICE_NAME,
                    ADDRESS_BOOK_OBJECT_PATH,
                    ADDRESS_BOOK_IFACE_NAME,
                    "contactsUpdated",
                    this, SLOT(changed(QStringList)));

    session.connect(ADDRESS_BOOK_SERVICE_NAME,
                    ADDRESS_BOOK_OBJECT_PATH,
                    ADDRESS_BOOK_IFACE_NAME,
                    "contactsRemoved",
                    this, SLOT(changed(QStringList)));

    session.connect(ADDRESS_BOOK_SERVICE_NAME,
                    ADDRESS_BOOK_OBJECT_PATH,
                    ADDRESS_BOOK_IFACE_NAME,
                    "contactsAdded",
                    this, SLOT(changed(QStringList)));
}

AddressBookTrigger::~AddressBookTrigger()
{
}

void AddressBookTrigger::createSource(const QString &sourceName)
{
    QDBusInterface iface(ADDRESS_BOOK_SERVICE_NAME,
                         ADDRESS_BOOK_OBJECT_PATH,
                         ADDRESS_BOOK_IFACE_NAME);
    if (iface.lastError().isValid()) {
        qWarning() << "Fail to create addressbook session:" << iface.lastError().message();
    } else {
        iface.call("createSource", sourceName);
    }
}

void AddressBookTrigger::changed(QStringList ids)
{
    qDebug() << "AB CHANGED";
    Q_EMIT contactsUpdated();
}
