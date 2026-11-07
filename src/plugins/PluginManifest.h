#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

// One entry of a manager.json-style plugin source manifest. Field semantics
// follow mpv-Emo's manager.json so existing upstream sources can be imported
// with a low migration cost (docs/nativeization-plan.md §4.1):
//
//   name        unique plugin id; characters [A-Za-z0-9_-] (the upstream
//               manager.json mixes "uosc_danmaku", "hdr-toys", "anime4k"),
//               which also keeps it safe to use as a directory name
//   url         upstream git repository (required)
//   branch      tracked branch (default "main")
//   source      subtree inside the upstream repo that holds the plugin
//               content; "." means the whole repository (default ".")
//   destination install target as declared by the manifest, e.g.
//               "scripts/uosc" or "shaders/Anime4K/glsl" (required)
//   mode        "check" (default): update checks only compare the remote
//               HEAD against |baseline| and never touch installed files;
//               "replace": updates re-clone and atomically swap the install
//               (with a backup, and a protective skip on local modifications)
//   baseline    upstream commit/tag the local customizations are based on;
//               empty when the manifest does not declare one
//
// Parsing is pure logic (no I/O, no git) so it can be unit-tested directly.
struct PluginManifest
{
    QString name;
    QString url;
    QString branch;
    QString source;
    QString destination;
    QString mode;
    QString baseline;

    // A manifest that failed validation has valid == false and |error| set;
    // the other fields may then be meaningless.
    bool valid = false;
    QString error;

    bool isReplace() const { return mode == QLatin1String("replace"); }
    // Script plugins install under a scripts/ destination and can be
    // appended to mpv's --scripts path list; shader/asset plugins are only
    // distributed as files (P8 v1 does not mount them automatically).
    bool isScriptPlugin() const;
    // Directory inside an installed plugin clone that holds the plugin
    // content: the manifest |source| subtree (the clone root itself when
    // source is "."). |pluginDir| is the installed clone directory.
    QString contentRoot(const QString &pluginDir) const;

    // Parses one manifest object. Missing required fields (name/url/
    // destination), out-of-domain names, path traversal in source/
    // destination, or a mode other than "check"/"replace" fail validation;
    // absent branch/source/mode/baseline fall back to their defaults.
    static PluginManifest parse(const QJsonObject &object);
    // Parses a manifest document: either the mpv-Emo form
    // {"sources": [ ... ]} or a bare JSON array of entries. Returns an empty
    // vector and sets |error| on malformed input or duplicate names (plugin
    // ids are unique).
    static QVector<PluginManifest> parseList(const QByteArray &json,
                                             QString *error = nullptr);
};
