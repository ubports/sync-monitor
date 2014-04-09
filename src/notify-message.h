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

#include <QtCore/QString>
#include <QtCore/QObject>

#include <libnotify/notify.h>

class NotifyMessage : public QObject
{
    Q_OBJECT
public:
    NotifyMessage(bool singleMessage, QObject *parent = 0);
    ~NotifyMessage();

    void show(const QString &title, const QString &msg, const QString &iconName);
    void askYesOrNo(const QString &title, const QString &msg, const QString &iconName);

Q_SIGNALS:
    void questionAccepted();
    void questionRejected();
    void messageClosed();

private:
    NotifyNotification *m_notification;
    bool m_singleMessage;

    static int m_instanceCount;

    static void onQuestionAccepted(NotifyNotification *notification,
                                   char *action,
                                   NotifyMessage *self);
    static void onQuestionRejected(NotifyNotification *notification,
                                   char *action,
                                   NotifyMessage *self);
    static void onNotificationClosed(NotifyNotification *notification,
                                     NotifyMessage *self);
};
