#include "compat/MpvProfileScanner.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include "config/MpvConfigParser.h"

namespace {

constexpr auto kMpvConf = "mpv.conf";
constexpr auto kInputConf = "input.conf";
constexpr auto kProfilesConf = "profiles.conf";
constexpr auto kPlatformConf = "platform.conf";

// Known top-level config files, mapped to their ImportFileSummary kind.
// profiles.conf uses the same [section] + key=value syntax as mpv.conf (it is
// just an include-style split of the profile definitions), so one parser
// covers both; see architecture.md "配置兼容层".
ImportFileSummary summaryForConfigFile(const QString &absolutePath,
                                       const QString &relativePath,
                                       const QString &kind)
{
    ImportFileSummary summary;
    summary.kind = kind;
    summary.relativePath = relativePath;

    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return summary;

    const QString text = QString::fromUtf8(file.readAll());
    summary.sizeBytes = file.size();
    file.close();

    if (kind == QLatin1String("input-conf")) {
        summary.entryCount =
            static_cast<int>(MpvConfigParser::parseInputConf(text).size());
    } else {
        // mpv.conf / profiles.conf / platform.conf share the option syntax.
        summary.entryCount =
            static_cast<int>(MpvConfigParser::parseMpvConf(text).size());
    }
    return summary;
}

// Recursively collect files under |subDirName| whose suffix is in |suffixes|
// (empty list = all files). Returns relative paths sorted for stable output.
QStringList walkFiles(const QString &configDir, const QString &subDirName,
                      const QStringList &suffixes)
{
    QStringList result;
    const QDir subDir(configDir + QLatin1Char('/') + subDirName);
    if (!subDir.exists())
        return result;

    QDirIterator it(subDir.absolutePath(),
                    QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        if (!suffixes.isEmpty()
            && !suffixes.contains(info.suffix().toLower()))
            continue;
        result.append(subDirName + QLatin1Char('/') + info.filePath().mid(subDir.absolutePath().length() + 1));
    }
    result.sort();
    return result;
}

} // namespace

QVariantMap ImportSummary::toVariantMap() const
{
    QVariantMap map;
    map.insert(QStringLiteral("sourceDir"), sourceDir);
    map.insert(QStringLiteral("scriptCount"), scriptCount);
    map.insert(QStringLiteral("scriptOptCount"), scriptOptCount);
    map.insert(QStringLiteral("shaderCount"), shaderCount);
    map.insert(QStringLiteral("fontCount"), fontCount);
    map.insert(QStringLiteral("totalBytes"), totalBytes);
    QVariantList fileList;
    fileList.reserve(files.size());
    for (const ImportFileSummary &file : files) {
        QVariantMap fileMap;
        fileMap.insert(QStringLiteral("kind"), file.kind);
        fileMap.insert(QStringLiteral("relativePath"), file.relativePath);
        fileMap.insert(QStringLiteral("sizeBytes"), file.sizeBytes);
        fileMap.insert(QStringLiteral("entryCount"), file.entryCount);
        fileList.append(fileMap);
    }
    map.insert(QStringLiteral("files"), fileList);
    return map;
}

ImportSummary ImportSummary::fromVariantMap(const QVariantMap &map)
{
    ImportSummary summary;
    summary.sourceDir = map.value(QStringLiteral("sourceDir")).toString();
    summary.scriptCount =
        map.value(QStringLiteral("scriptCount")).toInt();
    summary.scriptOptCount =
        map.value(QStringLiteral("scriptOptCount")).toInt();
    summary.shaderCount = map.value(QStringLiteral("shaderCount")).toInt();
    summary.fontCount = map.value(QStringLiteral("fontCount")).toInt();
    summary.totalBytes =
        map.value(QStringLiteral("totalBytes")).toLongLong();
    const QVariantList fileList = map.value(QStringLiteral("files")).toList();
    for (const QVariant &entry : fileList) {
        const QVariantMap fileMap = entry.toMap();
        ImportFileSummary file;
        file.kind = fileMap.value(QStringLiteral("kind")).toString();
        file.relativePath =
            fileMap.value(QStringLiteral("relativePath")).toString();
        file.sizeBytes = fileMap.value(QStringLiteral("sizeBytes")).toLongLong();
        file.entryCount = fileMap.value(QStringLiteral("entryCount")).toInt();
        summary.files.append(file);
    }
    return summary;
}

QString MpvProfileScanner::locateDefaultConfigDir()
{
    return locateConfigDirIn(QDir::homePath(),
                             qEnvironmentVariable("XDG_CONFIG_HOME"),
                             qEnvironmentVariable("APPDATA"));
}

QString MpvProfileScanner::locateConfigDirIn(const QString &homeDir,
                                             const QString &xdgConfigHomeDir,
                                             const QString &appDataDir)
{
    QStringList candidates;
#if defined(Q_OS_WIN)
    // Windows: mpv reads %APPDATA%\mpv (or portable_config next to mpv.exe,
    // which is out of scope for a scan-based import).
    if (!appDataDir.isEmpty())
        candidates.append(appDataDir + QLatin1String("/mpv"));
#elif defined(Q_OS_MACOS)
    if (!homeDir.isEmpty())
        candidates.append(homeDir + QLatin1String("/.config/mpv"));
#else
    // Linux/BSD: ~/.config/mpv first; XDG_CONFIG_HOME is only consulted as a
    // fallback (the ask specifies this order), even though it usually points
    // at the same directory.
    if (!homeDir.isEmpty())
        candidates.append(homeDir + QLatin1String("/.config/mpv"));
    if (!xdgConfigHomeDir.isEmpty())
        candidates.append(xdgConfigHomeDir + QLatin1String("/mpv"));
#endif

    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    return QString();
}

ImportSummary MpvProfileScanner::scan(const QString &configDir)
{
    ImportSummary summary;
    const QDir dir(configDir);
    if (configDir.isEmpty() || !dir.exists())
        return summary;

    summary.sourceDir = dir.absolutePath();

    // Top-level config files (each present = one entry in the file list).
    const QFileInfo mpvConf(dir.filePath(QLatin1String(kMpvConf)));
    if (mpvConf.isFile())
        summary.files.append(summaryForConfigFile(
            mpvConf.absoluteFilePath(), QLatin1String(kMpvConf),
            QStringLiteral("mpv-conf")));
    const QFileInfo inputConf(dir.filePath(QLatin1String(kInputConf)));
    if (inputConf.isFile())
        summary.files.append(summaryForConfigFile(
            inputConf.absoluteFilePath(), QLatin1String(kInputConf),
            QStringLiteral("input-conf")));
    const QFileInfo profilesConf(dir.filePath(QLatin1String(kProfilesConf)));
    if (profilesConf.isFile())
        summary.files.append(summaryForConfigFile(
            profilesConf.absoluteFilePath(), QLatin1String(kProfilesConf),
            QStringLiteral("profiles-conf")));
    const QFileInfo platformConf(dir.filePath(QLatin1String(kPlatformConf)));
    if (platformConf.isFile())
        summary.files.append(summaryForConfigFile(
            platformConf.absoluteFilePath(), QLatin1String(kPlatformConf),
            QStringLiteral("platform-conf")));

    // script-opts/: per-script option files.
    const QStringList scriptOpts =
        walkFiles(summary.sourceDir, QStringLiteral("script-opts"),
                  QStringList{QStringLiteral("conf")});
    for (const QString &relPath : scriptOpts) {
        summary.files.append(summaryForConfigFile(
            summary.sourceDir + QLatin1Char('/') + relPath, relPath,
            QStringLiteral("script-opt")));
        ++summary.scriptOptCount;
    }

    // scripts/, shaders/, fonts/: counted, not parsed.
    const QStringList scripts = walkFiles(
        summary.sourceDir, QStringLiteral("scripts"),
        QStringList{QStringLiteral("lua"), QStringLiteral("js")});
    for (const QString &relPath : scripts) {
        ImportFileSummary file;
        file.kind = QStringLiteral("script");
        file.relativePath = relPath;
        const QFileInfo info(summary.sourceDir + QLatin1Char('/') + relPath);
        file.sizeBytes = info.size();
        summary.files.append(file);
        ++summary.scriptCount;
    }
    // A scripts/ directory can hold data files next to scripts (e.g. module
    // assets); surface them so the wizard shows what would come along.
    const QStringList scriptOthers = walkFiles(
        summary.sourceDir, QStringLiteral("scripts"), QStringList{});
    for (const QString &relPath : scriptOthers) {
        const QString suffix =
            QFileInfo(relPath).suffix().toLower();
        if (suffix == QLatin1String("lua") || suffix == QLatin1String("js"))
            continue;
        ImportFileSummary file;
        file.kind = QStringLiteral("other");
        file.relativePath = relPath;
        file.sizeBytes = QFileInfo(summary.sourceDir + QLatin1Char('/')
                                   + relPath).size();
        summary.files.append(file);
    }

    const QStringList shaders = walkFiles(
        summary.sourceDir, QStringLiteral("shaders"),
        QStringList{QStringLiteral("glsl"), QStringLiteral("hook")});
    for (const QString &relPath : shaders) {
        ImportFileSummary file;
        file.kind = QStringLiteral("shader");
        file.relativePath = relPath;
        file.sizeBytes =
            QFileInfo(summary.sourceDir + QLatin1Char('/') + relPath).size();
        summary.files.append(file);
        ++summary.shaderCount;
    }

    const QStringList fonts = walkFiles(summary.sourceDir,
                                        QStringLiteral("fonts"), QStringList{});
    for (const QString &relPath : fonts) {
        ImportFileSummary file;
        file.kind = QStringLiteral("font");
        file.relativePath = relPath;
        file.sizeBytes =
            QFileInfo(summary.sourceDir + QLatin1Char('/') + relPath).size();
        summary.files.append(file);
        ++summary.fontCount;
    }

    for (const ImportFileSummary &file : summary.files)
        summary.totalBytes += file.sizeBytes;

    return summary;
}
