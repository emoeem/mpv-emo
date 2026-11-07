#include "plugins/RemoteManifestSource.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QtGlobal>

namespace {

bool isFetchableUrl(const QUrl &url)
{
    return url.isValid() && !url.host().isEmpty()
           && (url.scheme() == QLatin1String("https")
               || url.scheme() == QLatin1String("http"));
}

} // namespace

QString RemoteManifestSource::defaultSourceUrl()
{
    // P8 v2 default source: the user repository's manager.json, the same
    // format the builtin catalog was transcribed from
    // (assets/plugins/builtin_sources.json).
    return QStringLiteral(
        "https://raw.githubusercontent.com/emoeem/mpv/main/manager.json");
}

RemoteManifestSource::RemoteManifestSource(const QString &appDataDir,
                                           QObject *parent)
    : RemoteManifestSource(appDataDir, nullptr, nullptr, parent)
{
}

RemoteManifestSource::RemoteManifestSource(const QString &appDataDir,
                                           PluginManager *manager,
                                           PluginRemoteFetcher *fetcher,
                                           QObject *parent)
    : QObject(parent),
      m_cacheDir(appDataDir + QStringLiteral("/plugin-sources")),
      m_sourceListPath(m_cacheDir + QStringLiteral("/sources.json")),
      m_manager(manager)
{
    init(manager, fetcher);
}

void RemoteManifestSource::init(PluginManager *manager,
                                PluginRemoteFetcher *fetcher)
{
    m_manager = manager;
    // Cache files are written via QSaveFile, which does not create the
    // directory itself.
    QDir::root().mkpath(m_cacheDir);
    if (fetcher) {
        m_fetcher = fetcher;
    } else {
        m_ownedFetcher = true;
        m_fetcher = new QNetworkPluginRemoteFetcher(this);
    }
    connect(m_fetcher, &PluginRemoteFetcher::finished, this,
            &RemoteManifestSource::onFetchFinished);

    loadSourceList();
    loadCachedManifests();
    rebuildEntries();

    if (m_manager) {
        // Re-merge when the local install state moves (install/uninstall/
        // enable/check), so the online section always mirrors the store.
        connect(m_manager, &PluginManager::changed, this,
                &RemoteManifestSource::onManagerChanged);
    }
}

QString RemoteManifestSource::cacheFileName(const QUrl &url)
{
    QString host;
    host.reserve(url.host().size());
    for (const QChar character : url.host()) {
        if (character.isLetterOrNumber() || character == QLatin1Char('.')
            || character == QLatin1Char('-'))
            host.append(character);
        else
            host.append(QLatin1Char('_'));
    }
    const QByteArray digest =
        QCryptographicHash::hash(url.toString().toUtf8(),
                                 QCryptographicHash::Sha256).toHex();
    return QStringLiteral("%1-%2.json")
        .arg(host.isEmpty() ? QStringLiteral("source") : host,
             QString::fromLatin1(digest.left(12)));
}

// ---- source list ------------------------------------------------------------

void RemoteManifestSource::loadSourceList()
{
    m_sources.clear();
    SourceEntry defaultSource;
    defaultSource.url = QUrl(defaultSourceUrl());
    defaultSource.isDefault = true;
    m_sources.append(defaultSource);

    QFile file(m_sourceListPath);
    if (!file.open(QIODevice::ReadOnly))
        return; // first run: only the default source
    const QJsonArray sources = QJsonDocument::fromJson(file.readAll())
                                   .object()
                                   .value(QLatin1String("sources"))
                                   .toArray();
    for (const QJsonValue &value : sources) {
        const QUrl url(value.toString());
        // Tolerate garbage and a persisted copy of the default: neither
        // becomes a second source row.
        if (!isFetchableUrl(url) || url == defaultSource.url)
            continue;
        SourceEntry entry;
        entry.url = url;
        m_sources.append(entry);
    }
}

bool RemoteManifestSource::saveSourceList() const
{
    QJsonArray sources;
    for (const SourceEntry &source : m_sources) {
        if (!source.isDefault)
            sources.append(source.url.toString());
    }
    QJsonObject root;
    root.insert(QLatin1String("sources"), sources);

    QSaveFile file(m_sourceListPath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

// ---- cache ------------------------------------------------------------------

QString RemoteManifestSource::cachePathFor(const QUrl &url) const
{
    return m_cacheDir + QLatin1Char('/') + cacheFileName(url);
}

void RemoteManifestSource::storeCache(const QUrl &url, const QByteArray &body)
{
    // One wrapper per source: the raw body plus the fetch time (reloaded on
    // the next start so the "last refreshed" label survives restarts).
    QJsonObject wrapper;
    wrapper.insert(QLatin1String("url"), url.toString());
    wrapper.insert(QLatin1String("fetchedAt"),
                   QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    wrapper.insert(QLatin1String("body"), QString::fromUtf8(body));

    QSaveFile file(cachePathFor(url));
    if (!file.open(QIODevice::WriteOnly)) {
        emit errorOccurred(QStringLiteral("cannot write plugin-source cache %1")
                               .arg(cachePathFor(url)));
        return; // non-fatal: the in-memory list is still updated
    }
    file.write(QJsonDocument(wrapper).toJson(QJsonDocument::Indented));
    if (!file.commit())
        emit errorOccurred(QStringLiteral("cannot commit plugin-source cache %1")
                               .arg(cachePathFor(url)));
}

void RemoteManifestSource::loadCachedManifests()
{
    for (const SourceEntry &source : m_sources)
        loadCachedManifestsFor(source.url);
}

void RemoteManifestSource::loadCachedManifestsFor(const QUrl &url)
{
    QFile file(cachePathFor(url));
    if (!file.open(QIODevice::ReadOnly))
        return; // no cache yet: the source just has no offline copy
    const QJsonObject wrapper = QJsonDocument::fromJson(file.readAll()).object();
    const QDateTime fetchedAt = QDateTime::fromString(
        wrapper.value(QLatin1String("fetchedAt")).toString(),
        Qt::ISODateWithMs);
    QString error;
    const QVector<PluginManifest> manifests = PluginManifest::parseList(
        wrapper.value(QLatin1String("body")).toString().toUtf8(), &error);
    if (!error.isEmpty()) {
        // Fail closed: a corrupt cache copy is dropped, never fed upstream.
        emit errorOccurred(QStringLiteral("cached manifest %1 is corrupt: %2")
                               .arg(cachePathFor(url), error));
        return;
    }
    m_manifests.insert(url.toString(), manifests);
    if (fetchedAt.isValid())
        m_lastRefresh.insert(url.toString(), fetchedAt);
}

// ---- merge ------------------------------------------------------------------

QVariantList RemoteManifestSource::mergeCatalog(
    const QVector<PluginManifest> &manifests, const QVariantList &localPlugins)
{
    QHash<QString, QVariantMap> local;
    for (const QVariant &value : localPlugins) {
        const QVariantMap row = value.toMap();
        local.insert(row.value(QStringLiteral("name")).toString(), row);
    }

    QVariantList merged;
    merged.reserve(manifests.size());
    QSet<QString> seen;
    for (const PluginManifest &manifest : manifests) {
        if (!manifest.valid || seen.contains(manifest.name))
            continue; // first source wins on cross-source name collisions
        seen.insert(manifest.name);
        const QVariantMap localRow = local.value(manifest.name);
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), manifest.name);
        entry.insert(QStringLiteral("url"), manifest.url);
        entry.insert(QStringLiteral("branch"), manifest.branch);
        entry.insert(QStringLiteral("source"), manifest.source);
        entry.insert(QStringLiteral("destination"), manifest.destination);
        entry.insert(QStringLiteral("mode"), manifest.mode);
        entry.insert(QStringLiteral("baseline"), manifest.baseline);
        entry.insert(QStringLiteral("isScript"), manifest.isScriptPlugin());
        entry.insert(QStringLiteral("installed"),
                     localRow.value(QStringLiteral("installed"), false).toBool());
        entry.insert(QStringLiteral("enabled"),
                     localRow.value(QStringLiteral("enabled"), false).toBool());
        entry.insert(QStringLiteral("head"),
                     localRow.value(QStringLiteral("head")).toString());
        entry.insert(QStringLiteral("remoteHead"),
                     localRow.value(QStringLiteral("remoteHead")).toString());
        entry.insert(QStringLiteral("updateAvailable"),
                     localRow.value(QStringLiteral("updateAvailable"), false)
                         .toBool());
        entry.insert(QStringLiteral("locallyModified"),
                     localRow.value(QStringLiteral("locallyModified"), false)
                         .toBool());
        merged.append(entry);
    }
    return merged;
}

QVector<PluginManifest> RemoteManifestSource::unionManifests() const
{
    QVector<PluginManifest> union_;
    QSet<QString> seen;
    for (const SourceEntry &source : m_sources) {
        const auto it = m_manifests.constFind(source.url.toString());
        if (it == m_manifests.constEnd())
            continue;
        for (const PluginManifest &manifest : it.value()) {
            if (!manifest.valid || seen.contains(manifest.name))
                continue;
            seen.insert(manifest.name);
            union_.append(manifest);
        }
    }
    return union_;
}

QVariantMap RemoteManifestSource::sourceRow(const SourceEntry &source) const
{
    QVariantMap row;
    row.insert(QStringLiteral("url"), source.url.toString());
    row.insert(QStringLiteral("isDefault"), source.isDefault);
    const QDateTime refresh = m_lastRefresh.value(source.url.toString());
    row.insert(QStringLiteral("lastRefreshMs"),
               refresh.isValid() ? refresh.toMSecsSinceEpoch() : 0);
    return row;
}

void RemoteManifestSource::recomputeEntries()
{
    m_entries = mergeCatalog(unionManifests(),
                             m_manager ? m_manager->plugins() : QVariantList());
    m_sourceRows.clear();
    m_sourceRows.reserve(m_sources.size());
    for (const SourceEntry &source : m_sources)
        m_sourceRows.append(sourceRow(source));
}

void RemoteManifestSource::rebuildEntries()
{
    recomputeEntries();
    if (m_manager) {
        // Online manifests become installable/updateable like builtin ones.
        // The manager's changed() then re-merges via onManagerChanged, which
        // does NOT push again — no recursion.
        m_manager->setRemoteCatalog(unionManifests());
    }
    emit changed();
}

void RemoteManifestSource::onManagerChanged()
{
    recomputeEntries();
    emit changed();
}

// ---- fetching ---------------------------------------------------------------

void RemoteManifestSource::refreshAll()
{
    if (m_refreshing)
        return; // one refresh cycle at a time; the queue drains itself
    for (const SourceEntry &source : m_sources)
        m_fetchQueue.append(source.url);
    startNextFetch();
}

void RemoteManifestSource::startNextFetch()
{
    if (m_currentUrl.isValid())
        return; // a fetch is in flight; the queue drains as replies arrive
    if (m_fetchQueue.isEmpty()) {
        if (m_refreshing) {
            m_refreshing = false;
            emit changed();
        }
        return;
    }
    m_refreshing = true;
    m_currentUrl = m_fetchQueue.takeFirst();
    emit changed();
    m_fetcher->fetch(m_currentUrl);
}

void RemoteManifestSource::onFetchFinished(
    const QUrl &url, const PluginRemoteFetchResult &result)
{
    if (url != m_currentUrl)
        return; // stale reply from a superseded fetch
    m_currentUrl = QUrl();

    bool sourceKnown = false;
    for (const SourceEntry &source : m_sources) {
        if (source.url == url) {
            sourceKnown = true;
            break;
        }
    }

    if (sourceKnown && result.success) {
        QString error;
        const QVector<PluginManifest> manifests =
            PluginManifest::parseList(result.body, &error);
        if (!error.isEmpty()) {
            // Fail closed, same contract as the builtin catalog loader: the
            // previous cache + manifests stay untouched, so the last good
            // list remains visible instead of half-parsed entries.
            emit errorOccurred(QStringLiteral("source %1: %2")
                                   .arg(url.toString(), error));
        } else {
            storeCache(url, result.body);
            m_manifests.insert(url.toString(), manifests);
            m_lastRefresh.insert(url.toString(),
                                 QDateTime::currentDateTimeUtc());
            rebuildEntries();
        }
    } else if (sourceKnown) {
        // Network failure: m_manifests keeps the cached manifests loaded at
        // startup — the drawer degrades to the last known list.
        emit errorOccurred(QStringLiteral("cannot fetch %1: %2 — showing the "
                                          "last cached list")
                               .arg(url.toString(), result.error));
    }
    emit changed();
    startNextFetch();
}

// ---- source management ------------------------------------------------------

bool RemoteManifestSource::addSource(const QUrl &url)
{
    if (!isFetchableUrl(url)) {
        emit errorOccurred(QStringLiteral("invalid manifest source \"%1\" — "
                                          "expected an http(s) URL with a host")
                               .arg(url.toString()));
        return false;
    }
    for (const SourceEntry &source : m_sources) {
        if (source.url == url) {
            emit errorOccurred(
                QStringLiteral("source %1 is already configured")
                    .arg(url.toString()));
            return false;
        }
    }
    SourceEntry entry;
    entry.url = url;
    m_sources.append(entry);
    if (!saveSourceList())
        emit errorOccurred(QStringLiteral("cannot save %1").arg(m_sourceListPath));
    // An offline add still shows the cached copy of the new source, if any.
    loadCachedManifestsFor(url);
    rebuildEntries();
    m_fetchQueue.append(url);
    startNextFetch();
    return true;
}

bool RemoteManifestSource::removeSource(const QUrl &url)
{
    bool removed = false;
    QVector<SourceEntry> kept;
    kept.reserve(m_sources.size());
    for (const SourceEntry &source : m_sources) {
        if (!source.isDefault && source.url == url)
            removed = true;
        else
            kept.append(source);
    }
    if (!removed) {
        emit errorOccurred(
            url == QUrl(defaultSourceUrl())
                ? QStringLiteral("the default source cannot be removed")
                : QStringLiteral("source %1 is not configured").arg(url.toString()));
        return false;
    }
    m_sources = kept;
    m_manifests.remove(url.toString());
    m_lastRefresh.remove(url.toString());
    m_fetchQueue.removeAll(url);
    QFile::remove(cachePathFor(url));
    if (!saveSourceList())
        emit errorOccurred(QStringLiteral("cannot save %1").arg(m_sourceListPath));
    rebuildEntries();
    return true;
}
