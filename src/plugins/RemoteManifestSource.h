#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVector>

#include "plugins/PluginManifest.h"
#include "plugins/PluginRemoteFetcher.h"

class PluginManager;

// P8 v2 online plugin manifest sources: fetches manager.json-compatible
// catalogs (docs/nativeization-plan.md §4.1) over HTTP(S), merges them with
// the local install state for the drawer's online section, and caches every
// successful fetch so an offline machine still shows the last known list.
// Network failure is a hint, never a crash (graceful degradation contract):
// the cached manifests stay loaded and errorOccurred reports the problem.
//
// Sources: the default upstream manifest plus user-added custom URLs,
// persisted under <appDataDir>/plugin-sources/sources.json. Every successful
// fetch is cached to <appDataDir>/plugin-sources/<source>.json (a wrapper
// carrying the fetch time and the raw body) and reloaded from there on the
// next start, so entries are available before the first refresh.
//
// Cross-source rule: sources are polled in order (default first, custom in
// insertion order) and the FIRST source declaring a plugin name wins. The
// shipped builtin catalog keeps priority over ALL online sources via
// PluginManager::setRemoteCatalog (builtin names are never shadowed).
//
// The network goes through the injectable PluginRemoteFetcher seam (unit
// tests answer synchronously, no real HTTP); the merge logic is a pure
// static function unit-tested without network or git.
class RemoteManifestSource : public QObject
{
    Q_OBJECT

    // Merged display rows: remote manifests x local install state
    // (installed/enabled/head/updateAvailable — see mergeCatalog()).
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    // One row per configured source: {url, isDefault, lastRefreshMs}.
    Q_PROPERTY(QVariantList sources READ sources NOTIFY changed)
    // True while a refresh cycle (possibly several queued fetches) runs.
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY changed)

public:
    // Production constructor: QNetworkAccessManager fetcher, |manager|
    // optional (merge target + remote-catalog receiver).
    explicit RemoteManifestSource(const QString &appDataDir,
                                  QObject *parent = nullptr);
    // Test constructor: injectable fetcher, optional manager.
    RemoteManifestSource(const QString &appDataDir, PluginManager *manager,
                         PluginRemoteFetcher *fetcher, QObject *parent = nullptr);

    QVariantList entries() const { return m_entries; }
    QVariantList sources() const { return m_sourceRows; }
    bool refreshing() const { return m_refreshing; }

    // ---- pure helpers (unit-tested without network or git) ----

    static QString defaultSourceUrl();
    // Cache file name (inside the plugin-sources directory) for a source
    // URL: sanitized host + a short digest of the full URL, so two sources
    // on one host with different paths never share a file.
    static QString cacheFileName(const QUrl &url);
    // The merge the drawer displays: |manifests| (already the deduplicated
    // union across sources) x |localPlugins| rows shaped like
    // PluginManager::plugins(). Remote-only names default to "not
    // installed"; known names copy installed/enabled/head/remoteHead/
    // updateAvailable/locallyModified from the local row. The manifest
    // fields (name/url/branch/source/destination/mode/baseline) keep their
    // remote values — including the URL of a plugin that is ALSO in the
    // builtin catalog (the manager's builtin row stays authoritative there).
    static QVariantList mergeCatalog(const QVector<PluginManifest> &manifests,
                                     const QVariantList &localPlugins);

    // ---- QML API ----

    // Queues every configured source (default first); one HTTP fetch at a
    // time, results cache + merge as the replies arrive. No-op while a
    // refresh cycle is already running.
    Q_INVOKABLE void refreshAll();
    // Adds a custom http(s) source, persists it and fetches it right away.
    // False (with errorOccurred) for invalid schemes, host-less URLs and
    // duplicates of an already-configured source.
    Q_INVOKABLE bool addSource(const QUrl &url);
    // Removes a custom source (its cache file goes with it); the default
    // source cannot be removed. False (with errorOccurred) for the default
    // and for unknown URLs.
    Q_INVOKABLE bool removeSource(const QUrl &url);

signals:
    void changed();
    void errorOccurred(const QString &message);

private:
    struct SourceEntry
    {
        QUrl url;
        bool isDefault = false;
    };

    void init(PluginManager *manager, PluginRemoteFetcher *fetcher);
    void loadSourceList();
    bool saveSourceList() const;
    // Reloads manifests + fetch times from every per-source cache file.
    void loadCachedManifests();
    void loadCachedManifestsFor(const QUrl &url);
    void storeCache(const QUrl &url, const QByteArray &body);
    QString cachePathFor(const QUrl &url) const;
    // Deduplicated union across sources, in source order (first wins).
    QVector<PluginManifest> unionManifests() const;
    QVariantMap sourceRow(const SourceEntry &source) const;
    void recomputeEntries();
    // recomputeEntries + push the union into PluginManager (when present).
    void rebuildEntries();
    void startNextFetch();
    void onFetchFinished(const QUrl &url, const PluginRemoteFetchResult &result);
    void onManagerChanged();

    QString m_cacheDir;
    QString m_sourceListPath;
    PluginManager *m_manager = nullptr;
    PluginRemoteFetcher *m_fetcher = nullptr;
    bool m_ownedFetcher = false;
    QVector<SourceEntry> m_sources;
    // Key = url.toString(): per-source manifests + last successful fetch.
    QHash<QString, QVector<PluginManifest>> m_manifests;
    QHash<QString, QDateTime> m_lastRefresh;
    QVariantList m_entries;
    QVariantList m_sourceRows;
    QVector<QUrl> m_fetchQueue;
    QUrl m_currentUrl;
    bool m_refreshing = false;
};
