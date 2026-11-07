#include "library/ThumbnailService.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

// Kill hung ffmpeg children after this long (network sources, corrupt files).
constexpr int kJobTimeoutMs = 30000;

// The stderr line ffmpeg prints per input: "Duration: HH:MM:SS.mmm".
const QRegularExpression &durationRegex()
{
    static const QRegularExpression re(
        QStringLiteral("Duration:\\s*(\\d+):(\\d+):(\\d+(?:\\.\\d+)?)"));
    return re;
}

} // namespace

ThumbnailService::ThumbnailService(const QString &cacheDir,
                                   const QString &ffmpegBinary,
                                   QObject *parent)
    : QObject(parent), m_cacheDir(cacheDir),
      m_ffmpegBinary(ffmpegBinary.isEmpty() ? QStringLiteral("ffmpeg")
                                            : ffmpegBinary)
{
    // Best effort: if this fails (read-only app data), generation will fail
    // per-file and degrade to the placeholder.
    QDir().mkpath(m_cacheDir);

    m_jobTimeout.setSingleShot(true);
    m_jobTimeout.setInterval(kJobTimeoutMs);
    connect(&m_jobTimeout, &QTimer::timeout, this,
            &ThumbnailService::onJobTimeout);
}

ThumbnailService::~ThumbnailService()
{
    // QProcess children are parented to us and killed on destruction; wait
    // briefly so a running child does not report into a dead object.
    if (m_jobProcess) {
        m_jobProcess->kill();
        m_jobProcess->waitForFinished(500);
    }
}

QString ThumbnailService::cachePathFor(const QString &cacheDir,
                                       const QString &path, qint64 mtimeMs)
{
    const QByteArray digest = QCryptographicHash::hash(
        path.toUtf8() + QByteArray::number(mtimeMs),
        QCryptographicHash::Sha1);
    return cacheDir + QLatin1Char('/') + QString::fromLatin1(digest.toHex())
           + QStringLiteral(".jpg");
}

QString ThumbnailService::seekPositionFor(double durationSeconds)
{
    if (!(durationSeconds > 0.0)) // also covers NaN
        return QStringLiteral("0.000");
    return QString::number(durationSeconds * 0.1, 'f', 3);
}

double ThumbnailService::durationFromFfmpegStderr(const QString &stderrText)
{
    const QRegularExpressionMatch match = durationRegex().match(stderrText);
    if (!match.hasMatch())
        return -1.0;
    const int hours = match.captured(1).toInt();
    const int minutes = match.captured(2).toInt();
    const double seconds = match.captured(3).toDouble();
    return hours * 3600.0 + minutes * 60.0 + seconds;
}

void ThumbnailService::probe()
{
    if (m_probeProcess) // already probing
        return;

    m_probeProcess = new QProcess(this);
    connect(m_probeProcess, &QProcess::finished, this, [this](int exitCode) {
        if (!m_probeProcess)
            return;
        // Real ffmpeg prints e.g. "ffmpeg version 6.1.1 Copyright ..." on
        // stdout; exit 0. Anything else counts as "no usable ffmpeg".
        m_available =
            exitCode == 0
            && QString::fromUtf8(m_probeProcess->readAllStandardOutput())
                   .contains(QStringLiteral("ffmpeg version"));
        m_probeProcess->deleteLater();
        m_probeProcess = nullptr;
        emit availabilityChanged();
    });
    connect(m_probeProcess, &QProcess::errorOccurred, this, [this]() {
        if (!m_probeProcess)
            return;
        m_available = false;
        m_probeProcess->deleteLater();
        m_probeProcess = nullptr;
        emit availabilityChanged();
    });
    m_probeProcess->start(m_ffmpegBinary, {QStringLiteral("-version")});
}

void ThumbnailService::requestThumbnail(const QString &path)
{
    if (path.isEmpty() || m_ready.contains(path))
        return;

    // Existing cache for the file's current mtime: register and report it
    // without any ffmpeg involvement (thumbnails survive ffmpeg going away).
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile())
        return; // nothing to thumbnail (stale library entry)
    const QString cacheFile =
        cachePathFor(m_cacheDir, path, info.lastModified().toMSecsSinceEpoch());
    const QFileInfo cached(cacheFile);
    if (cached.exists() && cached.size() > 0) {
        m_ready.insert(path, cacheFile);
        emit thumbnailReady(path, cacheFile);
        return;
    }

    // Generation needs ffmpeg; without it the request is dropped and the UI
    // keeps its type icons (graceful degradation).
    if (!m_available || m_queue.contains(path) || m_failed.contains(path))
        return;
    m_queue.enqueue(path);
    pump();
}

void ThumbnailService::pump()
{
    if (!m_available || m_jobProcess || m_queue.isEmpty())
        return;
    const QString path = m_queue.takeNext();
    if (path.isEmpty())
        return;
    startDurationProbe(path);
}

void ThumbnailService::startDurationProbe(const QString &path)
{
    m_jobPath = path;
    m_jobStage = JobStage::DurationProbe;
    m_jobCacheFile.clear();
    m_jobProcess = new QProcess(this);
    connect(m_jobProcess, &QProcess::finished, this,
            &ThumbnailService::onJobFinished);
    connect(m_jobProcess, &QProcess::errorOccurred, this,
            &ThumbnailService::onJobError);
    // Duration probe with no output file: real ffmpeg prints the input's
    // "Duration:" line to stderr and exits 1 ("At least one output file must
    // be specified") — that exit code is expected and not an error here.
    m_jobProcess->start(m_ffmpegBinary,
                        {QStringLiteral("-i"), m_jobPath});
    m_jobTimeout.start();
}

void ThumbnailService::startGeneration(const QString &seekPosition)
{
    m_jobStage = JobStage::Generate;
    m_jobCacheFile = cachePathFor(
        m_cacheDir, m_jobPath,
        QFileInfo(m_jobPath).lastModified().toMSecsSinceEpoch());

    // -y: the cache path is fresh for the current mtime, but a previous
    // failed run may have left a partial/empty file; never prompt for it
    // (a prompt would hang the headless QProcess).
    //
    // scale=320:-1 per spec (thumbnail width 320, aspect preserved). Known
    // limitation: for chroma-subsampled sources with odd heights ffmpeg can
    // reject -1 ("height not divisible by 2"); such files degrade to the
    // placeholder. (scale=320:-2 would avoid it; kept -1 per spec.)
    m_jobProcess->start(m_ffmpegBinary,
                        {QStringLiteral("-ss"), seekPosition,
                         QStringLiteral("-i"), m_jobPath,
                         QStringLiteral("-frames:v"), QStringLiteral("1"),
                         QStringLiteral("-vf"), QStringLiteral("scale=320:-1"),
                         QStringLiteral("-y"), m_jobCacheFile});
    m_jobTimeout.start();
}

void ThumbnailService::onJobFinished(int exitCode,
                                     QProcess::ExitStatus exitStatus)
{
    if (!m_jobProcess)
        return;

    const QString stderrText =
        QString::fromUtf8(m_jobProcess->readAllStandardError());

    if (m_jobStage == JobStage::DurationProbe) {
        startGeneration(seekPositionFor(durationFromFfmpegStderr(stderrText)));
        return;
    }

    // Generate stage: success needs exit 0 and a non-empty output file.
    finishJob(exitStatus == QProcess::NormalExit && exitCode == 0
              && QFileInfo(m_jobCacheFile).exists()
              && QFileInfo(m_jobCacheFile).size() > 0);
}

void ThumbnailService::onJobError(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    // FailedToStart / Crashed / Timedout etc. Guarded: whichever of
    // finished/errorOccurred/timeout arrives first finishes the job.
    if (m_jobProcess)
        finishJob(false);
}

void ThumbnailService::onJobTimeout()
{
    if (m_jobProcess)
        m_jobProcess->kill(); // finished(Crashed) -> finishJob(false)
}

void ThumbnailService::finishJob(bool success)
{
    const QString path = m_jobPath;
    const QString cacheFile = m_jobCacheFile;
    m_jobPath.clear();
    m_jobCacheFile.clear();
    m_jobTimeout.stop();
    dropJobProcess();

    if (success) {
        m_ready.insert(path, cacheFile);
        emit thumbnailReady(path, cacheFile);
    } else {
        m_failed.insert(path); // session-level: no retry loops
    }
    pump();
}

void ThumbnailService::dropJobProcess()
{
    if (!m_jobProcess)
        return;
    m_jobProcess->deleteLater();
    m_jobProcess = nullptr;
}
