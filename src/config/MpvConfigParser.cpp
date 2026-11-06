#include "config/MpvConfigParser.h"

namespace {

QStringList splitLines(const QString &text)
{
    QStringList lines = text.split(QLatin1Char('\n'));
    for (QString &line : lines)
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
    return lines;
}

} // namespace

QVector<MpvConfEntry> MpvConfigParser::parseMpvConf(const QString &text)
{
    QVector<MpvConfEntry> entries;
    const QStringList lines = splitLines(text);

    for (int i = 0; i < lines.size(); ++i) {
        const QString trimmed = lines.at(i).trimmed();

        MpvConfEntry entry;
        entry.lineNumber = i + 1;

        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
            continue;

        if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
            entry.kind = MpvConfEntry::Kind::ProfileHeader;
            entry.profileName = trimmed.mid(1, trimmed.size() - 2);
            entries.append(entry);
            continue;
        }

        const qsizetype eq = trimmed.indexOf(QLatin1Char('='));
        if (eq > 0) {
            entry.kind = MpvConfEntry::Kind::Option;
            entry.key = trimmed.left(eq).trimmed();
            // Inline '#' is part of the value: mpv.conf has no inline comments.
            entry.value = trimmed.mid(eq + 1).trimmed();
        }
        entries.append(entry);
    }
    return entries;
}

QVector<MpvInputBinding> MpvConfigParser::parseInputConf(const QString &text)
{
    QVector<MpvInputBinding> bindings;
    const QStringList lines = splitLines(text);

    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines.at(i);
        const QString trimmed = line.trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
            continue;

        // Find a '#' that starts a real comment: position 0 of the trimmed
        // line was handled above, so look for '# ' preceded by whitespace.
        qsizetype commentStart = -1;
        for (qsizetype pos = trimmed.indexOf(QLatin1Char('#')); pos >= 0;
             pos = trimmed.indexOf(QLatin1Char('#'), pos + 1)) {
            if (pos == 0)
                continue;
            if (trimmed.at(pos - 1).isSpace()) {
                commentStart = pos;
                break;
            }
        }

        QString code = trimmed;
        QString comment;
        if (commentStart >= 0) {
            code = trimmed.left(commentStart).trimmed();
            comment = trimmed.mid(commentStart + 1).trimmed();
        }

        const qsizetype firstSpace = code.indexOf(QLatin1Char(' '));
        if (firstSpace < 0)
            continue; // "key" alone has no command: not a usable binding

        MpvInputBinding binding;
        binding.lineNumber = i + 1;
        binding.key = code.left(firstSpace);
        binding.command = tokenizeCommand(code.mid(firstSpace + 1).trimmed());
        binding.comment = comment;

        if (binding.command.isEmpty())
            continue;
        bindings.append(binding);
    }
    return bindings;
}

QStringList MpvConfigParser::tokenizeCommand(const QString &command)
{
    QStringList tokens;
    QString current;
    bool inQuotes = false;

    for (const QChar ch : command) {
        if (ch == QLatin1Char('"')) {
            inQuotes = !inQuotes;
        } else if (ch.isSpace() && !inQuotes) {
            if (!current.isEmpty()) {
                tokens.append(current);
                current.clear();
            }
        } else {
            current.append(ch);
        }
    }
    if (!current.isEmpty())
        tokens.append(current);
    return tokens;
}
