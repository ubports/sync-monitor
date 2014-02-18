/*
 * Copyright 2014 Canonical Ltd.
 *
 * This file is part of sync-monitor.
 *
 * sync-monitor is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * contact-service-app is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
