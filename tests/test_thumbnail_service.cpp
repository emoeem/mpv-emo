#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "library/ThumbnailQueue.h"
#include "library/ThumbnailService.h"

using namespace Qt::Literals::StringLiterals;

// Headless unit tests for the P6 v2 thumbnail service. No real ffmpeg is
// ever executed:
//  - pure logic (cache-key hashing, seek computation, duration parsing,
//    queue scheduling) is tested directly;
//  - the probe/generation process chain runs against a stub *shell script*
//    that emulates the three ffmpeg invocation shapes the service uses
//    (`-version`, `-i` duration probe, `-frames:v` frame grab). The stub is
//    written per test into a QTemporaryDir, so each test controls its own
//    behavior (banner validation, generation success/failure);
//  - the "ffmpeg missing" degradation path uses a binary path that cannot
//    exist.
// The stub needs a POSIX shell to exec the script; CI runs on Linux.
namespace {

bool shellAvailable()
{
#ifdef Q_OS_UNIX
    return true;
#else
    return false;
#endif
}

// Non-empty placeholder payload for cache files. The service contract only
// requires the file to exist and be non-empty; nothing in these tests
// decodes image data, so the bytes need not be a decodable JPEG.
QByteArray stubImageBytes()
{
    return QByteArray("stub-thumbnail-payload\n");
}

// Writes the stub image file and the ffmpeg stub script into |dir| and
// returns the script path. The stub appends every argument vector it
// receives to <dir>/invocations.log so tests can assert the exact command
// lines (one line per invocation, arguments joined with spaces).
QString writeStub(const QTemporaryDir &dir, bool failGenerate = false,
                  bool badVersion = false)
{
    const QString jpegPath = dir.filePath(u"stub-image.bin"_s);
    QFile jpeg(jpegPath);
    QVERIFY(jpeg.open(QIODevice::WriteOnly));
    QVERIFY(jpeg.write(stubImageBytes()) > 0);
    jpeg.close();

    const QString logPath = dir.filePath(u"invocations.log"_s);
    const QString script = dir.filePath(u"ffmpeg-stub.sh"_s);
    QFile file(script);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(QStringLiteral(
                   "#!/bin/sh\n"
                   "# ffmpeg stub for ThumbnailService tests — no real ffmpeg.\n"
                   "LOG='%1'\n"
                   "JPEG='%2'\n"
                   "FAIL_GENERATE='%3'\n"
                   "BAD_VERSION='%4'\n"
                   "\n"
                   "printf '%s\\n' \"$*\" >> \"$LOG\"\n"
                   "\n"
                   "case \" $* \" in\n"
                   "  *\" -version \"*)\n"
                   "    if [ \"$BAD_VERSION\" = \"1\" ]; then\n"
                   "      echo \"definitely not an ffmpeg\"\n"
                   "    else\n"
                   "      echo \"ffmpeg version 6.1.1-stub Copyright (c) stub\"\n"
                   "    fi\n"
                   "    exit 0\n"
                   "    ;;\n"
                   "esac\n"
                   "\n"
                   "case \" $* \" in\n"
                   "  *\" -frames:v \"*)\n"
                   "    if [ \"$FAIL_GENERATE\" = \"1\" ]; then\n"
                   "      echo \"stub: generation failed\" >&2\n"
                   "      exit 1\n"
                   "    fi\n"
                   "    out=\"\"\n"
                   "    for a in \"$@\"; do out=\"$a\"; done\n"
                   "    cat \"$JPEG\" > \"$out\"\n"
                   "    exit 0\n"
                   "    ;;\n"
                   "esac\n"
                   "\n"
                   "# Duration probe: real ffmpeg exits 1 without an output file.\n"
                   "echo \"Input #0, matroska, from '$2':\" >&2\n"
                   "echo \"  Duration: 00:02:00.00, start: 0.000000,"
                   " bitrate: 1000 kb/s\" >&2\n"
                   "echo \"At least one output file must be specified\" >&2\n"
                   "exit 1\n")
                   .arg(logPath, jpegPath, failGenerate ? u"1"_s : u"0"_s,
                        badVersion ? u"1"_s : u"0"_s)
                   .toUtf8());
    file.close();
    QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::ExeOwner
                                | QFile::ReadGroup | QFile::ExeGroup
                                | QFile::ReadOther | QFile::ExeOther));
    return script;
}

// Creates a fake media file (existence is all the service stats).
QString writeMediaFile(const QTemporaryDir &dir)
{
    const QString path = dir.filePath(u"movie.mkv"_s);
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not really a video\n");
    file.close();
    return path;
}

QStringList readInvocations(const QString &logPath)
{
    QFile log(logPath);
    if (!log.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QStringList lines =
        QString::fromUtf8(log.readAll()).split(QLatin1Char('\n'));
    QStringList nonEmpty;
    for (const QString &line : lines) {
        if (!line.trimmed().isEmpty())
            nonEmpty.append(line);
    }
    return nonEmpty;
}

} // namespace

class TestThumbnailService : public QObject
{
    Q_OBJECT

private slots:
    // ---- pure logic: cache-key hashing ---------------------------------
    void cachePathForIsSha1OfPathAndMtime();
    void cachePathForChangesWithMtime();

    // ---- pure logic: seek + duration parsing ---------------------------
    void seekPositionComputesTenPercent();
    void durationParserHandlesFfmpegOutput();

    // ---- pure logic: queue scheduling ----------------------------------
    void queueDedupesAndKeepsFifoOrder();

    // ---- probe + generation through the stub chain ---------------------
    void probeAcceptsVersionBanner();
    void probeRejectsNonFfmpegBanner();
    void probeMissingBinaryReportsUnavailable();
    void requestGeneratesThumbnailThroughStubChain();
    void requestFailureIsNotRetried();

    // ---- ffmpeg missing: graceful degradation --------------------------
    void requestWithMissingBinaryDegradesGracefully();
    void cacheHitIsServedWithoutFfmpeg();
};

void TestThumbnailService::cachePathForIsSha1OfPathAndMtime()
{
    const QString path = u"/media/movie.mkv"_s;
    const qint64 mtimeMs = 1700000000000LL;

    const QString cachePath =
        ThumbnailService::cachePathFor(u"cache"_s, path, mtimeMs);

    // Spec: <cacheDir>/<sha1(path + mtime)>.jpg — recomputed here from the
    // documented hash input.
    const QString expected =
        u"cache"_s + QLatin1Char('/')
        + QString::fromLatin1(
            QCryptographicHash::hash(path.toUtf8()
                                         + QByteArray::number(mtimeMs),
                                     QCryptographicHash::Sha1)
                .toHex())
        + u".jpg"_s;
    QCOMPARE(cachePath, expected);

    const QString name = QFileInfo(cachePath).fileName();
    QVERIFY(QRegularExpression(u"^[0-9a-f]{40}\\.jpg$"_s).match(name)
                .hasMatch());
}

void TestThumbnailService::cachePathForChangesWithMtime()
{
    const QString path = u"/media/movie.mkv"_s;
    const QString forMtime =
        ThumbnailService::cachePathFor(u"cache"_s, path, 1700000000000LL);
    // Same inputs -> same name (stable), different mtime/path -> new name
    // (editing a file invalidates its thumbnail).
    QCOMPARE(ThumbnailService::cachePathFor(u"cache"_s, path, 1700000000000LL),
             forMtime);
    QVERIFY(ThumbnailService::cachePathFor(u"cache"_s, path,
                                           1700000000001LL)
            != forMtime);
    QVERIFY(ThumbnailService::cachePathFor(u"cache"_s, path + u"x"_s,
                                           1700000000000LL)
            != forMtime);
}

void TestThumbnailService::seekPositionComputesTenPercent()
{
    // 10% of the duration, formatted for ffmpeg's -ss.
    QCOMPARE(ThumbnailService::seekPositionFor(120.0), u"12.000"_s);
    QCOMPARE(ThumbnailService::seekPositionFor(5.0), u"0.500"_s);
    // Unknown/invalid durations seek to 0.
    QCOMPARE(ThumbnailService::seekPositionFor(0.0), u"0.000"_s);
    QCOMPARE(ThumbnailService::seekPositionFor(-10.0), u"0.000"_s);
    QCOMPARE(ThumbnailService::seekPositionFor(qQNaN()), u"0.000"_s);
}

void TestThumbnailService::durationParserHandlesFfmpegOutput()
{
    // The exact stderr shape ffmpeg prints for `-i file` with no output
    // (plus the trailing error and exit 1, which the parser ignores).
    const QString stderrText =
        u"Input #0, matroska, from 'movie.mkv':\n"
        "  Metadata:\n"
        "  Duration: 00:02:00.00, start: 0.000000, bitrate: 1000 kb/s\n"
        "At least one output file must be specified\n"_s;
    QCOMPARE(ThumbnailService::durationFromFfmpegStderr(stderrText), 120.0);
    QCOMPARE(ThumbnailService::durationFromFfmpegStderr(
                 u"  Duration: 01:00:00.500, start: 0.0"_s),
             3600.5);
    // "Duration: N/A" (live sources) and garbage -> unknown duration.
    QCOMPARE(ThumbnailService::durationFromFfmpegStderr(
                 u"  Duration: N/A, start: 0.000000"_s),
             -1.0);
    QCOMPARE(ThumbnailService::durationFromFfmpegStderr(u""_s), -1.0);
}

void TestThumbnailService::queueDedupesAndKeepsFifoOrder()
{
    ThumbnailQueue queue;
    QVERIFY(queue.isEmpty());
    QCOMPARE(queue.size(), 0);

    QVERIFY(queue.enqueue(u"a"_s));
    QVERIFY(queue.enqueue(u"b"_s));
    QVERIFY(!queue.enqueue(u"a"_s));      // duplicate request: dropped
    QVERIFY(!queue.enqueue(QString()));   // empty path: rejected
    QCOMPARE(queue.size(), 2);
    QVERIFY(queue.contains(u"a"_s));
    QVERIFY(queue.contains(u"b"_s));

    QCOMPARE(queue.takeNext(), u"a"_s);   // FIFO
    QCOMPARE(queue.takeNext(), u"b"_s);
    QVERIFY(queue.takeNext().isEmpty());  // drained
    QVERIFY(queue.isEmpty());

    QVERIFY(queue.enqueue(u"c"_s));
    queue.clear();
    QVERIFY(queue.isEmpty());
    QVERIFY(!queue.contains(u"c"_s));
}

void TestThumbnailService::probeAcceptsVersionBanner()
{
    if (!shellAvailable())
        QSKIP("the ffmpeg stub script needs a POSIX shell");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ThumbnailService service(dir.filePath(u"thumbs"_s), writeStub(dir));

    QSignalSpy spy(&service, &ThumbnailService::availabilityChanged);
    service.probe();
    QTRY_VERIFY(spy.size() >= 1);
    QVERIFY(service.isAvailable());
}

void TestThumbnailService::probeRejectsNonFfmpegBanner()
{
    if (!shellAvailable())
        QSKIP("the ffmpeg stub script needs a POSIX shell");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ThumbnailService service(dir.filePath(u"thumbs"_s),
                             writeStub(dir, false, /*badVersion=*/true));

    QSignalSpy spy(&service, &ThumbnailService::availabilityChanged);
    service.probe();
    QTRY_VERIFY(spy.size() >= 1);
    QVERIFY(!service.isAvailable());
}

void TestThumbnailService::probeMissingBinaryReportsUnavailable()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ThumbnailService service(
        dir.filePath(u"thumbs"_s),
        u"/nonexistent-dir-for-tests/ffmpeg-should-not-exist"_s);

    QSignalSpy spy(&service, &ThumbnailService::availabilityChanged);
    service.probe();
    QTRY_VERIFY(spy.size() >= 1);
    QVERIFY(!service.isAvailable());
}

void TestThumbnailService::requestGeneratesThumbnailThroughStubChain()
{
    if (!shellAvailable())
        QSKIP("the ffmpeg stub script needs a POSIX shell");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString mediaPath = writeMediaFile(dir);
    const qint64 mtimeMs =
        QFileInfo(mediaPath).lastModified().toMSecsSinceEpoch();
    const QString expectedCache =
        ThumbnailService::cachePathFor(dir.filePath(u"thumbs"_s), mediaPath,
                                       mtimeMs);

    ThumbnailService service(dir.filePath(u"thumbs"_s), writeStub(dir));
    service.probe();
    QTRY_VERIFY(service.isAvailable());

    QSignalSpy readySpy(&service, &ThumbnailService::thumbnailReady);
    service.requestThumbnail(mediaPath);
    QTRY_COMPARE(readySpy.size(), 1);
    QCOMPARE(readySpy.at(0).at(0).toString(), mediaPath);
    QCOMPARE(readySpy.at(0).at(1).toString(), expectedCache);
    QCOMPARE(service.cachedThumbnailPath(mediaPath), expectedCache);
    QVERIFY(QFileInfo(expectedCache).exists());
    QVERIFY(QFileInfo(expectedCache).size() > 0);

    // The serial chain ran exactly two children: the duration probe
    // (`-i file`, expected exit 1) and the frame grab at 10% of the stub's
    // 120 s duration with the spec'd filter; the output path is the last
    // argument.
    const QStringList invocations =
        readInvocations(dir.filePath(u"invocations.log"_s));
    QCOMPARE(invocations.size(), 3);
    QCOMPARE(invocations.at(0), u"-version"_s);
    QCOMPARE(invocations.at(1), u"-i " + mediaPath);
    QVERIFY(invocations.at(2).contains(u"-ss 12.000"_s));
    QVERIFY(invocations.at(2).contains(u"-i " + mediaPath));
    QVERIFY(invocations.at(2).contains(u"-frames:v 1"_s));
    QVERIFY(invocations.at(2).contains(u"-vf scale=320:-1"_s));
    QVERIFY(invocations.at(2).endsWith(expectedCache));

    // Idempotent: an already-known-ready path is never regenerated.
    service.requestThumbnail(mediaPath);
    QCOMPARE(readySpy.size(), 1);
    QCOMPARE(readInvocations(dir.filePath(u"invocations.log"_s)).size(), 3);
}

void TestThumbnailService::requestFailureIsNotRetried()
{
    if (!shellAvailable())
        QSKIP("the ffmpeg stub script needs a POSIX shell");

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString mediaPath = writeMediaFile(dir);
    const QString logPath = dir.filePath(u"invocations.log"_s);

    ThumbnailService service(dir.filePath(u"thumbs"_s),
                             writeStub(dir, /*failGenerate=*/true));
    service.probe();
    QTRY_VERIFY(service.isAvailable());

    QSignalSpy readySpy(&service, &ThumbnailService::thumbnailReady);
    service.requestThumbnail(mediaPath);
    // Duration probe + failed frame grab, then the queue stops.
    QTRY_COMPARE(readInvocations(logPath).size(), 2);
    QCOMPARE(readySpy.size(), 0);
    QCOMPARE(service.cachedThumbnailPath(mediaPath), QString());

    // No retry loop: still exactly two invocations later, and a repeated
    // request does not requeue the failed path.
    QTest::qWait(200);
    QCOMPARE(readInvocations(logPath).size(), 2);
    service.requestThumbnail(mediaPath);
    QTest::qWait(200);
    QCOMPARE(readInvocations(logPath).size(), 2);
    QCOMPARE(readySpy.size(), 0);
}

void TestThumbnailService::requestWithMissingBinaryDegradesGracefully()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString mediaPath = writeMediaFile(dir);
    const QString cacheDir = dir.filePath(u"thumbs"_s);

    ThumbnailService service(
        cacheDir, u"/nonexistent-dir-for-tests/ffmpeg-should-not-exist"_s);
    service.probe();
    QTRY_VERIFY(!service.isAvailable());

    QSignalSpy readySpy(&service, &ThumbnailService::thumbnailReady);
    service.requestThumbnail(mediaPath);
    QTest::qWait(200);
    QCOMPARE(readySpy.size(), 0);
    QCOMPARE(service.cachedThumbnailPath(mediaPath), QString());
    // Nothing was ever generated; the UI keeps its type icons.
    QCOMPARE(QDir(cacheDir).entryList(QDir::Files).size(), 0);
}

void TestThumbnailService::cacheHitIsServedWithoutFfmpeg()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString mediaPath = writeMediaFile(dir);
    const QString cacheDir = dir.filePath(u"thumbs"_s);
    QDir().mkpath(cacheDir);
    const QString expectedCache = ThumbnailService::cachePathFor(
        cacheDir, mediaPath, QFileInfo(mediaPath).lastModified()
                                 .toMSecsSinceEpoch());
    QFile cached(expectedCache);
    QVERIFY(cached.open(QIODevice::WriteOnly));
    QVERIFY(cached.write(stubImageBytes()) > 0);
    cached.close();

    // No ffmpeg at all: previously generated cache files are still served
    // (only generation needs the binary).
    ThumbnailService service(
        cacheDir, u"/nonexistent-dir-for-tests/ffmpeg-should-not-exist"_s);
    service.probe();
    QTRY_VERIFY(!service.isAvailable());

    QSignalSpy readySpy(&service, &ThumbnailService::thumbnailReady);
    service.requestThumbnail(mediaPath);
    QTRY_COMPARE(readySpy.size(), 1);
    QCOMPARE(readySpy.at(0).at(0).toString(), mediaPath);
    QCOMPARE(readySpy.at(0).at(1).toString(), expectedCache);
    QCOMPARE(service.cachedThumbnailPath(mediaPath), expectedCache);
}

QTEST_GUILESS_MAIN(TestThumbnailService)
#include "test_thumbnail_service.moc"
