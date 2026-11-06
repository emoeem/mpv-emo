#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// Persisted application settings (settings.json in AppDataLocation).
//
// Currently carries the P4 mpv-config import decision:
//   - importDecision: "" (unset, first run), "full", "selective",
//     "standalone" or "skip"
//   - selectedFiles:  relative paths chosen for "selective"
//   - dontAskAgain:   user chose "skip and don't ask again"
//   - lastImportSummary: the ImportSummary of the imported configuration
//   - importedAt:     ISO-8601 timestamp of the decision
//
// Storage is a single JSON document written atomically via QSaveFile, mirroring
// HistoryStore. The file path is injectable so tests use a temporary dir and
// QStandardPaths test mode keeps defaultFilePath() off the real app data.
class AppSettings : public QObject
{
    Q_OBJECT

public:
    static QString defaultFilePath();

    explicit AppSettings(QObject *parent = nullptr);
    explicit AppSettings(const QString &filePath, QObject *parent = nullptr);

    // Reads the file. Missing or corrupt files reset to defaults and return
    // false; a valid file returns true. Never throws.
    bool load();
    // Writes the current state atomically. Returns false on I/O failure.
    bool save();

    // "" while undecided (first run or wizard dismissed without a choice).
    QString importDecision() const { return m_importDecision; }
    void setImportDecision(const QString &decision);

    bool dontAskAgain() const { return m_dontAskAgain; }
    void setDontAskAgain(bool dontAskAgain) { m_dontAskAgain = dontAskAgain; }

    const QStringList &selectedFiles() const { return m_selectedFiles; }
    void setSelectedFiles(const QStringList &files) { m_selectedFiles = files; }

    QVariantMap lastImportSummary() const { return m_lastImportSummary; }
    void setLastImportSummary(const QVariantMap &summary)
    {
        m_lastImportSummary = summary;
    }

    QString importedAt() const { return m_importedAt; }

signals:
    void changed();

private:
    QString m_filePath;
    QString m_importDecision;
    bool m_dontAskAgain = false;
    QStringList m_selectedFiles;
    QVariantMap m_lastImportSummary;
    QString m_importedAt;
};
