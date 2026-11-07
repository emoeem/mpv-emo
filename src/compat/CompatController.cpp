#include "compat/CompatController.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include "compat/MpvImportRules.h"
#include "config/MpvConfigParser.h"

namespace {

QString defaultAppDataDir()
{
    return QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
}

QStringList allRelativePaths(const ImportSummary &summary)
{
    QStringList paths;
    paths.reserve(summary.files.size());
    for (const ImportFileSummary &file : summary.files)
        paths.append(file.relativePath);
    return paths;
}

} // namespace

CompatController::CompatController(QObject *parent)
    : CompatController(AppSettings::defaultFilePath(), defaultAppDataDir(),
                       MpvProfileScanner::locateDefaultConfigDir(), parent)
{
}

CompatController::CompatController(const QString &settingsFilePath,
                                   const QString &appDataDir,
                                   const QString &configDir, QObject *parent)
    : QObject(parent), m_settings(settingsFilePath, this),
      m_appDataDir(appDataDir), m_configDir(configDir)
{
}

bool CompatController::firstRunPromptNeeded() const
{
    return configDetected() && m_settings.importDecision().isEmpty()
           && !m_settings.dontAskAgain();
}

QVariantList CompatController::importFiles() const
{
    QVariantList list;
    list.reserve(m_summary.files.size());
    for (const ImportFileSummary &file : m_summary.files) {
        QVariantMap entry;
        entry.insert(QStringLiteral("kind"), file.kind);
        entry.insert(QStringLiteral("relativePath"), file.relativePath);
        entry.insert(QStringLiteral("sizeBytes"), file.sizeBytes);
        entry.insert(QStringLiteral("entryCount"), file.entryCount);
        list.append(entry);
    }
    return list;
}

void CompatController::requestWizard()
{
    m_wizardRequested = true;
    emit stateChanged();
}

QVariantMap CompatController::startupOptions()
{
    m_settings.load();
    m_summary = MpvProfileScanner::scan(m_configDir);

    const QString decision = m_settings.importDecision();
    QVariantMap options;
    if (decision == QLatin1String("full"))
        options = buildImportOptions(allRelativePaths(m_summary));
    else if (decision == QLatin1String("selective"))
        options = buildImportOptions(m_settings.selectedFiles());
    else if (decision == QLatin1String("standalone"))
        options = buildStandaloneOptions();
    // "skip" or undecided: pure EmoPlay defaults — but see below.

    // mpv loads input.conf inside mpv_initialize regardless of --config,
    // falling back to the config dir's input.conf when input-conf is unset
    // or unreadable (input/input.c mp_input_load_config). Whenever the user
    // did not explicitly import their input.conf, pin it to EmoPlay's own
    // empty file so nothing is loaded without consent.
    if (!options.contains(QStringLiteral("input-conf")))
        options.insert(QStringLiteral("input-conf"), ensureOwnInputConf());
    return options;
}

QVariantMap CompatController::buildImportOptions(
    const QStringList &selected) const
{
    QVariantMap options;
    if (!m_summary.isValid() || selected.isEmpty())
        return options;

    const QString sourceDir = m_summary.sourceDir;
    const auto isSelected = [&selected](const QString &relPath) {
        return selected.contains(relPath);
    };

    // mpv.conf: apply through the import rules (forced/strip/redirect).
    if (isSelected(QStringLiteral("mpv.conf"))) {
        QFile conf(sourceDir + QStringLiteral("/mpv.conf"));
        if (conf.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QVector<MpvConfEntry> entries =
                MpvConfigParser::parseMpvConf(
                    QString::fromUtf8(conf.readAll()));
            const ImportRuleResult rules = MpvImportRules::apply(
                entries, sourceDir, m_appDataDir);
            options.insert(rules.options);
            conf.close();
        }
    }

    // input.conf: loaded by mpv directly from the user's directory — the
    // file is never copied or written.
    if (isSelected(QStringLiteral("input.conf"))
        && QFile::exists(sourceDir + QStringLiteral("/input.conf"))) {
        options.insert(QStringLiteral("input-conf"),
                       sourceDir + QStringLiteral("/input.conf"));
    }

    // scripts: explicit file list (files, never a directory overwrite). The
    // --scripts option is a PATH list: ':' on Unix, ';' on Windows (mpv
    // options.rst "--scripts=file1.lua:file2.lua:..."), NOT the ',' used by
    // ordinary list options — a comma would make the whole string one
    // nonexistent path. Paths containing the separator character are
    // unsupported (documented limitation).
    QStringList scriptPaths;
    for (const ImportFileSummary &file : m_summary.files) {
        if (file.kind == QLatin1String("script")
            && isSelected(file.relativePath))
            scriptPaths.append(sourceDir + QLatin1Char('/') + file.relativePath);
    }
    if (!scriptPaths.isEmpty()) {
#if defined(Q_OS_WIN)
        const QLatin1Char scriptSep(';');
#else
        const QLatin1Char scriptSep(':');
#endif
        options.insert(QStringLiteral("scripts"),
                       scriptPaths.join(scriptSep));
    }

    // fonts: point mpv's fonts-dir at the user's fonts/ directory.
    if (m_summary.fontCount > 0) {
        bool anyFontSelected = false;
        for (const ImportFileSummary &file : m_summary.files)
            if (file.kind == QLatin1String("font")
                && isSelected(file.relativePath))
                anyFontSelected = true;
        if (anyFontSelected)
            options.insert(QStringLiteral("fonts-dir"),
                           sourceDir + QStringLiteral("/fonts"));
    }

    // config-dir makes ~~ paths inside imported option values (shaders,
    // ICC profiles, script-opts lookups from scripts) resolve against the
    // user's original directory — read-only usage.
    options.insert(QStringLiteral("config-dir"), sourceDir);

    return options;
}

QVariantMap CompatController::buildStandaloneOptions() const
{
    // Deterministic EmoPlay-only configuration: no user config-dir, no user
    // scripts; input-conf is pinned by startupOptions().
    return {};
}

QString CompatController::ensureOwnInputConf()
{
    const QString path = m_appDataDir + QStringLiteral("/input.conf");
    if (!QFile::exists(path)) {
        // AppDataLocation is not auto-created on a fresh machine; without
        // this the open below fails and mpv would fall back to the user's
        // own input.conf.
        QDir().mkpath(m_appDataDir);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            // input-conf pointing at a missing file makes mpv fall back to
            // the config dir's input.conf — report this instead of failing
            // silently.
            emit errorOccurred(
                QStringLiteral("cannot create %1 (%2): mpv may fall back to "
                               "loading the user's input.conf")
                    .arg(path, file.errorString()));
        }
    }
    return path;
}

void CompatController::persistDecision(const QString &decision,
                                       const QStringList &selected)
{
    m_settings.setImportDecision(decision);
    m_settings.setSelectedFiles(selected);
    m_settings.setLastImportSummary(m_summary.toVariantMap());
    m_settings.save();
    m_wizardRequested = false;
    emit stateChanged();
}

void CompatController::chooseFullImport()
{
    persistDecision(QStringLiteral("full"), allRelativePaths(m_summary));
}

void CompatController::chooseSelectiveImport(const QStringList &relativePaths)
{
    persistDecision(QStringLiteral("selective"), relativePaths);
}

void CompatController::chooseStandalone()
{
    persistDecision(QStringLiteral("standalone"), {});
}

void CompatController::skipAndDontAskAgain()
{
    m_settings.setDontAskAgain(true);
    persistDecision(QStringLiteral("skip"), {});
}
