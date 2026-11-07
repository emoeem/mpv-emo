#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// One timed lyric line ("[mm:ss.xx] text" in .lrc terms). Multi-timestamp
// lines expand into one entry per timestamp.
struct LrcLine
{
    double time = 0.0; // seconds from the start of playback
    QString text;
};

// Parses .lrc lyric files (P7 music mode). Pure logic: no mpv, no GUI; the
// only I/O is the file reader in parseFile(). Unit-testable like the other
// config parsers (docs/architecture.md decision 6).
//
// Supported syntax (verified against common .lrc usage):
//   - "[mm:ss]" / "[mm:ss.xx]" / "[mm:ss.xxx]" timestamps (1-3 fraction
//     digits; centiseconds and milliseconds both map to seconds)
//   - multiple timestamps per line ("[00:12.00][00:45.50] text"), each
//     becoming one entry with the same text
//   - metadata tags [ti:...] (title), [ar:...] (artist), [al:...] (album)
//   - UTF-8 BOM, CRLF and LF line endings
// Unknown bracketed tags ([by:], [offset:], ...) are skipped: they are not
// shown as lyric text. Lines without any timestamp are ignored.
class LrcParser
{
public:
    struct LrcData
    {
        QString title;
        QString artist;
        QString album;
        QVector<LrcLine> lines; // sorted by time
    };

    // Parses raw .lrc text; see the class comment for the grammar.
    static LrcData parse(const QString &content);
    // Reads |path| as UTF-8 and parses it. Missing/unreadable files return
    // an empty LrcData (lyrics are optional decoration, never an error).
    static LrcData parseFile(const QString &path);

    // "Same name next to the audio file" matching rule: audio at
    // <dir>/<stem>.<ext> pairs with <dir>/<stem>.lrc (and .LRC). Returns the
    // candidates in probe order; the caller picks the first that exists.
    static QStringList lrcCandidates(const QString &audioPath);
};
