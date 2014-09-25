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

int NotifyMessage::m_instanceCount = 0;

NotifyMessage::NotifyMessage(bool singleMessage, QObject *parent)
    : QObject(parent),
      m_notification(0),
      m_singleMessage(singleMessage)
{
    if (m_instanceCount == 0) {
        m_instanceCount++;
        notify_init(QCoreApplication::instance()->applicationName().toUtf8());
    }
}

NotifyMessage::~NotifyMessage()
{
    if (m_notification) {
        g_object_unref(m_notification);
        m_notification = 0;
    }

    m_instanceCount--;
    if (m_instanceCount == 0) {
        notify_uninit();
    }

}

void NotifyMessage::show(const QString &title, const QString &msg, const QString &iconName)
{
    m_notification = notify_notification_new(title.toUtf8().data(),
                                             msg.toUtf8().data(),
                                             iconName.isEmpty() ? (const char*) 0 : iconName.toUtf8().constData());

    notify_notification_set_timeout(m_notification, NOTIFY_EXPIRES_DEFAULT);
    notify_notification_show(m_notification, 0);
    g_signal_connect_after(m_notification,
                           "closed",
                           (GCallback) NotifyMessage::onNotificationClosed,
                           this);
}

void NotifyMessage::askYesOrNo(const QString &title,
                               const QString &msg,
                               const QString &iconName)
{
    m_notification = notify_notification_new(title.toUtf8().data(),
                                             msg.toUtf8().data(),
                                             iconName.isEmpty() ? (const char*) 0 : iconName.toUtf8().constData());
    notify_notification_set_hint_string(m_notification,
                                        "x-canonical-snap-decisions",
                                        "true");
    notify_notification_set_hint_string(m_notification,
                                        "x-canonical-private-button-tint",
                                        "true");
    notify_notification_set_hint_string(m_notification,
                                        "x-canonical-non-shaped-icon",
                                        "true");
    notify_notification_add_action(m_notification,
                                   "action_accept", "Yes",
                                   (NotifyActionCallback) NotifyMessage::onQuestionAccepted,
                                   this,
                                   NULL);
    notify_notification_add_action(m_notification,
                                   "action_reject", "No",
                                   (NotifyActionCallback) NotifyMessage::onQuestionRejected,
                                   this,
                                   NULL);
    notify_notification_show(m_notification, 0);
    g_signal_connect_after(m_notification,
                           "closed",
                           (GCallback) NotifyMessage::onNotificationClosed,
                           this);
}

void NotifyMessage::onQuestionAccepted(NotifyNotification *notification, char *action, NotifyMessage *self)
{
    Q_EMIT self->questionAccepted();
}

void NotifyMessage::onQuestionRejected(NotifyNotification *notification, char *action, NotifyMessage *self)
{
    Q_EMIT self->questionRejected();
}

void NotifyMessage::onNotificationClosed(NotifyNotification *notification, NotifyMessage *self)
{
    Q_EMIT self->messageClosed();
    if (self->m_singleMessage) {
        self->deleteLater();
    }
}
