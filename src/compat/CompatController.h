#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include "compat/MpvProfileScanner.h"
#include "settings/AppSettings.h"

// Glue between the P4 compat layer and the app: persists the import decision,
// scans the user's mpv directory, and assembles the option map that
// MpvEngine::initialize() applies before mpv_initialize().
//
// Startup order (see main.cpp):
//   1. read settings.json (import decision)
//   2. scan the user's mpv config dir
//   3. build options for the recorded decision (full/selective/standalone)
//   4. MpvEngine::initialize(options) — forced values re-asserted inside
//   5. first run with a detected config: the QML wizard asks; the choice
//      persists and the imported options take effect on the NEXT launch.
//
// User principles enforced here and in MpvImportRules:
//   - EmoPlay never writes into the user's mpv directory (config-dir points
//     there only for reading; all write paths are redirected to app data).
//   - Any decision is user-initiated; nothing is imported without consent.
//     mpv would otherwise silently load the user's input.conf (mpv loads it
//     in mpv_initialize regardless of --config) and auto-load every script
//     in the config dir's scripts/ subdirectory (load-scripts defaults to
//     yes even under libmpv). Therefore load-scripts is forced off in
//     MpvEngine and input-conf ALWAYS points somewhere we control: at the
//     user's original file only when it was explicitly imported, otherwise
//     at EmoPlay's own empty input.conf in app data.
class CompatController : public QObject
{
    Q_OBJECT

    // True when a user mpv config directory with importable content exists.
    Q_PROPERTY(bool configDetected READ configDetected NOTIFY stateChanged)
    // First run (no decision recorded), config found, not told to be quiet.
    Q_PROPERTY(bool firstRunPromptNeeded READ firstRunPromptNeeded
                   NOTIFY stateChanged)
    // Set by requestWizard() until the wizard reports a choice.
    Q_PROPERTY(bool wizardRequested READ wizardRequested NOTIFY stateChanged)
    Q_PROPERTY(QString sourceDir READ sourceDir NOTIFY stateChanged)
    // File entries for the selective-import checklist: kind, relativePath,
    // sizeBytes, entryCount.
    Q_PROPERTY(QVariantList importFiles READ importFiles NOTIFY stateChanged)

public:
    // Uses AppSettings::defaultFilePath() and platform default locations.
    explicit CompatController(QObject *parent = nullptr);
    // Injectable paths for tests: settings file, app data dir (receives
    // redirected write paths) and the mpv config dir to scan.
    CompatController(const QString &settingsFilePath,
                     const QString &appDataDir, const QString &configDir,
                     QObject *parent = nullptr);

    // Steps 1-3 above. Safe to call once at startup.
    QVariantMap startupOptions();

    bool configDetected() const { return m_summary.hasContent(); }
    bool firstRunPromptNeeded() const;
    bool wizardRequested() const { return m_wizardRequested; }
    QString sourceDir() const { return m_summary.sourceDir; }
    QVariantList importFiles() const;
    // Shared settings instance: other services (P5 quality controller)
    // persist their preferences into the same settings.json document.
    AppSettings &settings() { return m_settings; }

    // Wizard outcomes. Each persists the decision immediately and clears
    // wizardRequested. Imported engine options apply on the next launch.
    Q_INVOKABLE void chooseFullImport();
    Q_INVOKABLE void chooseSelectiveImport(const QStringList &relativePaths);
    Q_INVOKABLE void chooseStandalone();
    Q_INVOKABLE void skipAndDontAskAgain();
    // "Re-run import" entry point (settings UI in a later phase).
    Q_INVOKABLE void requestWizard();

signals:
    void stateChanged();
    // Non-fatal setup problems that would otherwise be silent (e.g. the
    // fallback input.conf could not be created and mpv might fall back to
    // the user's own input.conf). Logged by main.cpp.
    void errorOccurred(const QString &message);

private:
    // Options for importing exactly |selected| files (relative paths).
    QVariantMap buildImportOptions(const QStringList &selected) const;
    // Options for the "standalone" choice: deterministic EmoPlay-only config,
    // no user config-dir, no user scripts (input-conf pinned by
    // startupOptions).
    QVariantMap buildStandaloneOptions() const;
    // Returns the path of EmoPlay's own empty input.conf, creating it if
    // needed; emits errorOccurred when creation fails. Non-const: it writes
    // the file and may emit.
    QString ensureOwnInputConf();
    void persistDecision(const QString &decision, const QStringList &selected);

    AppSettings m_settings;
    QString m_appDataDir;
    QString m_configDir;
    ImportSummary m_summary;
    bool m_wizardRequested = false;
};
