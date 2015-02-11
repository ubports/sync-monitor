/*
 * Copyright (C) 2015 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QUrlQuery>
#include <QGuiApplication>
#include <QQuickView>
#include <QQmlEngine>
#include <QQmlContext>
#include <QDebug>

QVariantMap parseAccountArg(QStringList args)
{
    //syncmonitor:///authenticate?id=%1&service=%2
    Q_FOREACH(const QString &arg, args) {
        if (arg.startsWith("syncmonitorhelper:///")) {
            QUrl url = QUrl::fromPercentEncoding(arg.toUtf8());
            QString methodName = url.path().right(url.path().length() -1);
            if (methodName != "authenticate") {
                return QVariantMap();
            }

            //convert items to map
            QUrlQuery query(url);
            QList<QPair<QString, QString> > queryItemsPair = query.queryItems();
            QMap<QString, QString> queryItems;
            for(int i=0; i < queryItemsPair.count(); i++) {
                QPair<QString, QString> item = queryItemsPair[i];
                queryItems.insert(item.first, item.second);
            }

            if (queryItems.contains("id") && queryItems.contains("service")) {
                QVariantMap info;
                info.insert("accountId", queryItems.value("id"));
                info.insert("serviceName", queryItems.value("service"));
                return info;
            } else {
                return QVariantMap();
            }
        }
    }
    return QVariantMap();
}

int main(int argc, char **argv)
{
    QCoreApplication::setOrganizationName("Canonical");
    QCoreApplication::setOrganizationDomain("canonical.com");
    QCoreApplication::setApplicationName("Sync Monitor Helper");

    QGuiApplication *app = new QGuiApplication(argc, argv);
    QVariantMap accountInfo = parseAccountArg(app->arguments());
    if (accountInfo.isEmpty()) {
        qWarning() << "Usage: sync-monitor-helper syncmonitorhelper:///authenticate?id=<accountId>&service=<serviceName";
        delete app;
        return -1;
    }

    QQuickView *view = new QQuickView;
    app->connect(view->engine(), SIGNAL(quit()), SLOT(quit()));

    view->setResizeMode(QQuickView::SizeRootObjectToView);
    view->setTitle("Sync Monitor");
    view->rootContext()->setContextProperty("ONLINE_ACCOUNT", accountInfo);
    view->setSource(QUrl("qrc:/main.qml"));

    qDebug() << accountInfo;
    view->show();
    app->exec();
    delete view;
    delete app;
    return 0;
}
