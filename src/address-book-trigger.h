#ifndef __ADDRESS_BOOK_TRIGGER_H__
#define __ADDRESS_BOOK_TRIGGER_H__

#include <QtCore/QObject>
#include <QtCore/QStringList>

#include <QtDBus/QDBusInterface>

class AddressBookTrigger : public QObject
{
    Q_OBJECT
public:
    AddressBookTrigger(QObject *parent = 0);
    ~AddressBookTrigger();

Q_SIGNALS:
    void contactsUpdated();

private Q_SLOTS:
    void changed(QStringList ids);

private:
    QDBusInterface *m_iface;
};

#endif
