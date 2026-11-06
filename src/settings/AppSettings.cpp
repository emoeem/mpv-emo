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
    return true;
}

bool AppSettings::save()
{
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
