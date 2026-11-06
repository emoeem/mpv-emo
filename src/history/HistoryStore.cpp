#include "history/HistoryStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>

#include "mpv/MpvEngine.h"

HistoryStore::HistoryStore(MpvEngine *engine, const QString &filePath,
                           QObject *parent)
    : QObject(parent), m_engine(engine), m_filePath(filePath)
{
    load();

    if (m_engine) {
        connect(m_engine, &MpvEngine::fileLoaded, this,
                &HistoryStore::onFileLoaded);
        connect(m_engine, &MpvEngine::idleActiveChanged, this,
                [this](bool idle) { m_idle = idle; });
        // Playback state authority is mpv; pick up where it stands right now
        // (the observed-property events for past changes are not replayed).
        const QVariant idle = m_engine->getProperty(QStringLiteral("idle-active"));
        if (idle.isValid())
            m_idle = idle.toBool();
    }

    m_refreshTimer.setInterval(5000);
    connect(&m_refreshTimer, &QTimer::timeout, this, &HistoryStore::onTimerTick);
    m_refreshTimer.start();
}

QString HistoryStore::defaultFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/history.json");
}

void HistoryStore::recordPlayback(const QString &path, const QString &title,
                                  double position, double duration)
{
    if (path.isEmpty())
        return;

    QVariantMap entry;
    entry.insert(QStringLiteral("path"), path);
    entry.insert(QStringLiteral("title"), title);
    entry.insert(QStringLiteral("position"), position);
    entry.insert(QStringLiteral("duration"), duration);
    entry.insert(QStringLiteral("lastPlayedAt"),
                 QDateTime::currentDateTime().toString(Qt::ISODate));

    // Dedupe by path: the fresh entry replaces the old one and moves to top.
    for (int i = 0; i < m_entries.size();) {
        if (m_entries.at(i).toMap().value(QStringLiteral("path")).toString() == path)
            m_entries.removeAt(i);
        else
            ++i;
    }
    m_entries.prepend(entry);
    while (m_entries.size() > kMaxEntries)
        m_entries.removeLast();

    emit entriesChanged();
    save();
}

void HistoryStore::updateEntryPosition(const QString &path, double position)
{
    if (path.isEmpty())
        return;

    for (int i = 0; i < m_entries.size(); ++i) {
        QVariantMap entry = m_entries.at(i).toMap();
        if (entry.value(QStringLiteral("path")).toString() != path)
            continue;
        // Paused playback reports the same position every tick; skip the
        // disk write in that case.
        if (entry.value(QStringLiteral("position")).toDouble() == position)
            return;
        entry.insert(QStringLiteral("position"), position);
        m_entries.replace(i, entry);
        emit entriesChanged();
        save();
        return;
    }
}

void HistoryStore::onFileLoaded()
{
    if (!m_engine)
        return;

    const QString path = m_engine->getProperty(QStringLiteral("path")).toString();
    if (path.isEmpty())
        return; // no media behind the event; nothing to record

    m_currentPath = path;
    const QString title = m_engine->getProperty(QStringLiteral("media-title")).toString();
    const QVariant duration = m_engine->getProperty(QStringLiteral("duration"));
    recordPlayback(path, title, 0.0, duration.isValid() ? duration.toDouble() : 0.0);
}

void HistoryStore::onTimerTick()
{
    if (!m_engine || m_idle || m_currentPath.isEmpty())
        return;

    const QVariant position = m_engine->getProperty(QStringLiteral("time-pos"));
    if (!position.isValid())
        return; // property unavailable (e.g. between files)
    updateEntryPosition(m_currentPath, position.toDouble());
}

void HistoryStore::load()
{
    m_entries.clear();

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return; // no history yet

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return; // corrupt file: start with an empty history

    QJsonArray array;
    if (doc.isArray())
        array = doc.array();
    else if (doc.isObject())
        array = doc.object().value(QStringLiteral("entries")).toArray();

    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        QVariantMap entry;
        entry.insert(QStringLiteral("path"), object.value(QStringLiteral("path")).toString());
        entry.insert(QStringLiteral("title"), object.value(QStringLiteral("title")).toString());
        entry.insert(QStringLiteral("position"), object.value(QStringLiteral("position")).toDouble());
        entry.insert(QStringLiteral("duration"), object.value(QStringLiteral("duration")).toDouble());
        entry.insert(QStringLiteral("lastPlayedAt"),
                     object.value(QStringLiteral("lastPlayedAt")).toString());
        if (entry.value(QStringLiteral("path")).toString().isEmpty())
            continue;
        m_entries.append(entry);
    }
}

void HistoryStore::save()
{
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return;

    QJsonArray array;
    for (const QVariant &value : m_entries)
        array.append(QJsonObject::fromVariantMap(value.toMap()));

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("entries"), array);
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.commit(); // atomic replace; a crash mid-write keeps the old file
}
