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

#ifndef __PROVIDER_TEMPLATE_H__
#define __PROVIDER_TEMPLATE_H__

#include <QtCore/QObject>
#include <QtCore/QSettings>

class ProviderTemplate
{
public:
    ProviderTemplate();
    ~ProviderTemplate();

    void load();

    bool contains(const QString &provider) const;
    QStringList supportedServices(const QString &provider) const;
    QSettings *settings(const QString &provider) const;

private:
    QMap<QString, QSettings*> m_providers;
    QString m_baseDir;
};

#endif
