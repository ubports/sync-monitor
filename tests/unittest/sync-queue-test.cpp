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

#include "sync-account-mock.h"
#include "src/sync-queue.h"

#include <gmock/gmock.h>

#include <QObject>
#include <QtTest>
#include <QDebug>


class SyncQueueTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testRegisterNewAccount()
    {
        QStringList expectedServices;
        SyncQueue queue;
        SyncAccountMock account;

        expectedServices << "contacts" << "calendar";

        // empty queue
        QCOMPARE(queue.isEmpty(), true);
        QCOMPARE(queue.count(), 0);

        EXPECT_CALL(account, availableServices())
             .Times(1)
             .WillOnce(::testing::Return(expectedServices));

        // push a account
        queue.push(&account);
        QCOMPARE(queue.isEmpty(), false);
        QCOMPARE(queue.count(), 2);
        Q_FOREACH(const QString &serviceName, expectedServices) {
            QVERIFY(queue.contains(&account, serviceName));
        }
    }

    void testRegiterTwoAccounts()
    {
        QStringList expectedServices;
        SyncQueue queue;
        SyncAccountMock account;
        SyncAccountMock account2;

        expectedServices << "contacts" << "calendar";
        EXPECT_CALL(account, availableServices())
             .WillRepeatedly(::testing::Return(expectedServices));
        EXPECT_CALL(account2, availableServices())
             .WillRepeatedly(::testing::Return(expectedServices));

        queue.push(&account);
        queue.push(&account2);

        // Check if the account was registered correct
        QCOMPARE(queue.isEmpty(), false);
        QCOMPARE(queue.count(), 4);
        Q_FOREACH(const QString &serviceName, expectedServices) {
            QVERIFY(queue.contains(&account, serviceName));
            QVERIFY(queue.contains(&account2, serviceName));
        }
    }

    void testRemoveAccount()
    {
        QStringList expectedServices;
        SyncQueue queue;
        SyncAccountMock account;
        SyncAccountMock account2;

        expectedServices << "contacts" << "calendar";
        EXPECT_CALL(account, availableServices())
             .WillRepeatedly(::testing::Return(expectedServices));
        EXPECT_CALL(account2, availableServices())
             .WillRepeatedly(::testing::Return(expectedServices));

        queue.push(&account);
        queue.push(&account2);

        // remove all services from account
        queue.remove(&account);

        // Check if the account was registered correct
        QCOMPARE(queue.isEmpty(), false);
        QCOMPARE(queue.count(), 2);
        Q_FOREACH(const QString &serviceName, expectedServices) {
            QVERIFY(queue.contains(&account2, serviceName));
            QVERIFY(!queue.contains(&account, serviceName));
        }

        // remove calendar services from account2
        queue.remove(&account2, "calendar");

        // Check if the account was registered correct
        QCOMPARE(queue.isEmpty(), false);
        QCOMPARE(queue.count(), 1);
        QVERIFY(queue.contains(&account2, "contacts"));
        QVERIFY(!queue.contains(&account2, "calendar"));
    }

    void testPushAccountTwice()
    {
        QStringList expectedServices;
        SyncQueue queue;
        SyncAccountMock account;

        expectedServices << "contacts" << "calendar";

        EXPECT_CALL(account, availableServices())
             .Times(2)
             .WillRepeatedly(::testing::Return(expectedServices));

        // push a account twice
        queue.push(&account);
        queue.push(&account);

        // Check if the account was registered only once
        QCOMPARE(queue.isEmpty(), false);
        QCOMPARE(queue.count(), 2);
        Q_FOREACH(const QString &serviceName, expectedServices) {
            QVERIFY(queue.contains(&account, serviceName));
        }
    }

    void testPopAccount()
    {
        QStringList expectedServices;
        SyncQueue queue;
        SyncAccountMock account;
        SyncAccountMock account2;

        expectedServices << "contacts" << "calendar";
        EXPECT_CALL(account, availableServices())
             .WillRepeatedly(::testing::Return(expectedServices));
        EXPECT_CALL(account2, availableServices())
             .WillRepeatedly(::testing::Return(expectedServices));

        queue.push(&account);
        queue.push(&account2);

        SyncAccountMock *nextAccount;
        QString serviceName;

        // account with contacts
        serviceName = queue.popNext(reinterpret_cast<SyncAccount**>(&nextAccount));
        QCOMPARE(serviceName, expectedServices.first());
        QVERIFY(nextAccount == &account);

        // account with calendar
        serviceName = queue.popNext(reinterpret_cast<SyncAccount**>(&nextAccount));
        QCOMPARE(serviceName, expectedServices[1]);
        QVERIFY(nextAccount == &account);

        // account2 with contacts
        serviceName = queue.popNext(reinterpret_cast<SyncAccount**>(&nextAccount));
        QCOMPARE(serviceName, expectedServices.first());
        QVERIFY(nextAccount == &account2);

        // acount2 with calendar
        serviceName = queue.popNext(reinterpret_cast<SyncAccount**>(&nextAccount));
        QCOMPARE(serviceName, expectedServices[1]);
        QVERIFY(nextAccount == &account2);
    }


};

int main(int argc, char *argv[])
{
    // The following line causes Google Mock to throw an exception on failure,
    // which will be interpreted by your testing framework as a test failure.
    ::testing::GTEST_FLAG(throw_on_failure) = true;
    ::testing::InitGoogleMock(&argc, argv);

    QCoreApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    SyncQueueTest tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "sync-queue-test.moc"
