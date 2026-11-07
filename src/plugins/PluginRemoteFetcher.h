#pragma once

#include <QByteArray>
#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

// Result of one manifest-source fetch (one HTTP GET).
struct PluginRemoteFetchResult
{
    // HTTP status code; 0 when the request never got a reply (DNS failure,
    // timeout, offline machine...).
    int httpStatus = 0;
    QByteArray body;
    // Human-readable failure reason; empty on success.
    QString error;
    // Started AND answered with a 2xx status; only then is |body| meaningful.
    bool success = false;
};

Q_DECLARE_METATYPE(PluginRemoteFetchResult)

// Seam between RemoteManifestSource and the network, mirroring the
// PluginGitRunner seam: unit tests inject a fake that answers synchronously
// with fixture bytes and never touch the network; production runs
// QNetworkAccessManager asynchronously so a slow source never blocks the
// GUI thread.
//
// One instance carries one fetch at a time (RemoteManifestSource queues its
// sources); a fetch superseded by another fetch() call is aborted and emits
// nothing.
class PluginRemoteFetcher : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~PluginRemoteFetcher() override = default;

    // Starts a GET for |url|. Exactly one finished() signal is emitted per
    // call unless the fetch is superseded (see class doc). Synchronous
    // emission is allowed (fakes); RemoteManifestSource tolerates it.
    virtual void fetch(const QUrl &url) = 0;

signals:
    void finished(const QUrl &url, const PluginRemoteFetchResult &result);
};

// Production implementation: one QNetworkAccessManager GET per fetch, with a
// bounded transfer timeout so an offline machine degrades to the cached
// manifest list after a fixed wait instead of hanging the refresh forever.
class QNetworkPluginRemoteFetcher : public PluginRemoteFetcher
{
    Q_OBJECT

public:
    using PluginRemoteFetcher::PluginRemoteFetcher;

    void fetch(const QUrl &url) override;

private:
    void onReplyFinished(QNetworkReply *reply);

    // Created lazily on the first fetch (constructing a QNetworkAccessManager
    // is cheap but pointless for test instances that never fetch).
    QNetworkAccessManager *m_manager = nullptr;
    // Replies this runner still owes a finished() for. A reply removed here
    // (superseded) is aborted and ignored when its finished() arrives.
    QHash<QNetworkReply *, QUrl> m_pending;
};
