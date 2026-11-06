#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "history/HistoryStore.h"
#include "mpv/MpvEngine.h"

using namespace Qt::Literals::StringLiterals;

// Headless unit tests for the persisted playback history. The storage file
// path is injected per test (QTemporaryDir), the engine is the real libmpv
// core where needed (initialize works without media) and QStandardPaths test
// mode keeps defaultFilePath() away from the user's real app data.
class TestHistoryStore : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void defaultPathIsHistoryJsonInAppData();
    void recordsAndPersistsAcrossInstances();
    void dedupesByPathAndCapsAt50();
    void positionUpdateKeepsOrderAndTimestamp();
    void positionUpdateOfUnknownPathIsNoop();
    void corruptFileRecoversToEmpty();
    void emptyPathIsIgnored();
    void fileLoadedWithoutMediaRecordsNothing();
    void timerTickWithoutMediaIsNoop();
};

void TestHistoryStore::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestHistoryStore::defaultPathIsHistoryJsonInAppData()
{
    const QString path = HistoryStore::defaultFilePath();
    QVERIFY(QFileInfo(path).isAbsolute());
    QVERIFY(path.endsWith(u"history.json"_s));
    // Test mode redirects AppDataLocation away from the user's real data.
    QVERIFY(path.contains(u".qttest"_s));
}

void TestHistoryStore::recordsAndPersistsAcrossInstances()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(u"history.json"_s);

    {
        HistoryStore store(nullptr, file);
        QSignalSpy spy(&store, &HistoryStore::entriesChanged);
        QVERIFY(spy.isValid());

        store.recordPlayback(u"/a/first.mkv"_s, u"First"_s, 0.0, 100.0);
        store.recordPlayback(u"/b/second.mkv"_s, u"Second"_s, 12.5, 200.0);

        QCOMPARE(spy.count(), 2);
        QCOMPARE(store.entries().size(), 2);
        const QVariantMap top = store.entries().at(0).toMap();
        QCOMPARE(top.value(u"path"_s).toString(), u"/b/second.mkv"_s);
        QCOMPARE(top.value(u"title"_s).toString(), u"Second"_s);
        QCOMPARE(top.value(u"position"_s).toDouble(), 12.5);
        QCOMPARE(top.value(u"duration"_s).toDouble(), 200.0);
        const QDateTime played =
            QDateTime::fromString(top.value(u"lastPlayedAt"_s).toString(),
                                  Qt::ISODate);
        QVERIFY(played.isValid());
    }

    // A fresh instance over the same file sees the persisted entries.
    HistoryStore reloaded(nullptr, file);
    QCOMPARE(reloaded.entries().size(), 2);
    QCOMPARE(reloaded.entries().at(0).toMap().value(u"path"_s).toString(),
             u"/b/second.mkv"_s);
    QCOMPARE(reloaded.entries().at(1).toMap().value(u"path"_s).toString(),
             u"/a/first.mkv"_s);
}

void TestHistoryStore::dedupesByPathAndCapsAt50()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    HistoryStore store(nullptr, dir.filePath(u"history.json"_s));

    for (int i = 0; i < 60; ++i)
        store.recordPlayback(u"/m/%1.mkv"_s.arg(i), u"T%1"_s.arg(i), 0.0, i);

    QCOMPARE(store.entries().size(), 50);
    QCOMPARE(store.entries().at(0).toMap().value(u"path"_s).toString(),
             u"/m/59.mkv"_s);

    // Re-playing an older entry moves it to the top instead of duplicating.
    store.recordPlayback(u"/m/10.mkv"_s, u"T10 again"_s, 5.0, 10.0);
    QCOMPARE(store.entries().size(), 50);
    QCOMPARE(store.entries().at(0).toMap().value(u"path"_s).toString(),
             u"/m/10.mkv"_s);

    int occurrences = 0;
    for (const QVariant &entry : store.entries()) {
        if (entry.toMap().value(u"path"_s).toString() == u"/m/10.mkv"_s)
            ++occurrences;
    }
    QCOMPARE(occurrences, 1);

    // The oldest entry fell off the bottom when the cap was reached.
    for (const QVariant &entry : store.entries())
        QVERIFY(entry.toMap().value(u"path"_s).toString() != u"/m/0.mkv"_s);
}

void TestHistoryStore::positionUpdateKeepsOrderAndTimestamp()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    HistoryStore store(nullptr, dir.filePath(u"history.json"_s));

    store.recordPlayback(u"/a/first.mkv"_s, u"First"_s, 0.0, 100.0);
    store.recordPlayback(u"/b/second.mkv"_s, u"Second"_s, 0.0, 200.0);
    const QString stampBefore =
        store.entries().at(0).toMap().value(u"lastPlayedAt"_s).toString();

    QSignalSpy spy(&store, &HistoryStore::entriesChanged);
    store.updateEntryPosition(u"/b/second.mkv"_s, 77.5);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(store.entries().size(), 2); // no re-order, no duplicate
    const QVariantMap top = store.entries().at(0).toMap();
    QCOMPARE(top.value(u"path"_s).toString(), u"/b/second.mkv"_s);
    QCOMPARE(top.value(u"position"_s).toDouble(), 77.5);
    QCOMPARE(top.value(u"lastPlayedAt"_s).toString(), stampBefore);
}

void TestHistoryStore::positionUpdateOfUnknownPathIsNoop()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    HistoryStore store(nullptr, dir.filePath(u"history.json"_s));
    store.recordPlayback(u"/a/first.mkv"_s, u"First"_s, 1.0, 10.0);

    QSignalSpy spy(&store, &HistoryStore::entriesChanged);
    store.updateEntryPosition(u"/unknown.mkv"_s, 42.0);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(store.entries().size(), 1);
    QCOMPARE(store.entries().at(0).toMap().value(u"position"_s).toDouble(), 1.0);
}

void TestHistoryStore::corruptFileRecoversToEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(u"history.json"_s);

    QFile garbage(file);
    QVERIFY(garbage.open(QIODevice::WriteOnly));
    garbage.write("this is not json {{{");
    garbage.close();

    HistoryStore store(nullptr, file);
    QCOMPARE(store.entries().size(), 0);

    // Recording must overwrite the broken file with valid JSON again.
    store.recordPlayback(u"/a/first.mkv"_s, u"First"_s, 0.0, 10.0);
    HistoryStore reloaded(nullptr, file);
    QCOMPARE(reloaded.entries().size(), 1);
}

void TestHistoryStore::emptyPathIsIgnored()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    HistoryStore store(nullptr, dir.filePath(u"history.json"_s));

    store.recordPlayback(QString(), u"No path"_s, 0.0, 0.0);
    store.updateEntryPosition(QString(), 5.0);

    QCOMPARE(store.entries().size(), 0);
}

void TestHistoryStore::fileLoadedWithoutMediaRecordsNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    MpvEngine engine;
    QVERIFY(engine.initialize());

    HistoryStore store(&engine, dir.filePath(u"history.json"_s));
    QSignalSpy spy(&store, &HistoryStore::entriesChanged);

    // No media was loaded, so the handler finds no "path" property and must
    // ignore the event instead of recording an empty entry.
    QVERIFY(QMetaObject::invokeMethod(&engine, "fileLoaded"));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(store.entries().size(), 0);
}

void TestHistoryStore::timerTickWithoutMediaIsNoop()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    MpvEngine engine;
    QVERIFY(engine.initialize());

    HistoryStore store(&engine, dir.filePath(u"history.json"_s));
    store.recordPlayback(u"/x/a.mkv"_s, u"A"_s, 0.0, 10.0);

    QSignalSpy spy(&store, &HistoryStore::entriesChanged);
    // Engine is idle (no media): the 5s tick must not touch the entries.
    QVERIFY(QMetaObject::invokeMethod(&store, "onTimerTick"));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(store.entries().size(), 1);
    QCOMPARE(store.entries().at(0).toMap().value(u"position"_s).toDouble(), 0.0);
}

QTEST_GUILESS_MAIN(TestHistoryStore)
#include "test_history_store.moc"
