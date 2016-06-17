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
        SyncQueue queue;
        SyncAccountMock account(1);

        // empty queue
        QCOMPARE(queue.isEmpty(), true);
        QCOMPARE(queue.count(), 0);

        // push a account
        queue.push(&account);
        QCOMPARE(queue.isEmpty(), false);
        QCOMPARE(queue.count(), 1);

        // check if queue accepts any source
        QVERIFY(queue.contains(&account, "my source"));
        QVERIFY(queue.contains(&account, "my source2"));
    }

    void testRegiterTwoAccounts()
    {
        SyncQueue queue;
        SyncAccountMock account(1);
        SyncAccountMock account2(2);

        queue.push(&account);
        queue.push(&account2);

        // Check if the account was registered correct
        QCOMPARE(queue.isEmpty(), false);
        QCOMPARE(queue.count(), 2);
        QVERIFY(queue.contains(&account, "my source"));
        QVERIFY(queue.contains(&account2, "my source2"));
    }

    void testRemoveAccount()
    {
        SyncQueue queue;
        SyncAccountMock account(1);
        SyncAccountMock account2(2);

        queue.push(&account);
        queue.push(&account2);

        // remove all services from account
        queue.remove(&account);

        // Check if the account was registered correct
        QCOMPARE(queue.isEmpty(), false);
        QCOMPARE(queue.count(), 1);
        QVERIFY(!queue.contains(&account, "my source"));
        QVERIFY(queue.contains(&account2, "my source2"));
    }

    void testPushAccountTwice()
    {
        SyncQueue queue;
        SyncAccountMock account(1);

        // push a account twice
        queue.push(&account);
        queue.push(&account);

        // Check if the account was registered only once
        QCOMPARE(queue.isEmpty(), false);
        QCOMPARE(queue.count(), 1);
        QVERIFY(queue.contains(&account, "my source"));
    }

    void testPopAccount()
    {
        SyncQueue queue;
        SyncAccountMock account(1);
        SyncAccountMock account2(2);

        queue.push(&account);
        queue.push(&account2, QStringLiteral("account2Source"), false);

        // Firs account with all sources
        SyncJob job = queue.popNext();
        QCOMPARE(queue.count(), 1);
        QCOMPARE(job.account()->id(), account.id());
        QCOMPARE(job.sources().size(), 0);
        QVERIFY(job.contains("my source1"));
        QVERIFY(job.contains("my source2"));

        // account2 with a single source
        job = queue.popNext();
        QCOMPARE(queue.count(), 0);
        QCOMPARE(job.account()->id(), account2.id());
        QCOMPARE(job.sources().size(), 1);
        QVERIFY(job.contains(QStringLiteral("account2Source")));
        QVERIFY(!job.contains(QStringLiteral("my source")));
    }

    void testAppendMoreSources()
    {
        SyncQueue queue;
        SyncAccountMock account(1);
        queue.push(&account, QStringLiteral("account1Source0"), false);
        queue.push(&account, QStringLiteral("account1Source1"), false);

        QCOMPARE(queue.count(), 1);
        QVERIFY(queue.contains(&account, QStringLiteral("account1Source0")));
        QVERIFY(queue.contains(&account, QStringLiteral("account1Source1")));
    }

    void testAppendMoreSourcesAfterAddAccount()
    {
        SyncQueue queue;
        SyncAccountMock account(1);
        queue.push(&account);
        queue.push(&account, QStringLiteral("account1Source0"), false);
        QCOMPARE(queue.count(), 1);
        queue.remove(&account);
        QCOMPARE(queue.count(), 0);
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
