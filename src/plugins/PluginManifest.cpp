#include "plugins/PluginManifest.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace {

// Plugin ids become directory names under <AppData>/plugins/, so the charset
// is deliberately narrow. This mirrors the ids actually used by mpv-Emo's
// manager.json (uosc, uosc_danmaku, anime4k, file_browser, hdr-toys,
// trakt_scrobble) while excluding path separators and dots (no traversal,
// no hidden directories).
const QRegularExpression &namePattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^[A-Za-z0-9_-]+$"));
    return pattern;
}

// Relative paths (manifest source/destination) must stay inside the tree
// they are applied to: absolute paths, ".." segments and backslashes are
// rejected.
bool isSafeRelativePath(const QString &path)
{
    if (path.isEmpty() || path.startsWith(QLatin1Char('/'))
        || path.startsWith(QLatin1Char('\\'))
        || path.contains(QLatin1Char('\\')))
        return false;
    const QStringList segments = path.split(QLatin1Char('/'));
    for (const QString &segment : segments) {
        if (segment.isEmpty() || segment == QLatin1String(".")
            || segment == QLatin1String(".."))
            return false;
    }
    return true;
}

// Reads a string field. Returns false when the key is present but is not a
// JSON string (type confusion would silently produce "" otherwise).
bool readString(const QJsonObject &object, const QString &key,
                QString *value, QString *error)
{
    const QJsonValue json = object.value(key);
    if (json.isUndefined()) {
        value->clear();
        return true;
    }
    if (!json.isString()) {
        *error = QStringLiteral("field \"%1\" must be a string").arg(key);
        return false;
    }
    *value = json.toString();
    return true;
}

} // namespace

bool PluginManifest::isScriptPlugin() const
{
    // manager.json destinations are either "scripts/<name>" (or a single
    // file "scripts/<name>.lua") for script plugins, or "shaders/<...>" for
    // shader packs. Only the first kind feeds mpv's --scripts option.
    const QString prefix = QStringLiteral("scripts/");
    return destination.startsWith(prefix);
}

QString PluginManifest::contentRoot(const QString &pluginDir) const
{
    if (source == QLatin1String("."))
        return pluginDir;
    return pluginDir + QLatin1Char('/') + source;
}

PluginManifest PluginManifest::parse(const QJsonObject &object)
{
    PluginManifest manifest;
    manifest.valid = false;

    QString error;
    QString name;
    QString url;
    QString destination;
    QString branch;
    QString source;
    QString mode;
    QString baseline;

    if (!readString(object, QStringLiteral("name"), &name, &error)
        || !readString(object, QStringLiteral("url"), &url, &error)
        || !readString(object, QStringLiteral("destination"), &destination,
                       &error)
        || !readString(object, QStringLiteral("branch"), &branch, &error)
        || !readString(object, QStringLiteral("source"), &source, &error)
        || !readString(object, QStringLiteral("mode"), &mode, &error)
        || !readString(object, QStringLiteral("baseline"), &baseline, &error)) {
        manifest.error = error;
        return manifest;
    }

    if (name.isEmpty()) {
        manifest.error = QStringLiteral("missing required field \"name\"");
        return manifest;
    }
    if (!namePattern().match(name).hasMatch()) {
        manifest.error =
            QStringLiteral("invalid plugin name \"%1\" (allowed: A-Z a-z 0-9 _ -)")
                .arg(name);
        return manifest;
    }
    if (url.isEmpty()) {
        manifest.error =
            QStringLiteral("missing required field \"url\" for \"%1\"").arg(name);
        return manifest;
    }
    if (destination.isEmpty()) {
        manifest.error = QStringLiteral("missing required field \"destination\" "
                                        "for \"%1\"")
                             .arg(name);
        return manifest;
    }
    if (!isSafeRelativePath(destination)) {
        manifest.error = QStringLiteral("unsafe destination path \"%1\" for \"%2\"")
                             .arg(destination, name);
        return manifest;
    }
    if (!source.isEmpty() && source != QLatin1String(".")
        && !isSafeRelativePath(source)) {
        manifest.error = QStringLiteral("unsafe source path \"%1\" for \"%2\"")
                             .arg(source, name);
        return manifest;
    }
    // mode: "check" (default) or "replace"; anything else is rejected so a
    // typo can never silently pick the destructive semantics.
    if (!mode.isEmpty() && mode != QLatin1String("check")
        && mode != QLatin1String("replace")) {
        manifest.error = QStringLiteral("invalid mode \"%1\" for \"%2\" "
                                        "(expected \"check\" or \"replace\")")
                             .arg(mode, name);
        return manifest;
    }

    manifest.name = name;
    manifest.url = url;
    manifest.destination = destination;
    manifest.branch = branch.isEmpty() ? QStringLiteral("main") : branch;
    manifest.source =
        source.isEmpty() ? QStringLiteral(".") : source;
    manifest.mode =
        mode.isEmpty() ? QStringLiteral("check") : mode;
    manifest.baseline = baseline;
    manifest.valid = true;
    return manifest;
}

QVector<PluginManifest> PluginManifest::parseList(const QByteArray &json,
                                                  QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return QVector<PluginManifest>();
    };

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return fail(QStringLiteral("manifest is not valid JSON: %1")
                        .arg(parseError.errorString()));
    if (!doc.isObject() && !doc.isArray())
        return fail(QStringLiteral("manifest must be an object or an array"));

    QJsonArray entries;
    if (doc.isObject()) {
        // mpv-Emo manager.json form: {"sources": [ ... ]}.
        const QJsonValue sources = doc.object().value(QStringLiteral("sources"));
        if (!sources.isArray())
            return fail(QStringLiteral("manifest object lacks a \"sources\" array"));
        entries = sources.toArray();
    } else {
        entries = doc.array();
    }

    QVector<PluginManifest> manifests;
    manifests.reserve(entries.size());
    QStringList seenNames;
    for (int i = 0; i < entries.size(); ++i) {
        if (!entries.at(i).isObject())
            return fail(QStringLiteral("manifest entry %1 is not an object").arg(i));
        const PluginManifest manifest =
            PluginManifest::parse(entries.at(i).toObject());
        if (!manifest.valid) {
            return fail(QStringLiteral("manifest entry %1: %2")
                            .arg(QString::number(i), manifest.error));
        }
        if (seenNames.contains(manifest.name)) {
            return fail(QStringLiteral("duplicate plugin name \"%1\"")
                            .arg(manifest.name));
        }
        seenNames.append(manifest.name);
        manifests.append(manifest);
    }
    if (error)
        error->clear();
    return manifests;
}
