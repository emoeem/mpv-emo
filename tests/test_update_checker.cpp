#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkProxy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

#include "update/UpdateChecker.h"

using namespace Qt::Literals::StringLiterals;

// Minimal loopback HTTP server: answers every connection with one canned
// response and records the raw request bytes. All UpdateChecker network
// tests run against 127.0.0.1 — no request ever leaves the machine.
class LoopbackServer : public QObject
{
    Q_OBJECT

public:
    explicit LoopbackServer(QObject *parent = nullptr)
        : QObject(parent)
    {
        connect(&m_server, &QTcpServer::newConnection, this,
                &LoopbackServer::onNewConnection);
    }

    bool start() { return m_server.listen(QHostAddress::LocalHost); }
    quint16 port() const { return m_server.serverPort(); }

    void setResponse(int status, const QByteArray &reason,
                     const QByteArray &body)
    {
        m_status = status;
        m_reason = reason;
        m_body = body;
    }
    // Hold connections open instead of answering; releaseHeld() answers them
    // (used to verify a second check() while one is in flight is a no-op).
    void setHoldOpen(bool hold) { m_holdOpen = hold; }
    void releaseHeld()
    {
        const QList<QTcpSocket *> held = m_held;
        m_held.clear();
        for (QTcpSocket *socket : held)
            respond(socket);
    }

    int requestCount() const { return m_requests.size(); }
    QByteArray lastRequestLine() const { return m_requestLine; }
    QByteArray userAgentHeader() const { return m_userAgent; }

private slots:
    void onNewConnection()
    {
        QTcpSocket *socket = m_server.nextPendingConnection();
        if (!socket)
            return;
        connect(socket, &QTcpSocket::disconnected, socket,
                &QTcpSocket::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            m_buffers[socket] += socket->readAll();
            if (!m_buffers[socket].contains("\r\n\r\n"))
                return;
            recordRequest(m_buffers.take(socket));
            if (m_holdOpen) {
                m_held.append(socket);
                return;
            }
            respond(socket);
        });
    }

private:
    void recordRequest(const QByteArray &raw)
    {
        m_requests.append(raw);
        const QList<QByteArray> parts = raw.split('\n');
        if (!parts.isEmpty())
            m_requestLine = parts.first().trimmed();
        for (const QByteArray &line : parts.mid(1)) {
            if (line.startsWith("User-Agent:"))
                m_userAgent = line.mid(11).trimmed();
        }
    }

    void respond(QTcpSocket *socket)
    {
        QByteArray head = "HTTP/1.1 " + QByteArray::number(m_status) + ' '
                          + m_reason + "\r\n"
                          "Content-Type: application/json\r\n"
                          "Connection: close\r\n"
                          "Content-Length: "
                          + QByteArray::number(m_body.size()) + "\r\n\r\n";
        socket->write(head + m_body);
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QList<QTcpSocket *> m_held;
    QList<QByteArray> m_requests;
    QByteArray m_requestLine;
    QByteArray m_userAgent;
    bool m_holdOpen = false;
    int m_status = 200;
    QByteArray m_reason = "OK";
    QByteArray m_body;
};

class TestUpdateChecker : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Pure logic: tag parsing, version comparison, URL assembly.
    void tagParsing();
    void versionComparison();
    void isNewerCases();
    void releasesUrlAssembly();

    // Network path over a 127.0.0.1 loopback server (no real requests).
    void loopbackFindsNewerRelease();
    void loopbackMissingHtmlUrlFallsBackToReleasePage();
    void loopbackSameVersionIsSilent();
    void loopbackHttpErrorIsSilent();
    void loopbackMalformedJsonIsSilent();
    void secondCheckWhileInFlightIsIgnored();

private:
    static QByteArray releaseJson(const QByteArray &tag,
                                  const QByteArray &htmlUrl);
};

void TestUpdateChecker::initTestCase()
{
    // Loopback traffic must never be routed through a proxy picked up from
    // the environment (none is expected on CI, but this is cheap insurance).
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
}

QByteArray TestUpdateChecker::releaseJson(const QByteArray &tag,
                                          const QByteArray &htmlUrl)
{
    QJsonObject object;
    object.insert("tag_name", QString::fromUtf8(tag));
    object.insert("html_url", QString::fromUtf8(htmlUrl));
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

void TestUpdateChecker::tagParsing()
{
    QCOMPARE(UpdateChecker::tagToVersion(u"v0.2.0-beta"_s), u"0.2.0-beta"_s);
    QCOMPARE(UpdateChecker::tagToVersion(u"V1.2.3"_s), u"1.2.3"_s);
    QCOMPARE(UpdateChecker::tagToVersion(u"0.1.0"_s), u"0.1.0"_s);
    QCOMPARE(UpdateChecker::tagToVersion(u"  v0.3.0 "_s), u"0.3.0"_s);
    // 'v' not followed by a digit is not a version prefix.
    QCOMPARE(UpdateChecker::tagToVersion(u"verbose-1"_s), u"verbose-1"_s);
    // Degenerate inputs pass through.
    QCOMPARE(UpdateChecker::tagToVersion(u""_s), u""_s);
    QCOMPARE(UpdateChecker::tagToVersion(u"v"_s), u"v"_s);
}

void TestUpdateChecker::versionComparison()
{
    using AC = UpdateChecker;
    // Equal numbers with different component counts.
    QCOMPARE(AC::compareVersions(u"1.0"_s, u"1.0.0"_s), 0);
    QCOMPARE(AC::compareVersions(u"0.1.0-alpha"_s, u"0.1.0-alpha"_s), 0);
    // Numeric ordering, left to right; not lexicographic.
    QVERIFY(AC::compareVersions(u"0.3.0"_s, u"0.2.9"_s) > 0);
    QVERIFY(AC::compareVersions(u"0.2.0"_s, u"1.0.0"_s) < 0);
    QVERIFY(AC::compareVersions(u"10.0.0"_s, u"9.0.0"_s) > 0);
    // A release outranks its own pre-releases.
    QVERIFY(AC::compareVersions(u"1.0.0"_s, u"1.0.0-beta"_s) > 0);
    QVERIFY(AC::compareVersions(u"1.0.0-rc.1"_s, u"1.0.0"_s) < 0);
    // Pre-release ranks: alpha < beta < rc, numeric tails compare as numbers.
    QVERIFY(AC::compareVersions(u"1.0.0-alpha"_s, u"1.0.0-beta"_s) < 0);
    QVERIFY(AC::compareVersions(u"1.0.0-beta"_s, u"1.0.0-rc.1"_s) < 0);
    QVERIFY(AC::compareVersions(u"1.0.0-rc.1"_s, u"1.0.0-rc.2"_s) < 0);
    QVERIFY(AC::compareVersions(u"1.0.0-alpha.2"_s, u"1.0.0-alpha.10"_s) < 0);
    // Unrecognized identifiers order deterministically and tie on equality.
    QCOMPARE(AC::compareVersions(u"1.0.0-dev"_s, u"1.0.0-dev"_s), 0);
    QVERIFY(AC::compareVersions(u"1.0.0-dev.1"_s, u"1.0.0-dev.2"_s) < 0);
    // 'v' prefixes are normalized inside the comparison.
    QCOMPARE(AC::compareVersions(u"v0.2.0-beta"_s, u"0.2.0-beta"_s), 0);
    QVERIFY(AC::compareVersions(u"v0.3.0-beta"_s, u"0.2.0-beta"_s) > 0);
}

void TestUpdateChecker::isNewerCases()
{
    // A newer prerelease beats an older one.
    QVERIFY(UpdateChecker::isNewer(u"0.2.0-beta"_s, u"0.3.0-alpha"_s));
    // A release beats its own pre-release; never the other way around.
    QVERIFY(UpdateChecker::isNewer(u"0.2.0-beta"_s, u"v0.2.0"_s));
    QVERIFY(!UpdateChecker::isNewer(u"0.2.0"_s, u"0.2.0-beta"_s));
    // Same version is never newer (identity includes the 'v' normalization).
    QVERIFY(!UpdateChecker::isNewer(u"0.2.0-beta"_s, u"0.2.0-beta"_s));
    QVERIFY(!UpdateChecker::isNewer(u"0.1.0"_s, u"v0.1.0"_s));
    // Rank progress within one release candidate series.
    QVERIFY(UpdateChecker::isNewer(u"0.2.0-beta"_s, u"0.2.0-rc.1"_s));
    QVERIFY(UpdateChecker::isNewer(u"1.0.0-rc.2"_s, u"1.0.0-rc.10"_s));
    QVERIFY(!UpdateChecker::isNewer(u"1.0.0-rc.10"_s, u"1.0.0-rc.2"_s));
}

void TestUpdateChecker::releasesUrlAssembly()
{
    UpdateChecker checker(u"0.1.0"_s, this);
    QCOMPARE(checker.releasesUrl(),
             QUrl(u"https://api.github.com/repos/emoeem/mpv-emo/releases/latest"_s));
    // Injectable base (used by the loopback cases); a trailing slash is
    // trimmed so the path never doubles.
    checker.setApiBase(u"http://127.0.0.1:8080/"_s);
    QCOMPARE(checker.releasesUrl(),
             QUrl(u"http://127.0.0.1:8080/repos/emoeem/mpv-emo/releases/latest"_s));
}

void TestUpdateChecker::loopbackFindsNewerRelease()
{
    LoopbackServer server(this);
    QVERIFY(server.start());
    server.setResponse(200, "OK", releaseJson("v99.0.0",
            "https://github.com/emoeem/mpv-emo/releases/tag/v99.0.0"));

    UpdateChecker checker(u"0.2.0-beta"_s, this);
    checker.setApiBase(u"http://127.0.0.1:%1"_s.arg(server.port()));
    QSignalSpy found(&checker, &UpdateChecker::updateAvailable);
    QSignalSpy done(&checker, &UpdateChecker::checkFinished);
    QVERIFY(found.isValid() && done.isValid());

    checker.check();
    QVERIFY(done.wait(10000));

    QCOMPARE(found.count(), 1);
    QCOMPARE(found.at(0).at(0).toString(), u"99.0.0"_s);
    QCOMPARE(found.at(0).at(1).toUrl(),
             QUrl(u"https://github.com/emoeem/mpv-emo/releases/tag/v99.0.0"_s));
    QVERIFY(checker.hasUpdate());
    QCOMPARE(checker.latestVersion(), u"99.0.0"_s);
    QCOMPARE(checker.releaseUrl(),
             u"https://github.com/emoeem/mpv-emo/releases/tag/v99.0.0"_s);

    // The request hit the loopback endpoint with the required User-Agent.
    QCOMPARE(server.requestCount(), 1);
    QVERIFY(server.lastRequestLine()
                .startsWith("GET /repos/emoeem/mpv-emo/releases/latest"));
    QVERIFY(server.userAgentHeader().startsWith("EmoPlay/0.2.0-beta"));
}

void TestUpdateChecker::loopbackMissingHtmlUrlFallsBackToReleasePage()
{
    LoopbackServer server(this);
    QVERIFY(server.start());
    server.setResponse(200, "OK", releaseJson("v31.0.0", ""));

    UpdateChecker checker(u"0.2.0-beta"_s, this);
    checker.setApiBase(u"http://127.0.0.1:%1"_s.arg(server.port()));
    QSignalSpy found(&checker, &UpdateChecker::updateAvailable);
    QSignalSpy done(&checker, &UpdateChecker::checkFinished);

    checker.check();
    QVERIFY(done.wait(10000));

    QCOMPARE(found.count(), 1);
    QCOMPARE(found.at(0).at(1).toUrl(),
             QUrl(u"https://github.com/emoeem/mpv-emo/releases/tag/v31.0.0"_s));
}

void TestUpdateChecker::loopbackSameVersionIsSilent()
{
    LoopbackServer server(this);
    QVERIFY(server.start());
    server.setResponse(200, "OK", releaseJson("v0.2.0-beta",
            "https://github.com/emoeem/mpv-emo/releases/tag/v0.2.0-beta"));

    UpdateChecker checker(u"0.2.0-beta"_s, this);
    checker.setApiBase(u"http://127.0.0.1:%1"_s.arg(server.port()));
    QSignalSpy found(&checker, &UpdateChecker::updateAvailable);
    QSignalSpy done(&checker, &UpdateChecker::checkFinished);

    checker.check();
    QVERIFY(done.wait(10000));

    QCOMPARE(server.requestCount(), 1);
    QCOMPARE(found.count(), 0);
    QVERIFY(!checker.hasUpdate());
    QCOMPARE(checker.latestVersion(), u""_s);
}

void TestUpdateChecker::loopbackHttpErrorIsSilent()
{
    LoopbackServer server(this);
    QVERIFY(server.start());
    server.setResponse(404, "Not Found", "{}");

    UpdateChecker checker(u"0.2.0-beta"_s, this);
    checker.setApiBase(u"http://127.0.0.1:%1"_s.arg(server.port()));
    QSignalSpy found(&checker, &UpdateChecker::updateAvailable);
    QSignalSpy done(&checker, &UpdateChecker::checkFinished);

    checker.check();
    QVERIFY(done.wait(10000));

    QCOMPARE(server.requestCount(), 1);
    QCOMPARE(found.count(), 0);
    QVERIFY(!checker.hasUpdate());
}

void TestUpdateChecker::loopbackMalformedJsonIsSilent()
{
    LoopbackServer server(this);
    QVERIFY(server.start());
    server.setResponse(200, "OK", QByteArray("not-json"));

    UpdateChecker checker(u"0.2.0-beta"_s, this);
    checker.setApiBase(u"http://127.0.0.1:%1"_s.arg(server.port()));
    QSignalSpy found(&checker, &UpdateChecker::updateAvailable);
    QSignalSpy done(&checker, &UpdateChecker::checkFinished);

    checker.check();
    QVERIFY(done.wait(10000));

    QCOMPARE(found.count(), 0);
    QVERIFY(!checker.hasUpdate());
}

void TestUpdateChecker::secondCheckWhileInFlightIsIgnored()
{
    LoopbackServer server(this);
    QVERIFY(server.start());
    server.setResponse(200, "OK", releaseJson("v99.0.0",
            "https://github.com/emoeem/mpv-emo/releases/tag/v99.0.0"));
    server.setHoldOpen(true);

    UpdateChecker checker(u"0.2.0-beta"_s, this);
    checker.setApiBase(u"http://127.0.0.1:%1"_s.arg(server.port()));
    QSignalSpy found(&checker, &UpdateChecker::updateAvailable);
    QSignalSpy done(&checker, &UpdateChecker::checkFinished);

    checker.check();
    QTRY_COMPARE(server.requestCount(), 1);
    checker.check(); // a check is in flight: this must send nothing
    server.releaseHeld();
    QVERIFY(done.wait(10000));

    QCOMPARE(server.requestCount(), 1);
    QCOMPARE(found.count(), 1);
    QCOMPARE(checker.latestVersion(), u"99.0.0"_s);
}

// GUI-less: this suite needs only QtCore + QtNetwork (loopback sockets),
// so it must not demand a platform plugin on display-less CI runners.
QTEST_GUILESS_MAIN(TestUpdateChecker)
#include "test_update_checker.moc"
