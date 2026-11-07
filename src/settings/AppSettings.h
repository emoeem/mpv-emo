#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// Persisted application settings (settings.json in AppDataLocation).
//
// Currently carries:
//  - the P4 mpv-config import decision:
//   - importDecision: "" (unset, first run), "full", "selective",
//     "standalone" or "skip"
//   - selectedFiles:  relative paths chosen for "selective"
//   - dontAskAgain:   user chose "skip and don't ask again"
//   - lastImportSummary: the ImportSummary of the imported configuration
//   - importedAt:     ISO-8601 timestamp of the decision
//  - the P5 quality preferences in a nested "quality" section:
//   - contentMode ("auto"/"anime"/"live", default "auto")
//   - qualityTier ("off"/"low"/"medium"/"high", default "off")
//   - toneMappingMode ("auto" or an explicit mpv tone-mapping value,
//     default "auto")
//   - scaleOverride ("" = engine picks, or an explicit --scale value)
//   - debandMode ("auto"/"yes"/"no" user override, default "auto")
//   - manualShaderList (absolute shader paths checked in the quality panel)
//  - the P7 music preferences in a nested "music" section:
//   - equalizerEnabled (10-band EQ wired to mpv's af chain, default false)
//   - equalizerPreset ("flat"/"bass"/"vocal"/"live"/"custom", default "flat")
//   - equalizerGains (per-band gains in dB, up to 10 doubles, default all 0)
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

    // P5 quality section. All setters emit changed() and persist only when
    // the caller calls save() (same contract as the import decision).
    QString qualityContentMode() const { return m_qualityContentMode; }
    void setQualityContentMode(const QString &mode);
    QString qualityTier() const { return m_qualityTier; }
    void setQualityTier(const QString &tier);
    QString qualityToneMapping() const { return m_qualityToneMapping; }
    void setQualityToneMapping(const QString &mode);
    QString qualityScaleOverride() const { return m_qualityScaleOverride; }
    void setQualityScaleOverride(const QString &scale);
    QString qualityDebandMode() const { return m_qualityDebandMode; }
    void setQualityDebandMode(const QString &mode);
    const QStringList &manualShaderList() const { return m_manualShaderList; }
    void setManualShaderList(const QStringList &shaders);

    // P7 music section. Same contract as the quality section: setters emit
    // changed() and persist only when the caller calls save().
    bool musicEqualizerEnabled() const { return m_musicEqualizerEnabled; }
    void setMusicEqualizerEnabled(bool enabled);
    QString musicEqualizerPreset() const { return m_musicEqualizerPreset; }
    void setMusicEqualizerPreset(const QString &preset);
    const QList<double> &musicEqualizerGains() const
    {
        return m_musicEqualizerGains;
    }
    void setMusicEqualizerGains(const QList<double> &gains);

signals:
    void changed();

private:
    QString m_filePath;
    QString m_importDecision;
    bool m_dontAskAgain = false;
    QStringList m_selectedFiles;
    QVariantMap m_lastImportSummary;
    QString m_importedAt;
    QString m_qualityContentMode = QStringLiteral("auto");
    QString m_qualityTier = QStringLiteral("off");
    QString m_qualityToneMapping = QStringLiteral("auto");
    QString m_qualityScaleOverride;
    QString m_qualityDebandMode = QStringLiteral("auto");
    QStringList m_manualShaderList;
    bool m_musicEqualizerEnabled = false;
    QString m_musicEqualizerPreset = QStringLiteral("flat");
    QList<double> m_musicEqualizerGains;
};
