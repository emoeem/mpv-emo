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
constexpr auto kQualityManualShaders = "manualShaderList";

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
    m_manualShaderList.clear();

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
    const QJsonArray shaders =
        quality.value(QLatin1String(kQualityManualShaders)).toArray();
    m_manualShaderList.reserve(shaders.size());
    for (const QJsonValue &value : shaders)
        m_manualShaderList.append(value.toString());
    return true;
}

bool AppSettings::save()
{
    QJsonObject quality;
    quality.insert(QLatin1String(kQualityContentMode), m_qualityContentMode);
    quality.insert(QLatin1String(kQualityTier), m_qualityTier);
    quality.insert(QLatin1String(kQualityToneMapping), m_qualityToneMapping);
    quality.insert(QLatin1String(kQualityScaleOverride), m_qualityScaleOverride);
    QJsonArray shaders;
    for (const QString &shader : m_manualShaderList)
        shaders.append(shader);
    quality.insert(QLatin1String(kQualityManualShaders), shaders);

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

void AppSettings::setManualShaderList(const QStringList &shaders)
{
    if (m_manualShaderList == shaders)
        return;
    m_manualShaderList = shaders;
    emit changed();
}
