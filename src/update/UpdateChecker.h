#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

// P9 update check: queries the GitHub releases API for the latest release of
// emoeem/mpv-emo and reports it when the published tag is newer than the
// running build (EMOPLAY_VERSION, passed in by main.cpp). Scope is
// deliberately minimal:
//   - One GET per check() call; main.cpp schedules exactly one per launch.
//   - The result is only ever *reported*. This class never downloads or
//     installs anything; opening the release page is user-initiated QML
//     (qml/UpdateBadge.qml -> Qt.openUrlExternally).
//   - Every failure mode (offline, HTTP error, malformed JSON, missing
//     fields) is silent: nothing but checkFinished() is emitted, so a
//     network-less launch looks exactly like an up-to-date one.
class UpdateChecker : public QObject
{
    Q_OBJECT
    // The three properties below flip together when a newer release has been
    // confirmed; QML binds the control-bar badge to them. They stay at their
    // defaults (no update) until then — including on every failure path.
    Q_PROPERTY(bool hasUpdate READ hasUpdate NOTIFY hasUpdateChanged FINAL)
    Q_PROPERTY(QString latestVersion READ latestVersion
                   NOTIFY latestVersionChanged FINAL)
    Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY releaseUrlChanged FINAL)

public:
    explicit UpdateChecker(const QString &currentVersion,
                           QObject *parent = nullptr);

    bool hasUpdate() const { return m_hasUpdate; }
    QString latestVersion() const { return m_latestVersion; }
    QString releaseUrl() const { return m_releaseUrl; }

    // --- pure logic, unit-tested headlessly without any network ----------
    // "v0.2.0-beta" -> "0.2.0-beta": trims whitespace and strips one leading
    // 'v'/'V' when a digit follows (so a tag like "verbose-1" survives).
    // Unknown shapes pass through trimmed; empty stays empty.
    static QString tagToVersion(const QString &tag);
    // Dotted-numeric comparison with pre-release handling. Documented
    // simplification (details in UpdateChecker.cpp): numeric components
    // compare left to right (missing = 0, so "1.0" == "1.0.0"), a release
    // outranks its own pre-releases, and pre-release identifiers rank
    // alpha < beta < rc (rc.2 > rc.1) < anything unrecognized.
    static int compareVersions(const QString &a, const QString &b);
    // True when candidate is strictly newer than the running version.
    static bool isNewer(const QString &current, const QString &candidate);

    // Assembled URL of the GitHub "latest release" endpoint. The base is
    // injectable so tests can point check() at a local loopback server and
    // never touch the real API.
    QUrl releasesUrl() const;
    void setApiBase(const QString &apiBase);

public slots:
    // Starts one GET of releasesUrl(). Ignored while a check is in flight.
    // A newer tag emits updateAvailable() and flips the properties above;
    // every failure emits nothing but checkFinished().
    void check();

signals:
    // Carries the parsed version (tag without any leading 'v') and the
    // release page URL from the API response ("html_url", falling back to
    // the deterministic /releases/tag/<tag> page when absent).
    void updateAvailable(const QString &version, const QUrl &releaseUrl);
    // Emitted once per completed check(), on success and failure alike;
    // lets headless tests await the silent paths instead of sleeping.
    void checkFinished();
    void hasUpdateChanged();
    void latestVersionChanged();
    void releaseUrlChanged();

private:
    void finishCheck();
    void setFound(const QString &version, const QUrl &url);

    QString m_currentVersion;
    QString m_apiBase;
    bool m_hasUpdate = false;
    QString m_latestVersion;
    QString m_releaseUrl;
    QNetworkAccessManager *m_nam = nullptr;
    QPointer<QNetworkReply> m_reply;
};
