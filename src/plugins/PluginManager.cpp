#include "plugins/PluginManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QtGlobal>

namespace {

constexpr auto kStatePlugins = "plugins";
constexpr auto kStateEnabled = "enabled";
constexpr auto kStateHead = "head";

QString actionTitle(const QString &action)
{
    if (action == QLatin1String("install"))
        return QStringLiteral("Install plugin");
    if (action == QLatin1String("uninstall"))
        return QStringLiteral("Uninstall plugin");
    if (action == QLatin1String("update"))
        return QStringLiteral("Update plugin");
    return QStringLiteral("Check for update");
}

} // namespace

QString PluginManager::defaultCatalogPath()
{
    // Compiled into the app target via qt_add_resources (PREFIX "/").
    return QStringLiteral("qrc:/assets/plugins/builtin_sources.json");
}

PluginManager::PluginManager(const QString &appDataDir,
                             const QString &catalogPath, QObject *parent)
    : PluginManager(appDataDir, catalogPath, nullptr, parent)
{
}

PluginManager::PluginManager(const QString &appDataDir,
                             const QString &catalogPath,
                             PluginGitRunner *gitRunner, QObject *parent)
    : QObject(parent),
      m_appDataDir(appDataDir),
      m_pluginsRoot(appDataDir + QStringLiteral("/plugins")),
      m_statePath(m_pluginsRoot + QStringLiteral("/state.json")),
      m_catalogPath(catalogPath),
      m_git(gitRunner)
{
    // git runs with the plugin root as cwd, so it must exist before the
    // first QProcess starts (QStandardPaths dirs are not auto-created).
    QDir::root().mkpath(m_pluginsRoot);

    if (!m_git) {
        // Owned here: pending git children are killed when the manager dies
        // (QProcess kills its running children on destruction).
        m_git = new QProcessPluginGitRunner(this);
    }
    // The state machine routes through the runner's finished() signal for
    // both the owned production runner and the injected test fake.
    connect(m_git, &PluginGitRunner::finished, this,
            &PluginManager::onGitFinished);

    if (!loadCatalog())
        emit errorOccurred(QStringLiteral("cannot read plugin catalog %1")
                               .arg(catalogPath));
    loadState();
}

bool PluginManager::loadCatalog()
{
    QFile file(m_catalogPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QString error;
    // Fail closed: a half-readable catalog must not silently drop entries.
    m_catalog = PluginManifest::parseList(file.readAll(), &error);
    if (m_catalog.isEmpty() && !error.isEmpty()) {
        qWarning("plugins: catalog %s: %s", qPrintable(m_catalogPath),
                 qPrintable(error));
    }
    return !m_catalog.isEmpty();
}

void PluginManager::loadState()
{
    m_state.clear();
    QFile file(m_statePath);
    if (!file.open(QIODevice::ReadOnly))
        return; // no state yet: everything defaults to disabled/absent
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonObject plugins = root.value(QLatin1String(kStatePlugins)).toObject();
    for (auto it = plugins.constBegin(); it != plugins.constEnd(); ++it) {
        const QJsonObject entry = it.value().toObject();
        PluginState state;
        state.enabled = entry.value(QLatin1String(kStateEnabled)).toBool();
        state.head = entry.value(QLatin1String(kStateHead)).toString();
        m_state.insert(it.key(), state);
    }
}

bool PluginManager::saveState()
{
    QJsonObject plugins;
    for (auto it = m_state.constBegin(); it != m_state.constEnd(); ++it) {
        QJsonObject entry;
        entry.insert(QLatin1String(kStateEnabled), it->enabled);
        entry.insert(QLatin1String(kStateHead), it->head);
        plugins.insert(it.key(), entry);
    }
    QJsonObject root;
    root.insert(QLatin1String(kStatePlugins), plugins);

    QSaveFile file(m_statePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

PluginManifest PluginManager::manifestFor(const QString &name) const
{
    for (const PluginManifest &manifest : m_catalog) {
        if (manifest.name == name)
            return manifest;
    }
    return PluginManifest(); // invalid
}

bool PluginManager::isInstalled(const QString &name) const
{
    return QFileInfo::exists(pluginDir(name));
}

QString PluginManager::pluginDir(const QString &name) const
{
    return m_pluginsRoot + QLatin1Char('/') + name;
}

// ---- pure helpers -----------------------------------------------------------

QLatin1Char PluginManager::scriptSeparator()
{
    // mpv's --scripts is an OPT_PATHLIST: ':' on Unix, ';' on Windows
    // (options.rst "--scripts=file1.lua:file2.lua:..."), NOT the ',' used by
    // ordinary list options. Same convention as CompatController.
#if defined(Q_OS_WIN)
    return QLatin1Char(';');
#else
    return QLatin1Char(':');
#endif
}

QStringList PluginManager::scriptEntryPaths(const PluginManifest &manifest,
                                            const QString &pluginsRoot)
{
    if (!manifest.valid || !manifest.isScriptPlugin())
        return {};
    const QString contentRoot =
        manifest.contentRoot(pluginsRoot + QLatin1Char('/') + manifest.name);
    const QString base = QFileInfo(manifest.destination).fileName();
    // Single-file destination (manager.json also carries entries like
    // "scripts/mpv360.lua"): the entry is the file inside the content root.
    if (base.endsWith(QLatin1String(".lua"), Qt::CaseInsensitive)
        || base.endsWith(QLatin1String(".js"), Qt::CaseInsensitive))
        return { contentRoot + QLatin1Char('/') + base };
    // Directory destination: mpv loads a directory entry from the --scripts
    // path list via its "main.lua" (mpv 0.37.0 player/scripting.c,
    // mp_load_script: S_ISDIR branch looks up "main.<backend ext>").
    return { contentRoot };
}

bool PluginManager::joinExistingScripts(const QVariant &value, QString *joined)
{
    // The compat layer always stores a string, but tolerate a QStringList
    // value as well; anything else is NOT touched (never overwrite a value
    // we do not understand).
    if (value.typeId() == QMetaType::QString) {
        *joined = value.toString();
        return true;
    }
    if (value.typeId() == QMetaType::QStringList) {
        *joined = value.toStringList().join(scriptSeparator());
        return true;
    }
    return false;
}

void PluginManager::mergeScriptOptions(QVariantMap &options,
                                       const QStringList &entryPaths)
{
    if (entryPaths.isEmpty())
        return; // never create an empty "scripts" value
    const QString addition = entryPaths.join(scriptSeparator());
    if (options.contains(QStringLiteral("scripts"))) {
        QString existing;
        if (!joinExistingScripts(options.value(QStringLiteral("scripts")),
                                 &existing)) {
            qWarning("plugins: refusing to merge into a non-string \"scripts\" "
                     "option");
            return;
        }
        if (!existing.isEmpty()) {
            // Append-only: whatever the user imported keeps its position.
            options.insert(QStringLiteral("scripts"),
                           existing + scriptSeparator() + addition);
            return;
        }
    }
    options.insert(QStringLiteral("scripts"), addition);
}

void PluginManager::appendScriptOptions(QVariantMap &options) const
{
    QStringList entries;
    for (const PluginManifest &manifest : m_catalog) {
        const auto it = m_state.constFind(manifest.name);
        const bool enabled = it != m_state.constEnd() && it->enabled;
        if (enabled && isInstalled(manifest.name))
            entries.append(scriptEntryPaths(manifest, m_pluginsRoot));
    }
    mergeScriptOptions(options, entries);
}

// ---- model ------------------------------------------------------------------

bool PluginManager::updateAvailable(const PluginManifest &manifest,
                                    const PluginState &state) const
{
    if (!isInstalled(manifest.name))
        return false;
    const QString remote = m_remoteHeads.value(manifest.name);
    if (remote.isEmpty())
        return false;
    // check mode: the manifest baseline is the reference (upstream moved
    // ahead of what the local customizations are based on). Without a
    // baseline (and always for replace mode) the reference is the recorded
    // install HEAD.
    const QString reference =
        (!manifest.isReplace() && !manifest.baseline.isEmpty())
            ? manifest.baseline
            : state.head;
    return remote != reference;
}

QVariantList PluginManager::plugins() const
{
    QVariantList list;
    list.reserve(m_catalog.size());
    for (const PluginManifest &manifest : m_catalog) {
        const PluginState state = m_state.value(manifest.name);
        const bool installed = isInstalled(manifest.name);
        const QStringList entryPaths =
            scriptEntryPaths(manifest, m_pluginsRoot);

        QVariantMap entry;
        entry.insert(QStringLiteral("name"), manifest.name);
        entry.insert(QStringLiteral("url"), manifest.url);
        entry.insert(QStringLiteral("branch"), manifest.branch);
        entry.insert(QStringLiteral("source"), manifest.source);
        entry.insert(QStringLiteral("destination"), manifest.destination);
        entry.insert(QStringLiteral("mode"), manifest.mode);
        entry.insert(QStringLiteral("baseline"), manifest.baseline);
        entry.insert(QStringLiteral("isScript"), manifest.isScriptPlugin());
        entry.insert(QStringLiteral("installed"), installed);
        entry.insert(QStringLiteral("enabled"),
                     installed && manifest.isScriptPlugin() && state.enabled);
        entry.insert(QStringLiteral("head"), state.head);
        entry.insert(QStringLiteral("remoteHead"), m_remoteHeads.value(manifest.name));
        entry.insert(QStringLiteral("updateAvailable"),
                     updateAvailable(manifest, state));
        entry.insert(QStringLiteral("locallyModified"),
                     m_locallyModified.value(manifest.name, false));
        entry.insert(QStringLiteral("entryPath"),
                     entryPaths.value(0, QString()));
        list.append(entry);
    }
    return list;
}

// ---- planning ---------------------------------------------------------------

QVariantMap PluginManager::planOperation(const QString &name,
                                         const QString &action)
{
    QVariantMap plan;
    const PluginManifest manifest = manifestFor(name);
    if (!manifest.valid) {
        plan.insert(QStringLiteral("error"),
                    QStringLiteral("unknown plugin \"%1\"").arg(name));
        return plan;
    }
    const bool isInstall = action == QLatin1String("install");
    const bool isUninstall = action == QLatin1String("uninstall");
    const bool isUpdate = action == QLatin1String("update");
    const bool isCheck = action == QLatin1String("check");
    if (!isInstall && !isUninstall && !isUpdate && !isCheck) {
        plan.insert(QStringLiteral("error"),
                    QStringLiteral("unknown action \"%1\"").arg(action));
        return plan;
    }

    QStringList details;
    if (isInstall) {
        details.append(QStringLiteral("Clone %1 (branch %2, --depth 1)")
                           .arg(manifest.url, manifest.branch));
        details.append(QStringLiteral("Install into %1").arg(pluginDir(name)));
        if (manifest.isReplace()) {
            details.append(QStringLiteral(
                "Mode replace: later updates replace this directory. A backup "
                "is kept and the replace is refused when local modifications "
                "are detected."));
        } else {
            details.append(QStringLiteral(
                "Mode check: later updates only compare the baseline — your "
                "local customizations are never overwritten."));
        }
    } else if (isUninstall) {
        details.append(
            QStringLiteral("Delete %1 including any local customizations "
                           "(irreversible).")
                .arg(pluginDir(name)));
        details.append(QStringLiteral(
            "Its paths disappear from the scripts option on the next launch."));
    } else if (isUpdate && !manifest.isReplace()) {
        // check mode: "update" can only mean a comparison (the nativeization
        // plan's baseline flow needs a manual three-way merge; this batch
        // never rewrites customized files).
        details.append(
            QStringLiteral(
                "Check only: compare the remote HEAD of %1 (%2) against the "
                "baseline %3 via git ls-remote. No local file is touched.")
                .arg(manifest.url, manifest.branch,
                     manifest.baseline.isEmpty()
                         ? QStringLiteral("<recorded install HEAD>")
                         : manifest.baseline));
        details.append(QStringLiteral(
            "Upstream moves ahead of the baseline require a manual merge of "
            "your local customizations; local customizations are never "
            "overwritten automatically."));
    } else if (isUpdate) {
        details.append(QStringLiteral("Re-clone %1 (branch %2) into %3")
                           .arg(manifest.url, manifest.branch,
                                m_pluginsRoot + QStringLiteral("/.staging-")
                                    + name));
        details.append(QStringLiteral("Move %1 to %2 (backup first)")
                           .arg(pluginDir(name),
                                m_pluginsRoot + QStringLiteral("/.backup-")
                                    + name));
        details.append(QStringLiteral("Swap the fresh clone into %1 atomically")
                           .arg(pluginDir(name)));
        details.append(QStringLiteral(
            "Refused when git reports local modifications — local "
            "customizations are never overwritten."));
    } else {
        details.append(QStringLiteral(
            "Run git ls-remote against %1 (branch %2) and compare the result "
            "with the baseline/installed HEAD. No file is touched.")
                           .arg(manifest.url, manifest.branch));
    }

    plan.insert(QStringLiteral("name"), name);
    plan.insert(QStringLiteral("action"), action);
    plan.insert(QStringLiteral("title"),
                actionTitle(action) + QStringLiteral(": ") + name);
    plan.insert(QStringLiteral("details"), details);
    return plan;
}

// ---- operations -------------------------------------------------------------

void PluginManager::setEnabled(const QString &name, bool enabled)
{
    const PluginManifest manifest = manifestFor(name);
    if (!manifest.valid || !manifest.isScriptPlugin())
        return;
    const auto it = m_state.find(name);
    if (enabled && !isInstalled(name))
        return;
    if (it != m_state.end()) {
        if (it->enabled == enabled)
            return;
        it->enabled = enabled;
    } else {
        if (!enabled)
            return;
        PluginState state;
        state.enabled = true;
        m_state.insert(name, state);
    }
    if (!saveState())
        emit errorOccurred(QStringLiteral("cannot save %1").arg(m_statePath));
    emit changed();
}

void PluginManager::install(const QString &name)
{
    const PluginManifest manifest = manifestFor(name);
    if (!manifest.valid) {
        emit operationFinished(name, QStringLiteral("install"), false,
                               QStringLiteral("unknown plugin \"%1\"").arg(name));
        return;
    }
    if (m_busy || isInstalled(name)) {
        emit operationFinished(name, QStringLiteral("install"), false,
                               m_busy ? QStringLiteral("another operation is in progress")
                                      : QStringLiteral("already installed"));
        return;
    }
    begin(name, Action::Install);
}

void PluginManager::uninstall(const QString &name)
{
    const PluginManifest manifest = manifestFor(name);
    if (!manifest.valid) {
        emit operationFinished(name, QStringLiteral("uninstall"), false,
                               QStringLiteral("unknown plugin \"%1\"").arg(name));
        return;
    }
    if (m_busy || !isInstalled(name)) {
        emit operationFinished(name, QStringLiteral("uninstall"), false,
                               m_busy ? QStringLiteral("another operation is in progress")
                                      : QStringLiteral("not installed"));
        return;
    }
    // File removal runs synchronously: bounded by the plugin directory size,
    // and no git/network is involved.
    QDir dir(pluginDir(name));
    if (!dir.removeRecursively()) {
        emit operationFinished(name, QStringLiteral("uninstall"), false,
                               QStringLiteral("failed to delete %1").arg(dir.path()));
        return;
    }
    m_state.remove(name);
    m_remoteHeads.remove(name);
    m_locallyModified.remove(name);
    if (!saveState())
        emit errorOccurred(QStringLiteral("cannot save %1").arg(m_statePath));
    emit changed();
    emit operationFinished(name, QStringLiteral("uninstall"), true,
                           QStringLiteral("uninstalled %1").arg(name));
}

void PluginManager::update(const QString &name)
{
    const PluginManifest manifest = manifestFor(name);
    if (!manifest.valid) {
        emit operationFinished(name, QStringLiteral("update"), false,
                               QStringLiteral("unknown plugin \"%1\"").arg(name));
        return;
    }
    if (m_busy) {
        emit operationFinished(name, QStringLiteral("update"), false,
                               QStringLiteral("another operation is in progress"));
        return;
    }
    if (!isInstalled(name)) {
        // Both modes compare against the local install; a missing install
        // is an install, not an update.
        emit operationFinished(name, QStringLiteral("update"), false,
                               QStringLiteral("not installed"));
        return;
    }
    begin(name, Action::Update);
}

void PluginManager::checkForUpdate(const QString &name)
{
    const PluginManifest manifest = manifestFor(name);
    if (!manifest.valid) {
        emit operationFinished(name, QStringLiteral("check"), false,
                               QStringLiteral("unknown plugin \"%1\"").arg(name));
        return;
    }
    if (m_busy) {
        emit operationFinished(name, QStringLiteral("check"), false,
                               QStringLiteral("another operation is in progress"));
        return;
    }
    begin(name, Action::Check);
}

void PluginManager::checkAll()
{
    if (m_busy || m_catalog.isEmpty())
        return;
    m_checkQueue.clear();
    for (const PluginManifest &manifest : m_catalog)
        m_checkQueue.append(manifest.name);
    begin(m_checkQueue.takeFirst(), Action::CheckAll);
}

// ---- state machine ----------------------------------------------------------

void PluginManager::begin(const QString &name, Action action)
{
    m_busy = true;
    m_currentName = name;
    m_action = action;
    m_newHead.clear();
    const PluginManifest manifest = manifestFor(name);
    switch (action) {
    case Action::Install:
        m_step = Step::Clone;
        break;
    case Action::Update:
        // check mode never rewrites files — its "update" is the comparison.
        m_step = manifest.isReplace() ? Step::Status : Step::LsRemote;
        break;
    case Action::Uninstall:
    case Action::Check:
    case Action::CheckAll:
        m_step = Step::LsRemote;
        break;
    }
    emit changed();
    runStep();
}

void PluginManager::runStep()
{
    const PluginManifest manifest = manifestFor(m_currentName);
    Q_ASSERT(manifest.valid);

    switch (m_step) {
    case Step::Clone: {
        const QString target = m_action == Action::Install
                                   ? pluginDir(m_currentName)
                                   : m_stagingDir;
        if (m_action != Action::Install) {
            // Remove a leftover staging directory from a crashed run.
            if (QFileInfo::exists(m_stagingDir))
                QDir(m_stagingDir).removeRecursively();
        }
        m_git->run(QStringList{QStringLiteral("clone"), QStringLiteral("--depth"),
                               QStringLiteral("1"), QStringLiteral("--branch"),
                               manifest.branch, QStringLiteral("--"),
                               manifest.url, target},
                   m_pluginsRoot);
        break;
    }
    case Step::ReadHead:
        m_git->run(QStringList{QStringLiteral("rev-parse"), QStringLiteral("HEAD")},
                   m_action == Action::Install ? pluginDir(m_currentName)
                                               : m_stagingDir);
        break;
    case Step::LsRemote:
        m_git->run(QStringList{QStringLiteral("ls-remote"), manifest.url,
                               QStringLiteral("refs/heads/") + manifest.branch},
                   m_pluginsRoot);
        break;
    case Step::Status:
        m_git->run(QStringList{QStringLiteral("status"), QStringLiteral("--porcelain")},
                   pluginDir(m_currentName));
        break;
    }
}

void PluginManager::onGitFinished(const QStringList &arguments,
                                  const QString &workingDirectory,
                                  const PluginGitResult &result)
{
    Q_UNUSED(arguments);
    Q_UNUSED(workingDirectory);
    if (!m_busy)
        return; // stray signal (should not happen; single-flight invariant)

    switch (m_step) {
    case Step::Clone: {
        if (!result.started || result.exitCode != 0) {
            if (m_action != Action::Install
                && QFileInfo::exists(m_stagingDir)) {
                QDir(m_stagingDir).removeRecursively();
            }
            fail(QStringLiteral("git clone failed: %1")
                     .arg(result.standardError.trimmed()));
            return;
        }
        m_step = Step::ReadHead;
        runStep();
        return;
    }
    case Step::ReadHead: {
        if (!result.started || result.exitCode != 0) {
            fail(QStringLiteral("git rev-parse failed: %1")
                     .arg(result.standardError.trimmed()));
            return;
        }
        m_newHead = result.standardOutput.trimmed();
        if (m_action == Action::Install) {
            PluginState state;
            state.enabled = true; // installing expresses the intent to use it
            state.head = m_newHead;
            m_state.insert(m_currentName, state);
            if (!saveState())
                emit errorOccurred(
                    QStringLiteral("cannot save %1").arg(m_statePath));
            emit changed();
            // The full HEAD is recorded here and in state.json alike (the
            // drawer shows it); no 12-char truncation in the message.
            finish(true, QStringLiteral("installed %1 (%2)")
                             .arg(m_currentName, m_newHead));
            return;
        }
        // Update (replace mode): swap staging into place.
        performSwap();
        return;
    }
    case Step::LsRemote: {
        if (!result.started || result.exitCode != 0) {
            fail(QStringLiteral("git ls-remote failed: %1")
                     .arg(result.standardError.trimmed()));
            return;
        }
        const QString line =
            result.standardOutput.section(QLatin1Char('\n'), 0, 0);
        const QString remoteHead =
            line.section(QLatin1Char('\t'), 0, 0).trimmed();
        if (remoteHead.isEmpty()) {
            fail(QStringLiteral("branch not found on remote"));
            return;
        }
        m_remoteHeads.insert(m_currentName, remoteHead);
        if (m_action == Action::Check || m_action == Action::CheckAll
            || (m_action == Action::Update && !manifestFor(m_currentName).isReplace())) {
            // check-mode "update" and plain checks: only the comparison,
            // never file changes. Additionally probe local modifications so
            // the drawer can show the customization badge.
            if (isInstalled(m_currentName)) {
                m_step = Step::Status;
                runStep();
            } else {
                finishCheck();
            }
            return;
        }
        // Update (replace mode): guard against local modifications first.
        m_step = Step::Status;
        runStep();
        return;
    }
    case Step::Status: {
        if (!result.started || result.exitCode != 0) {
            fail(QStringLiteral("git status failed: %1")
                     .arg(result.standardError.trimmed()));
            return;
        }
        const bool modified = !result.standardOutput.trimmed().isEmpty();
        m_locallyModified.insert(m_currentName, modified);

        // Only a replace-mode update may proceed to the swap; a check-mode
        // update is the comparison itself and must never clone or rewrite
        // files (本地定制永不覆盖) — same contract as Action::Check.
        if (m_action == Action::Update
            && manifestFor(m_currentName).isReplace()) {
            if (modified) {
                // Protective skip: local customizations win, always.
                fail(QStringLiteral(
                    "local modifications detected — refusing to replace "
                    "(local customizations are never overwritten)"));
                return;
            }
            m_stagingDir = m_pluginsRoot + QStringLiteral("/.staging-")
                           + m_currentName;
            m_step = Step::Clone;
            runStep();
            return;
        }
        finishCheck();
        return;
    }
    }
}

void PluginManager::finishCheck()
{
    const PluginManifest manifest = manifestFor(m_currentName);
    const QString remote = m_remoteHeads.value(m_currentName);
    const PluginState state = m_state.value(m_currentName);
    const bool available = updateAvailable(manifest, state);
    QString message;
    if (!isInstalled(m_currentName)) {
        message = QStringLiteral("not installed (remote HEAD %1)")
                      .arg(remote.left(12));
    } else if (remote.isEmpty()) {
        message = QStringLiteral("could not determine the remote HEAD");
    } else if (available) {
        message = QStringLiteral("update available: %1 -> %2")
                      .arg(manifest.baseline.isEmpty() ? state.head
                                                       : manifest.baseline,
                           remote);
    } else {
        message = QStringLiteral("up to date");
    }
    emit changed();
    finish(true, message);
}

void PluginManager::performSwap()
{
    QDir root(m_pluginsRoot);
    const QString dir = pluginDir(m_currentName);
    const QString backup =
        m_pluginsRoot + QStringLiteral("/.backup-") + m_currentName;
    const QString oldHead = m_state.value(m_currentName).head;

    // Keep exactly one generation of backups.
    // (QDir::removeRecursively(const QString &) only exists from Qt 6.5; the
    // supported Qt 6.4 has the zero-argument overload only.)
    if (QFileInfo::exists(backup) && !QDir(backup).removeRecursively()) {
        fail(QStringLiteral("cannot remove the old backup %1").arg(backup));
        return;
    }
    if (!root.rename(dir, backup)) {
        fail(QStringLiteral("cannot move %1 to %2").arg(dir, backup));
        return;
    }
    if (!root.rename(m_stagingDir, dir)) {
        root.rename(backup, dir); // best-effort restore
        fail(QStringLiteral("cannot move %1 to %2").arg(m_stagingDir, dir));
        return;
    }
    m_state[m_currentName].head = m_newHead;
    if (!saveState())
        emit errorOccurred(QStringLiteral("cannot save %1").arg(m_statePath));
    emit changed();
    finish(true, QStringLiteral("updated %1 (%2 -> %3); old install kept at %4")
                     .arg(m_currentName, oldHead.left(12), m_newHead.left(12),
                          backup));
}

QString PluginManager::currentActionString() const
{
    switch (m_action) {
    case Action::Install: return QStringLiteral("install");
    case Action::Uninstall: return QStringLiteral("uninstall");
    case Action::Update: return QStringLiteral("update");
    case Action::Check:
    case Action::CheckAll: return QStringLiteral("check");
    }
    return QStringLiteral("check");
}

void PluginManager::finish(bool success, const QString &message)
{
    const QString action = currentActionString();
    const QString name = m_currentName;
    m_busy = false;
    m_currentName.clear();
    m_stagingDir.clear();
    emit operationFinished(name, action, success, message);
    emit changed();
    if (m_action == Action::CheckAll && !m_checkQueue.isEmpty())
        begin(m_checkQueue.takeFirst(), Action::CheckAll);
}

void PluginManager::fail(const QString &message)
{
    qWarning("plugins: %s: %s", qPrintable(m_currentName), qPrintable(message));
    finish(false, message);
}
