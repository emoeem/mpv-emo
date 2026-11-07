#include "plugins/PluginRemoteFetcher.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace {

// Bounded wait per source: the graceful-degradation contract requires the
// refresh to fail (and the cached list to show) even when the network
// silently blackholes packets.
constexpr int kTransferTimeoutMs = 15000;

} // namespace

void QNetworkPluginRemoteFetcher::fetch(const QUrl &url)
{
    if (!m_manager)
        m_manager = new QNetworkAccessManager(this);
    // Supersede any in-flight fetch: dropped from the pending map first so
    // the abort's own finished() is ignored (exactly one finished() per
    // outstanding fetch — the newest one).
    const QList<QNetworkReply *> stale = m_pending.keys();
    for (QNetworkReply *reply : stale) {
        m_pending.remove(reply);
        reply->abort();
        reply->deleteLater();
    }

    QNetworkRequest request(url);
    request.setTransferTimeout(kTransferTimeoutMs);
    // Qt 6's default redirect policy is already NoLessSafeRedirects; set it
    // explicitly so a manifest moving to https keeps fetching.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader(QByteArrayLiteral("Accept"),
                         QByteArrayLiteral("application/json"));

    QNetworkReply *reply = m_manager->get(request);
    m_pending.insert(reply, url);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply] { onReplyFinished(reply); });
}

void QNetworkPluginRemoteFetcher::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    const QUrl url = m_pending.take(reply);
    if (!url.isValid())
        return; // superseded fetch: aborted, reported as nothing

    PluginRemoteFetchResult result;
    result.httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        result.error = reply->errorString();
    } else if (result.httpStatus < 200 || result.httpStatus > 299) {
        result.error = QStringLiteral("HTTP status %1").arg(result.httpStatus);
    } else {
        result.success = true;
    }
    emit finished(url, result);
}
