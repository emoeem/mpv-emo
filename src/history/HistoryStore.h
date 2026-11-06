#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

class MpvEngine;

// Persistent "recently played" history, kept in a JSON file.
//
// Recording: when the engine reports MPV_EVENT_FILE_LOADED the current
// media (path / media-title / duration) is recorded at the top of the
// list. While playback runs, a 5-second timer refreshes the current
// entry's position from the engine's "time-pos" property, so the stored
// value survives an app crash or a forced quit.
//
// Persistence: entries are stored under
//   QStandardPaths::AppDataLocation/history.json
// as {"version": 1, "entries": [...]}; each entry has path, title,
// position, duration and lastPlayedAt (ISO 8601). The list is deduped by
// path (re-play moves the entry to the top) and capped at 50 entries.
// Writes go through QSaveFile for atomic replacement.
//
// Tests can pass an explicit file path and a null engine, which makes the
// whole store runnable headless; the engine-driven paths are guarded
// no-ops when no engine or no media is present.
class HistoryStore : public QObject
{
    Q_OBJECT
    // QVariantList of QVariantMap: path, title, position, duration,
    // lastPlayedAt.
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)

public:
    // |engine| may be null (then nothing is recorded automatically);
    // |filePath| is injected so tests can use a temporary location.
    explicit HistoryStore(MpvEngine *engine, const QString &filePath,
                          QObject *parent = nullptr);

    // Default storage location; requires the application name to be set
    // (QGuiApplication does that before this is called in main()).
    static QString defaultFilePath();

    QVariantList entries() const { return m_entries; }

    // Insert (or move to the top) an entry for |path| and persist.
    // Empty paths are ignored.
    void recordPlayback(const QString &path, const QString &title,
                        double position, double duration);

    // Update only the stored position of the entry for |path|, keeping its
    // order and lastPlayedAt untouched. No-op when the path is unknown.
    void updateEntryPosition(const QString &path, double position);

signals:
    void entriesChanged();

private slots:
    void onFileLoaded();
    void onTimerTick();

private:
    void load();
    void save();

    MpvEngine *m_engine = nullptr;
    QString m_filePath;
    QVariantList m_entries;
    QTimer m_refreshTimer;
    QString m_currentPath;
    bool m_idle = true;

    static constexpr int kMaxEntries = 50;
};
