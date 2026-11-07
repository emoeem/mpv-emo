#include "music/LrcParser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

namespace {

// One [mm:ss(.fraction)] timestamp. The fraction is optional and 1-3 digits;
// both [mm:ss.xx] (centiseconds) and [mm:ss.xxx] (milliseconds) occur in the
// wild, and some writers even emit [mm:ss.x].
const QRegularExpression &timeTagPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("\\[(\\d{1,3}):(\\d{1,2})(?:[.:](\\d{1,3}))?\\]"));
    return pattern;
}

// [ti:]/[ar:]/[al:] metadata tags (case-insensitive, like the rest of .lrc).
const QRegularExpression &metadataTagPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^\\[(ti|ar|al):(.*)\\]$"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

double timestampToSeconds(const QRegularExpressionMatch &match)
{
    const int minutes = match.captured(1).toInt();
    const int seconds = match.captured(2).toInt();
    double secondsTotal = minutes * 60.0 + seconds;
    const QString fraction = match.captured(3);
    if (!fraction.isEmpty()) {
        // "5" -> 0.5 s, "25" -> 0.25 s, "250" -> 0.25 s (position digits).
        double scale = 1.0;
        for (int i = 0; i < fraction.size(); ++i)
            scale *= 10.0;
        secondsTotal += fraction.toInt() / scale;
    }
    return secondsTotal;
}

} // namespace

LrcParser::LrcData LrcParser::parse(const QString &content)
{
    LrcData data;

    // QString::fromUtf8 keeps a leading BOM as U+FEFF; drop it so it never
    // lands in the first tag/line. (Some files also carry a UTF-16 BOM
    // decoded to U+FEFF+garbage — that garbage fails every pattern and is
    // skipped like any other unparseable line.)
    QString text = content;
    while (text.startsWith(QChar(0xFEFF)))
        text.remove(0, 1);

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (QString line : lines) {
        // CRLF (and stray CR) endings: trim before matching.
        while (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        if (line.isEmpty())
            continue;

        // Metadata tags are the only lines that parse to nothing after the
        // tag check, so try them first.
        const QRegularExpressionMatch meta = metadataTagPattern().match(line);
        if (meta.hasMatch()) {
            const QString key = meta.captured(1).toLower();
            const QString value = meta.captured(2).trimmed();
            if (key == QLatin1String("ti"))
                data.title = value;
            else if (key == QLatin1String("ar"))
                data.artist = value;
            else if (key == QLatin1String("al"))
                data.album = value;
            continue;
        }

        // Leading timestamps: every match becomes one lyric entry.
        QVector<double> timestamps;
        qsizetype scanFrom = 0;
        for (;;) {
            const QRegularExpressionMatch match =
                timeTagPattern().match(line, scanFrom);
            if (!match.hasMatch() || match.capturedStart() != scanFrom)
                break;
            timestamps.append(timestampToSeconds(match));
            scanFrom = match.capturedEnd();
        }
        if (timestamps.isEmpty())
            continue; // plain text, unknown tag or garbage: not a lyric line

        const QString lyric = line.mid(scanFrom).trimmed();
        for (const double time : timestamps)
            data.lines.append({time, lyric});
    }

    // Files are not required to list lines in playback order.
    std::sort(data.lines.begin(), data.lines.end(),
              [](const LrcLine &a, const LrcLine &b) { return a.time < b.time; });
    return data;
}

LrcParser::LrcData LrcParser::parseFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return parse(QString::fromUtf8(file.readAll()));
}

QStringList LrcParser::lrcCandidates(const QString &audioPath)
{
    if (audioPath.isEmpty())
        return {};
    const QFileInfo info(audioPath);
    const QString stem = info.dir().filePath(info.completeBaseName());
    return {stem + QStringLiteral(".lrc"), stem + QStringLiteral(".LRC")};
}
