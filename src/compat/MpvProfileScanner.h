#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

// Per-file result of a compat scan. Relative to the scanned config directory.
struct ImportFileSummary
{
    // One of: "mpv-conf", "input-conf", "profiles-conf", "platform-conf",
    // "script-opt", "script", "shader", "font", "other".
    QString kind;
    QString relativePath; // e.g. "mpv.conf", "scripts/file-browser/main.lua"
    qint64 sizeBytes = 0;
    // Number of parsed config entries (options / bindings / profile entries).
    // Zero for binary or non-config files.
    int entryCount = 0;
};

// Result of scanning a user's mpv configuration directory (P4 compat layer).
// Pure value type: no I/O happens after scan() returns.
struct ImportSummary
{
    QString sourceDir; // absolute config dir; empty when nothing was scanned
    QVector<ImportFileSummary> files;
    int scriptCount = 0;   // scripts/*.lua / *.js (recursive)
    int scriptOptCount = 0; // script-opts/*.conf
    int shaderCount = 0;   // shaders/*.glsl / *.hook (recursive)
    int fontCount = 0;     // fonts/* files
    qint64 totalBytes = 0;

    bool isValid() const { return !sourceDir.isEmpty(); }
    // True when the scan found anything importable at all.
    bool hasContent() const { return !files.isEmpty(); }

    QVariantMap toVariantMap() const;
    static ImportSummary fromVariantMap(const QVariantMap &map);
};

// Locates and scans an existing mpv configuration directory.
//
// Pure logic: everything takes explicit directories, so tests inject a
// temporary tree instead of touching the real HOME (see tests/fixtures).
class MpvProfileScanner
{
public:
    // Platform default location for mpv's config dir, checked in this order:
    //   Linux:   $HOME/.config/mpv, falling back to $XDG_CONFIG_HOME/mpv
    //   Windows: %APPDATA%/mpv
    //   macOS:   $HOME/.config/mpv
    // Returns an empty string when none of the candidates exists.
    static QString locateDefaultConfigDir();

    // Same probe with injected environment values (base-dir parameters, so
    // tests never read or write the real HOME):
    //   homeDir          - e.g. /home/user ($HOME)
    //   xdgConfigHomeDir - e.g. /xdg/config ($XDG_CONFIG_HOME), may be empty
    //   appDataDir       - e.g. C:/Users/user/AppData/Roaming (%APPDATA%),
    //                      may be empty
    // |homeDir| is used when non-empty; on Windows |appDataDir| wins.
    static QString locateConfigDirIn(const QString &homeDir,
                                     const QString &xdgConfigHomeDir,
                                     const QString &appDataDir);

    // Scan a config directory (the "mpv" directory itself). Missing or
    // unreadable directories yield an invalid summary; individual unreadable
    // files are skipped. Known files are parsed with MpvConfigParser to
    // produce entry counts; scripts/, shaders/ and fonts/ are walked
    // recursively and only counted.
    static ImportSummary scan(const QString &configDir);
};
