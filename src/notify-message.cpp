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

NotifyMessage *NotifyMessage::m_instance = 0;

void NotifyMessage::destroy()
{
    if (m_instance) {
        delete m_instance;
        m_instance = 0;
    }
}

NotifyMessage *NotifyMessage::instance()
{
    if (m_instance == 0) {
        m_instance = new NotifyMessage();
    }
    return m_instance;
}
