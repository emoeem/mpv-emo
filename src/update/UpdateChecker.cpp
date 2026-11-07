#include "update/UpdateChecker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

// Endpoint of the GitHub releases API (public repo, unauthenticated) and the
// release page base used as a fallback when the response carries no
// "html_url". GitHub's REST API rejects requests without a User-Agent
// header; check() always sets one.
static const char kApiBase[] = "https://api.github.com";
static const char kLatestReleasePath[] =
    "/repos/emoeem/mpv-emo/releases/latest";
static const char kReleasePageBase[] =
    "https://github.com/emoeem/mpv-emo/releases";

namespace {

struct VersionParts
{
    QList<int> numbers;
    QString suffix; // text after the first '-'; empty means a release
};

VersionParts splitVersion(const QString &version)
{
    VersionParts parts;
    const qsizetype dash = version.indexOf(u'-');
    const QString core = (dash < 0) ? version : version.left(dash);
    parts.suffix = (dash < 0) ? QString() : version.mid(dash + 1);
    const QStringList chunks = core.split(u'.', Qt::SkipEmptyParts);
    for (const QString &chunk : chunks) {
        bool ok = false;
        const int number = chunk.toInt(&ok);
        parts.numbers.append(ok ? number : 0);
    }
    return parts;
}

// Rank of a pre-release identifier: alpha < beta < rc ("pre" counts as rc).
// Unrecognized identifiers rank after rc. Ranks only order pre-releases
// among themselves — a release outranks every pre-release, which
// compareVersions() handles before this is consulted.
int preReleaseRank(const QString &identifier)
{
    if (identifier == QLatin1String("alpha"))
        return 0;
    if (identifier == QLatin1String("beta"))
        return 1;
    if (identifier == QLatin1String("rc") || identifier == QLatin1String("pre"))
        return 2;
    return 3;
}

// Compares two pre-release identifiers such as "beta", "rc.2" or "dev".
// The letter part (before the first '.') ranks via preReleaseRank(); equal
// known ranks then compare the numeric tail ("rc.2" > "rc.1", a missing
// tail counts as 0). Unrecognized identifiers with different letters fall
// back to lexicographic order — not a value judgement, only determinism.
int compareIdentifiers(const QString &a, const QString &b)
{
    const qsizetype dotA = a.indexOf(u'.');
    const qsizetype dotB = b.indexOf(u'.');
    const QString lettersA = (dotA < 0) ? a : a.left(dotA);
    const QString lettersB = (dotB < 0) ? b : b.left(dotB);
    const QString tailA = (dotA < 0) ? QString() : a.mid(dotA + 1);
    const QString tailB = (dotB < 0) ? QString() : b.mid(dotB + 1);

    const int rankA = preReleaseRank(lettersA);
    const int rankB = preReleaseRank(lettersB);
    if (rankA != rankB)
        return rankA < rankB ? -1 : 1;
    if (rankA == 3 && lettersA != lettersB)
        return (lettersA < lettersB) ? -1 : 1;

    bool okA = false;
    bool okB = false;
    const int numberA = tailA.toInt(&okA);
    const int numberB = tailB.toInt(&okB);
    if (numberA != numberB)
        return numberA < numberB ? -1 : 1;
    if (!okA && !okB && tailA != tailB)
        return (tailA < tailB) ? -1 : 1;
    return 0;
}

} // namespace

UpdateChecker::UpdateChecker(const QString &currentVersion, QObject *parent)
    : QObject(parent),
      m_currentVersion(currentVersion),
      m_apiBase(QLatin1String(kApiBase)),
      m_nam(new QNetworkAccessManager(this))
{
}

QString UpdateChecker::tagToVersion(const QString &tag)
{
    QString version = tag.trimmed();
    // Strip a git-convention "v"/"V" prefix only when a digit follows, so
    // tags like "verbose-1" pass through untouched.
    if (version.size() >= 2 && (version.startsWith(u'v') || version.startsWith(u'V'))
        && version.at(1).isDigit())
        version.remove(0, 1);
    return version;
}

int UpdateChecker::compareVersions(const QString &a, const QString &b)
{
    const VersionParts pa = splitVersion(tagToVersion(a));
    const VersionParts pb = splitVersion(tagToVersion(b));

    // Numeric components left to right; a missing component counts as 0
    // ("1.0" == "1.0.0"). Non-numeric components already normalized to 0.
    const int count = qMax(pa.numbers.size(), pb.numbers.size());
    for (int i = 0; i < count; ++i) {
        const int x = (i < pa.numbers.size()) ? pa.numbers.at(i) : 0;
        const int y = (i < pb.numbers.size()) ? pb.numbers.at(i) : 0;
        if (x != y)
            return x < y ? -1 : 1;
    }

    // Numerically equal: a release outranks its own pre-releases
    // ("0.2.0" > "0.2.0-beta", "0.2.0-rc.1" < "0.2.0").
    if (pa.suffix.isEmpty() || pb.suffix.isEmpty())
        return pa.suffix.isEmpty() ? (pb.suffix.isEmpty() ? 0 : 1) : -1;

    // Both are pre-releases of the same numbers.
    const int identifierOrder = compareIdentifiers(pa.suffix, pb.suffix);
    if (identifierOrder != 0)
        return identifierOrder;

    // Fully tied (e.g. two unrecognized identifiers): lexicographic so the
    // result is at least deterministic.
    if (pa.suffix == pb.suffix)
        return 0;
    return (pa.suffix < pb.suffix) ? -1 : 1;
}

bool UpdateChecker::isNewer(const QString &current, const QString &candidate)
{
    return compareVersions(candidate, current) > 0;
}

QUrl UpdateChecker::releasesUrl() const
{
    QString base = m_apiBase;
    while (base.endsWith(u'/'))
        base.chop(1);
    return QUrl(base + QLatin1String(kLatestReleasePath));
}

void UpdateChecker::setApiBase(const QString &apiBase)
{
    m_apiBase = apiBase;
}

void UpdateChecker::check()
{
    // One in-flight check at a time; a second check() during a request is a
    // no-op (main.cpp only schedules one per launch anyway).
    if (m_reply)
        return;

    QNetworkRequest request(releasesUrl());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    // GitHub's REST API requires a User-Agent; identify the app and version
    // (documented requirement, not decoration).
    request.setRawHeader(
        "User-Agent",
        QStringLiteral("EmoPlay/%1 (+%2)")
            .arg(m_currentVersion, QLatin1String(kReleasePageBase))
            .toLatin1());
    request.setRawHeader("Accept", "application/vnd.github+json");
    // Bounded transfer so an unreachable endpoint cannot hang the check
    // forever; the timeout lands on the same silent failure path.
    request.setTransferTimeout(15000);

    m_reply = m_nam->get(request);
    connect(m_reply, &QNetworkReply::finished, this,
            &UpdateChecker::finishCheck);
}

void UpdateChecker::finishCheck()
{
    QNetworkReply *reply = m_reply.data();
    if (!reply)
        return;
    m_reply.clear();
    reply->deleteLater();
    emit checkFinished();

    // Silent failure: transport/HTTP errors (offline, 404, rate limit,
    // transfer timeout) keep every property at its default.
    if (reply->error() != QNetworkReply::NoError)
        return;

    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
    if (!document.isObject())
        return;
    const QJsonObject object = document.object();
    const QString tag = object.value(QLatin1String("tag_name")).toString();
    if (tag.isEmpty())
        return;

    const QString version = tagToVersion(tag);
    if (!isNewer(m_currentVersion, version))
        return;

    // Never auto-download: the only thing surfaced is the release page URL,
    // which QML opens when (and only when) the user clicks the badge.
    QUrl url(object.value(QLatin1String("html_url")).toString());
    if (!url.isValid() || url.toString().isEmpty()) {
        url = QUrl(QStringLiteral("%1/tag/%2")
                       .arg(QLatin1String(kReleasePageBase), tag));
    }
    setFound(version, url);
    emit updateAvailable(version, url);
}

void UpdateChecker::setFound(const QString &version, const QUrl &url)
{
    if (m_hasUpdate && m_latestVersion == version
        && m_releaseUrl == url.toString())
        return;
    m_hasUpdate = true;
    m_latestVersion = version;
    m_releaseUrl = url.toString();
    emit hasUpdateChanged();
    emit latestVersionChanged();
    emit releaseUrlChanged();
}
