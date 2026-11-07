#pragma once

#include <QHash>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QString>
#include <QTimer>

#include "library/ThumbnailQueue.h"

// On-demand thumbnail service for the P6 media library (P6 v2).
//
// Probes ffmpeg once (`ffmpeg -version` via QProcess, async). When ffmpeg is
// available, requested library entries get a JPEG thumbnail generated in the
// background, one process chain at a time (serial):
//
//   1. duration probe:   ffmpeg -i <file>            (parses the stderr
//      "Duration: HH:MM:SS.mmm" line; real ffmpeg exits 1 here because no
//      output file is given — that exit code is *not* a failure)
//   2. frame grab:       ffmpeg -ss <10% duration> -i <file> -frames:v 1
//                        -vf scale=320:-1 -y <out.jpg>
//
// The frame is cached at AppData/thumbnails/<sha1(path+mtime)>.jpg so a file
// with unchanged mtime is only ever generated once, and an edited file gets
// a fresh key automatically (stale files are kept, v1 has no cache GC).
//
// Graceful degradation is central: with ffmpeg missing, probe() reports
// available=false, nothing is ever queued or generated, cachedThumbnailPath()
// stays empty and the UI keeps its type icons. Cached files are still
// served when ffmpeg is gone (only generation needs the binary).
//
// Process handling stays event-driven (no waitForFinished) on the owner's
// thread; a 30 s kill timer reaps hung children. requestThumbnail() does the
// cache-lookup bookkeeping, pump() drives the serial queue (ThumbnailQueue
// holds the pure scheduling logic and is unit-tested directly).
class ThumbnailService : public QObject
{
    Q_OBJECT
    // True after the ffmpeg probe succeeded; drives graceful degradation.
    Q_PROPERTY(bool available READ isAvailable NOTIFY availabilityChanged)

public:
    // |cacheDir| receives the generated JPEGs; |ffmpegBinary| is injectable
    // so tests can point at a stub script or a nonexistent path.
    explicit ThumbnailService(const QString &cacheDir,
                              const QString &ffmpegBinary = {},
                              QObject *parent = nullptr);
    ~ThumbnailService() override;

    bool isAvailable() const { return m_available; }

    // Known cache file for |path| (empty when none was generated/registered
    // yet — the model exposes this as the "thumbnail" role).
    QString cachedThumbnailPath(const QString &path) const
    {
        return m_ready.value(path);
    }

    // Start the async `ffmpeg -version` probe. availabilityChanged() is
    // emitted once, when the probe settles (success or failure). Re-invoking
    // while a probe runs is a no-op; a settled probe may be re-run.
    void probe();

    // On-demand request for |path| (an absolute media file path, as stored
    // in the library). Behavior:
    //  - already known ready: no-op (the model already reflects it);
    //  - cache file exists on disk for the current mtime: registered and
    //    thumbnailReady() emitted immediately (works without ffmpeg);
    //  - otherwise queued for serial generation; dropped silently when
    //    ffmpeg is unavailable or the file is missing on disk.
    Q_INVOKABLE void requestThumbnail(const QString &path);

    // Pure cache-key function: |cacheDir|/<sha1(|path|+mtimeMs)>.jpg. The
    // mtime enters the hash so editing a file invalidates its thumbnail.
    static QString cachePathFor(const QString &cacheDir, const QString &path,
                                qint64 mtimeMs);
    // Pure seek computation: 10% of |durationSeconds| formatted for ffmpeg
    // ("-ss" value), clamped at 0 (unknown/invalid durations seek to 0).
    static QString seekPositionFor(double durationSeconds);
    // Pure parser for ffmpeg's stderr "Duration: HH:MM:SS.mmm" line; returns
    // seconds, or -1 when the line is absent/unparseable (e.g. "Duration: N/A").
    static double durationFromFfmpegStderr(const QString &stderrText);

signals:
    void availabilityChanged();
    // |cacheFile| is the generated file; the model maps it back to its rows.
    void thumbnailReady(const QString &path, const QString &cacheFile);

private:
    enum class JobStage
    {
        DurationProbe,
        Generate
    };

    void pump();
    void startDurationProbe(const QString &path);
    void startGeneration(const QString &seekPosition);
    void startJobProcess(const QStringList &arguments);
    void onJobFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onJobError(QProcess::ProcessError error);
    void onJobTimeout();
    // Shared teardown for the active job (guarded: the first of
    // finished/errorOccurred/timeout wins, later callbacks are no-ops).
    void finishJob(bool success);
    void dropJobProcess();
    // Destructor-only: disconnect + kill + reap, so a dying child can never
    // reenter finishJob()/pump() from waitForFinished's event dispatch.
    void teardownProcess(QProcess *process);

    QString m_cacheDir;
    QString m_ffmpegBinary;
    bool m_available = false;

    ThumbnailQueue m_queue;              // waiting paths (FIFO + QSet dedupe)
    QSet<QString> m_failed;              // failed this session, no retries
    QHash<QString, QString> m_ready;     // path -> cache file (known ready)

    QProcess *m_probeProcess = nullptr;  // one-shot availability probe
    QProcess *m_jobProcess = nullptr;    // current serial-chain child
    JobStage m_jobStage = JobStage::DurationProbe;
    QString m_jobPath;
    QString m_jobCacheFile;
    QTimer m_jobTimeout;                 // kills hung children after 30 s
};
