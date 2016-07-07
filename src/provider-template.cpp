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

#include "provider-template.h"

#include <QtCore/QDir>
#include <QtCore/QStringList>
#include <QtCore/QDebug>

#include "config.h"

ProviderTemplate::ProviderTemplate()
{
    m_baseDir = qgetenv("SYNC_MONITOR_TEMPLATE_PATH");
    if (m_baseDir.isEmpty()) {
        m_baseDir = QString(PROVIDER_TEMPLATE_PATH);
    } else {
        qDebug() << "loading templates from a alternative path: " << m_baseDir;
    }
}

ProviderTemplate::~ProviderTemplate()
{
    qDeleteAll(m_providers.values());
    m_providers.clear();
}

void ProviderTemplate::load()
{
    if (!m_providers.isEmpty()) {
        qWarning() << "ProviderTemplate already loaded";
        return;
    }

    QDir templateDir(PROVIDER_TEMPLATE_PATH);
    if (templateDir.exists()) {
        Q_FOREACH(QString templateFile, templateDir.entryList(QStringList() << "*.conf")) {
            QSettings *settings = new QSettings(templateDir.absoluteFilePath(templateFile), QSettings::IniFormat);
            QString providerName = templateFile.replace(".conf", "");
            m_providers.insert(providerName, settings);
        }
    } else {
        qWarning() << "Template directory does not exists.";
    }
    qDebug() << "Loaded tempaltes:" << m_providers.keys();
}

bool ProviderTemplate::contains(const QString &provider) const
{
    return m_providers.contains(provider);
}

QStringList ProviderTemplate::supportedServices(const QString &provider) const
{
    QStringList result;

    if (provider.isEmpty()) {
        Q_FOREACH(const QSettings* s, m_providers.values()) {
            result << s->childGroups();
        }
    } else if (m_providers.contains(provider)) {
        result << m_providers[provider]->childGroups();
    }
    result.removeDuplicates();
    result.removeOne(GLOBAL_CONFIG_GROUP);
    return result;
}

QSettings *ProviderTemplate::settings(const QString &provider) const
{
    return m_providers.value(provider, 0);
}

QStringList ProviderTemplate::providers() const
{
    return m_providers.keys();
}
