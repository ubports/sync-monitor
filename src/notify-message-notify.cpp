#include "notify-message.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <libnotify/notify.h>

static void notificationClosed(NotifyNotification *notification, gpointer data);

NotifyMessage::NotifyMessage()
{
    qDebug() << "Notify init";
    notify_init(QCoreApplication::instance()->applicationName().toUtf8());
}

NotifyMessage::~NotifyMessage()
{
    notify_uninit();
}

void NotifyMessage::show(const QString &title, const QString &msg)
{
    NotifyNotification *notify = notify_notification_new(title.toUtf8().data(),
                                                         msg.toUtf8().data(), 0);
    notify_notification_set_timeout(notify, NOTIFY_EXPIRES_DEFAULT);
    notify_notification_show(notify, 0);
    g_signal_connect_after(notify,
                           "closed",
                           (GCallback)notificationClosed,
                           0);
}

void notificationClosed(NotifyNotification *notification, gpointer data)
{
    g_object_unref(notification);
}
