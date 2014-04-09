#include "notify-message.h"

#include <QtCore/QDebug>

NotifyMessage::NotifyMessage()
{
}

NotifyMessage::~NotifyMessage()
{
}

void NotifyMessage::show(const QString &title, const QString &msg, const QString &icon)
{
    Q_UNUSED(icon);
    qDebug() << title << "\t" << msg;
}

