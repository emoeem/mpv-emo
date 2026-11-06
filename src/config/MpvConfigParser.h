#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// Data model for a parsed mpv.conf line.
struct MpvConfEntry
{
    enum class Kind
    {
        Option,        // key=value
        ProfileHeader, // [profile-name]
        Unknown        // anything else (kept for round-trip fidelity)
    };

    Kind kind = Kind::Unknown;
    QString key;         // option name (Option only)
    QString value;       // option value, inline '#' preserved (Option only)
    QString profileName; // profile name (ProfileHeader only)
    int lineNumber = 0;  // 1-based
};

// Data model for a parsed input.conf binding.
struct MpvInputBinding
{
    QString key;        // e.g. "Ctrl+p"
    QStringList command; // tokenized command words, quotes removed
    QString comment;    // trailing comment without the '#'
    int lineNumber = 0; // 1-based
};

// Parser for mpv's native configuration formats. Pure value logic with no
// mpv or GUI dependency, so it is unit-testable in isolation. This backs the
// P4 "mpv compatibility layer": scan ~/.config/mpv, map it into editable
// models, and re-apply it to libmpv at startup.
class MpvConfigParser
{
public:
    // Parse mpv.conf content. mpv.conf has no inline comments: '#' only
    // starts a comment at the beginning of a line, and may appear inside
    // values (e.g. color codes), where it is preserved.
    static QVector<MpvConfEntry> parseMpvConf(const QString &text);

    // Parse input.conf content. Here '#' does start a comment when it is at
    // the start of a line or preceded by whitespace.
    static QVector<MpvInputBinding> parseInputConf(const QString &text);

    // Split a command string into tokens on whitespace, honoring double
    // quotes for tokens containing spaces. An unterminated quote extends to
    // the end of the string (lenient, matches practical usage).
    static QStringList tokenizeCommand(const QString &command);
};
