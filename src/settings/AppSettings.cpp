#include "settings/AppSettings.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

constexpr auto kImportDecision = "importDecision";
constexpr auto kDontAskAgain = "dontAskAgain";
constexpr auto kSelectedFiles = "selectedFiles";
constexpr auto kLastImportSummary = "lastImportSummary";
constexpr auto kImportedAt = "importedAt";
constexpr auto kQuality = "quality";
constexpr auto kQualityContentMode = "contentMode";
constexpr auto kQualityTier = "qualityTier";
constexpr auto kQualityToneMapping = "toneMappingMode";
constexpr auto kQualityScaleOverride = "scaleOverride";
constexpr auto kQualityDebandMode = "debandMode";
constexpr auto kQualityManualShaders = "manualShaderList";
constexpr auto kQualityInterpolation = "interpolation";
constexpr auto kInterpolationEnabled = "enabled";
constexpr auto kInterpolationPreset = "preset";
constexpr auto kQualityIcc = "icc";
constexpr auto kIccEnabled = "enabled";
constexpr auto kIccProfilePath = "profilePath";
constexpr auto kMusic = "music";
constexpr auto kMusicEqualizerEnabled = "equalizerEnabled";
constexpr auto kMusicEqualizerPreset = "equalizerPreset";
constexpr auto kMusicEqualizerGains = "equalizerGains";

} // namespace

QString AppSettings::defaultFilePath()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dir + QStringLiteral("/settings.json");
}

AppSettings::AppSettings(QObject *parent)
    : AppSettings(defaultFilePath(), parent)
{
}

AppSettings::AppSettings(const QString &filePath, QObject *parent)
    : QObject(parent), m_filePath(filePath)
{
}

bool AppSettings::load()
{
    m_importDecision.clear();
    m_dontAskAgain = false;
    m_selectedFiles.clear();
    m_lastImportSummary.clear();
    m_importedAt.clear();
    m_qualityContentMode = QStringLiteral("auto");
    m_qualityTier = QStringLiteral("off");
    m_qualityToneMapping = QStringLiteral("auto");
    m_qualityScaleOverride.clear();
    m_qualityDebandMode = QStringLiteral("auto");
    m_manualShaderList.clear();
    m_qualityInterpolationEnabled = false;
    m_qualityInterpolationPreset = QStringLiteral("MEMC_RIFE_STD");
    m_qualityIccEnabled = false;
    m_qualityIccProfilePath.clear();
    m_musicEqualizerEnabled = false;
    m_musicEqualizerPreset = QStringLiteral("flat");
    m_musicEqualizerGains.clear();

    QFile file(m_filePath);
    if (!file.exists())
        return false;
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject())
        return false; // corrupt: stay at defaults, caller may rewrite

    const QJsonObject obj = doc.object();
    m_importDecision = obj.value(QLatin1String(kImportDecision)).toString();
    m_dontAskAgain = obj.value(QLatin1String(kDontAskAgain)).toBool();
    m_selectedFiles.reserve(obj.value(QLatin1String(kSelectedFiles)).toArray().size());
    for (const QJsonValue &value : obj.value(QLatin1String(kSelectedFiles)).toArray())
        m_selectedFiles.append(value.toString());
    const QJsonObject summary =
        obj.value(QLatin1String(kLastImportSummary)).toObject();
    m_lastImportSummary = summary.toVariantMap();
    m_importedAt = obj.value(QLatin1String(kImportedAt)).toString();

    const QJsonObject quality = obj.value(QLatin1String(kQuality)).toObject();
    m_qualityContentMode =
        quality.value(QLatin1String(kQualityContentMode)).toString(m_qualityContentMode);
    m_qualityTier = quality.value(QLatin1String(kQualityTier)).toString(m_qualityTier);
    m_qualityToneMapping =
        quality.value(QLatin1String(kQualityToneMapping)).toString(m_qualityToneMapping);
    m_qualityScaleOverride =
        quality.value(QLatin1String(kQualityScaleOverride)).toString();
    m_qualityDebandMode =
        quality.value(QLatin1String(kQualityDebandMode)).toString(m_qualityDebandMode);
    const QJsonArray shaders =
        quality.value(QLatin1String(kQualityManualShaders)).toArray();
    m_manualShaderList.reserve(shaders.size());
    for (const QJsonValue &value : shaders)
        m_manualShaderList.append(value.toString());

    const QJsonObject interpolation =
        quality.value(QLatin1String(kQualityInterpolation)).toObject();
    m_qualityInterpolationEnabled =
        interpolation.value(QLatin1String(kInterpolationEnabled))
            .toBool(m_qualityInterpolationEnabled);
    m_qualityInterpolationPreset =
        interpolation.value(QLatin1String(kInterpolationPreset))
            .toString(m_qualityInterpolationPreset);

    const QJsonObject icc =
        quality.value(QLatin1String(kQualityIcc)).toObject();
    m_qualityIccEnabled =
        icc.value(QLatin1String(kIccEnabled)).toBool(m_qualityIccEnabled);
    m_qualityIccProfilePath =
        icc.value(QLatin1String(kIccProfilePath)).toString();

    const QJsonObject music = obj.value(QLatin1String(kMusic)).toObject();
    m_musicEqualizerEnabled =
        music.value(QLatin1String(kMusicEqualizerEnabled)).toBool();
    m_musicEqualizerPreset =
        music.value(QLatin1String(kMusicEqualizerPreset)).toString(m_musicEqualizerPreset);
    const QJsonArray gains =
        music.value(QLatin1String(kMusicEqualizerGains)).toArray();
    for (const QJsonValue &value : gains)
        m_musicEqualizerGains.append(value.toDouble());
    return true;
}

bool AppSettings::save()
{
    QJsonObject quality;
    quality.insert(QLatin1String(kQualityContentMode), m_qualityContentMode);
    quality.insert(QLatin1String(kQualityTier), m_qualityTier);
    quality.insert(QLatin1String(kQualityToneMapping), m_qualityToneMapping);
    quality.insert(QLatin1String(kQualityScaleOverride), m_qualityScaleOverride);
    quality.insert(QLatin1String(kQualityDebandMode), m_qualityDebandMode);
    QJsonArray shaders;
    for (const QString &shader : m_manualShaderList)
        shaders.append(shader);
    quality.insert(QLatin1String(kQualityManualShaders), shaders);

    QJsonObject interpolation;
    interpolation.insert(QLatin1String(kInterpolationEnabled),
                         m_qualityInterpolationEnabled);
    interpolation.insert(QLatin1String(kInterpolationPreset),
                         m_qualityInterpolationPreset);
    quality.insert(QLatin1String(kQualityInterpolation), interpolation);

    QJsonObject icc;
    icc.insert(QLatin1String(kIccEnabled), m_qualityIccEnabled);
    icc.insert(QLatin1String(kIccProfilePath), m_qualityIccProfilePath);
    quality.insert(QLatin1String(kQualityIcc), icc);

    QJsonObject music;
    music.insert(QLatin1String(kMusicEqualizerEnabled), m_musicEqualizerEnabled);
    music.insert(QLatin1String(kMusicEqualizerPreset), m_musicEqualizerPreset);
    QJsonArray gains;
    for (const double gain : m_musicEqualizerGains)
        gains.append(gain);
    music.insert(QLatin1String(kMusicEqualizerGains), gains);

    QJsonObject obj;
    obj.insert(QLatin1String(kImportDecision), m_importDecision);
    obj.insert(QLatin1String(kDontAskAgain), m_dontAskAgain);
    QJsonArray files;
    for (const QString &file : m_selectedFiles)
        files.append(file);
    obj.insert(QLatin1String(kSelectedFiles), files);
    obj.insert(QLatin1String(kLastImportSummary),
               QJsonObject::fromVariantMap(m_lastImportSummary));
    obj.insert(QLatin1String(kImportedAt), m_importedAt);
    obj.insert(QLatin1String(kQuality), quality);
    obj.insert(QLatin1String(kMusic), music);

    // Atomic write via QSaveFile: a crash mid-write leaves the previous
    // settings intact, mirroring HistoryStore.
    const QDir parentDir = QFileInfo(m_filePath).absoluteDir();
    if (!parentDir.exists() && !parentDir.mkpath(QStringLiteral(".")))
        return false;

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return file.commit();
}

void AppSettings::setImportDecision(const QString &decision)
{
    if (m_importDecision == decision)
        return;
    m_importDecision = decision;
    m_importedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    emit changed();
}

void AppSettings::setQualityContentMode(const QString &mode)
{
    if (m_qualityContentMode == mode)
        return;
    m_qualityContentMode = mode;
    emit changed();
}

void AppSettings::setQualityTier(const QString &tier)
{
    if (m_qualityTier == tier)
        return;
    m_qualityTier = tier;
    emit changed();
}

void AppSettings::setQualityToneMapping(const QString &mode)
{
    if (m_qualityToneMapping == mode)
        return;
    m_qualityToneMapping = mode;
    emit changed();
}

void AppSettings::setQualityScaleOverride(const QString &scale)
{
    if (m_qualityScaleOverride == scale)
        return;
    m_qualityScaleOverride = scale;
    emit changed();
}

void AppSettings::setQualityDebandMode(const QString &mode)
{
    if (m_qualityDebandMode == mode)
        return;
    m_qualityDebandMode = mode;
    emit changed();
}

void AppSettings::setManualShaderList(const QStringList &shaders)
{
    if (m_manualShaderList == shaders)
        return;
    m_manualShaderList = shaders;
    emit changed();
}

void AppSettings::setQualityInterpolationEnabled(bool enabled)
{
    if (m_qualityInterpolationEnabled == enabled)
        return;
    m_qualityInterpolationEnabled = enabled;
    emit changed();
}

void AppSettings::setQualityInterpolationPreset(const QString &preset)
{
    if (m_qualityInterpolationPreset == preset)
        return;
    m_qualityInterpolationPreset = preset;
    emit changed();
}

void AppSettings::setQualityIccEnabled(bool enabled)
{
    if (m_qualityIccEnabled == enabled)
        return;
    m_qualityIccEnabled = enabled;
    emit changed();
}

void AppSettings::setQualityIccProfilePath(const QString &path)
{
    if (m_qualityIccProfilePath == path)
        return;
    m_qualityIccProfilePath = path;
    emit changed();
}

void AppSettings::setMusicEqualizerEnabled(bool enabled)
{
    if (m_musicEqualizerEnabled == enabled)
        return;
    m_musicEqualizerEnabled = enabled;
    emit changed();
}

void AppSettings::setMusicEqualizerPreset(const QString &preset)
{
    if (m_musicEqualizerPreset == preset)
        return;
    m_musicEqualizerPreset = preset;
    emit changed();
}

void AppSettings::setMusicEqualizerGains(const QList<double> &gains)
{
    if (m_musicEqualizerGains == gains)
        return;
    m_musicEqualizerGains = gains;
    emit changed();
}
