#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include "plugins/PluginGitRunner.h"
#include "plugins/PluginManifest.h"

// P8 plugin store v1 (docs/nativeization-plan.md §4): installs upstream git
// assets declared in a manager.json-compatible catalog and appends enabled
// script plugins to mpv's --scripts path list.
//
// Plugin content lives under <appDataDir>/plugins/<name>/ as a shallow
// (depth 1) clone of the manifest source, so `git status --porcelain` can
// detect user customizations and future batches can fetch-based update.
//
// Core principle — "local customizations are never overwritten":
//  - mode "check" never touches installed files; update checks only compare
//    the remote HEAD against the manifest baseline (`git ls-remote`).
//  - mode "replace" re-clones into a staging directory and atomically swaps,
//    but refuses when `git status --porcelain` reports local modifications,
//    and always moves the old install to a sibling ".backup-<name>" directory
//    first.
//  - enabling a plugin only APPENDS entry paths to the existing "scripts"
//    option value assembled by the P4 compat layer; the user's own script
//    list is never replaced or reordered.
//
// Every mutating action is planned by planOperation() and must be confirmed
// in QML (PluginDrawer shows the change list in a dialog) before install/
// update/uninstall is invoked.
//
// All git invocations go through the injectable PluginGitRunner interface;
// unit tests drive the whole state machine with a synchronous fake and never
// touch the network.
class PluginManager : public QObject
{
    Q_OBJECT

    // Catalog + live status for the drawer (see plugins() for the fields).
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)

public:
    // |appDataDir| is EmoPlay's AppDataLocation: the plugin root is
    // <appDataDir>/plugins and per-plugin state is <appDataDir>/plugins/
    // state.json. |catalogPath| is a manager.json-style catalog readable via
    // QFile (the app passes "qrc:/plugins/builtin_sources.json"; tests pass
    // a source-tree file).
    explicit PluginManager(const QString &appDataDir,
                           const QString &catalogPath,
                           QObject *parent = nullptr);
    // Injectable git runner for tests.
    PluginManager(const QString &appDataDir, const QString &catalogPath,
                  PluginGitRunner *gitRunner, QObject *parent = nullptr);

    QVariantList plugins() const;
    bool busy() const { return m_busy; }

    // ---- pure helpers (unit-tested without git) ----

    // mpv's --scripts is a path list (OPT_PATHLIST): ':' on Unix, ';' on
    // Windows — never the ',' used by ordinary list options (mpv options.rst
    // "--scripts=file1.lua:file2.lua:...").
    static QLatin1Char scriptSeparator();
    // Paths appended to --scripts for an installed plugin. Script plugins
    // only; shader/asset plugins yield an empty list. For a single-file
    // destination ("scripts/mpv360.lua") the entry is the file inside the
    // content root; for a directory destination the entry is the content
    // root itself, which mpv loads via its "main.lua" (mpv 0.37.0
    // player/scripting.c mp_load_script: a directory entry is loaded through
    // "main.<backend ext>").
    static QStringList scriptEntryPaths(const PluginManifest &manifest,
                                        const QString &pluginsRoot);
    // Appends |entryPaths| to the "scripts" option, preserving whatever the
    // compat layer assembled. An empty |entryPaths| leaves the map untouched
    // (no empty "scripts" value is ever created).
    static void mergeScriptOptions(QVariantMap &options,
                                   const QStringList &entryPaths);

    // ---- QML API ----

    // Human-readable change list for the drawer's confirmation dialog:
    // {name, action, title, details: [..]}. Never performs the operation and
    // never runs git. Unknown name/action yields an "error" key.
    Q_INVOKABLE QVariantMap planOperation(const QString &name,
                                          const QString &action);
    // Executed only after the QML dialog confirmed the plan above.
    Q_INVOKABLE void install(const QString &name);
    Q_INVOKABLE void uninstall(const QString &name);
    Q_INVOKABLE void update(const QString &name);
    // Remote check only: records the remote HEAD (update-available state),
    // never touches files — for check-mode plugins this IS the "update".
    Q_INVOKABLE void checkForUpdate(const QString &name);
    Q_INVOKABLE void checkAll();
    // Takes effect at the next launch (the scripts option is assembled once
    // at startup); the drawer states this explicitly.
    Q_INVOKABLE void setEnabled(const QString &name, bool enabled);
    // Startup hook: appends enabled+installed plugin entry paths to |options|
    // (see mergeScriptOptions).
    void appendScriptOptions(QVariantMap &options) const;

    static QString defaultCatalogPath();

signals:
    void changed();
    // Result of a confirmed operation (including refused pre-flight checks).
    void operationFinished(const QString &name, const QString &action,
                           bool success, const QString &message);
    // Non-fatal problems (unreadable catalog, failed state save...).
    void errorOccurred(const QString &message);

private:
    struct PluginState
    {
        bool enabled = false;
        // Upstream HEAD recorded when the plugin was installed or replaced.
        QString head;
    };

    enum class Action { Install, Uninstall, Update, Check, CheckAll };
    enum class Step {
        Clone,        // git clone --depth 1 --branch <branch> -- <url> <target>
        ReadHead,     // git rev-parse HEAD (in the new install)
        LsRemote,     // git ls-remote <url> refs/heads/<branch>
        Status,       // git status --porcelain (replace-mode guard)
    };

    bool loadCatalog();
    void loadState();
    bool saveState();
    PluginManifest manifestFor(const QString &name) const;
    bool isInstalled(const QString &name) const;
    QString pluginDir(const QString &name) const;
    // Joins a QVariant holding the current "scripts" option value (QString,
    // or QStringList as a defensive fallback) into |joined|. False for any
    // other type — such values are never overwritten.
    static bool joinExistingScripts(const QVariant &value, QString *joined);

    void begin(const QString &name, Action action);
    void runStep();
    QString currentActionString() const;
    void finish(bool success, const QString &message);
    void fail(const QString &message);
    void onGitFinished(const QStringList &arguments,
                       const QString &workingDirectory,
                       const PluginGitResult &result);
    void finishCheck();
    void performSwap();
    // Update-available decision per manifest mode (see class doc).
    bool updateAvailable(const PluginManifest &manifest,
                         const PluginState &state) const;

    QString m_appDataDir;
    QString m_pluginsRoot;
    QString m_statePath;
    QString m_catalogPath;
    QVector<PluginManifest> m_catalog;
    QHash<QString, PluginState> m_state;
    // Live results of the last remote check (not persisted).
    QHash<QString, QString> m_remoteHeads;
    // Live results of the last `git status --porcelain` check (not persisted).
    QHash<QString, bool> m_locallyModified;
    PluginGitRunner *m_git = nullptr;
    bool m_busy = false;
    Action m_action = Action::Check;
    Step m_step = Step::LsRemote;
    QString m_currentName;
    QString m_stagingDir;
    QString m_newHead;
    QStringList m_checkQueue;
};
