#include "compat/MpvKeyName.h"

#include <QChar>
#include <Qt>

namespace {

// Qt::Key -> mpv base key name (without modifiers). Empty for unmappable.
QString baseKeyName(int key)
{
    switch (key) {
    case Qt::Key_Escape: return QLatin1String("ESC");
    case Qt::Key_Tab: return QLatin1String("TAB");
    case Qt::Key_Backtab: return QLatin1String("TAB");
    case Qt::Key_Backspace: return QLatin1String("BS");
    case Qt::Key_Return: return QLatin1String("ENTER");
    case Qt::Key_Enter: return QLatin1String("ENTER");
    case Qt::Key_Insert: return QLatin1String("INS");
    case Qt::Key_Delete: return QLatin1String("DEL");
    case Qt::Key_Home: return QLatin1String("HOME");
    case Qt::Key_End: return QLatin1String("END");
    case Qt::Key_Left: return QLatin1String("left");
    case Qt::Key_Up: return QLatin1String("up");
    case Qt::Key_Right: return QLatin1String("right");
    case Qt::Key_Down: return QLatin1String("down");
    case Qt::Key_PageUp: return QLatin1String("PGUP");
    case Qt::Key_PageDown: return QLatin1String("PGDWN");
    case Qt::Key_Space: return QLatin1String("SPACE");
    default: break;
    }

    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return QStringLiteral("f%1").arg(key - Qt::Key_F1 + 1);

    // Letters (Qt::Key_A..Z are ASCII 'A'..'Z') and digits ('0'..'9') map to
    // their literal character, matching mpv's grammar.
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return QString(QChar::fromLatin1(char(key - Qt::Key_A + 'a')));
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return QString(QChar::fromLatin1(char(key)));

    // Punctuation that can appear under Ctrl in imported bindings.
    switch (key) {
    case Qt::Key_Comma: return QLatin1String(",");
    case Qt::Key_Period: return QLatin1String(".");
    case Qt::Key_Slash: return QLatin1String("/");
    case Qt::Key_Semicolon: return QLatin1String(";");
    case Qt::Key_Apostrophe: return QLatin1String("'");
    case Qt::Key_BracketLeft: return QLatin1String("[");
    case Qt::Key_BracketRight: return QLatin1String("]");
    case Qt::Key_Minus: return QLatin1String("-");
    case Qt::Key_Equal: return QLatin1String("=");
    case Qt::Key_Backslash: return QLatin1String("\\");
    case Qt::Key_QuoteLeft: return QLatin1String("`");
    default: break;
    }
    return QString();
}

QString withModifiers(const QString &base, bool ctrl, bool alt, bool meta,
                      bool shift)
{
    QString name;
    if (ctrl) name += QStringLiteral("ctrl+");
    if (alt) name += QStringLiteral("alt+");
    if (meta) name += QStringLiteral("meta+");
    if (shift) name += QStringLiteral("shift+");
    return name + base;
}

} // namespace

QString MpvKeyName::fromQt(int key, int modifiers, const QString &text)
{
    const bool ctrl = (modifiers & Qt::ControlModifier) != 0;
    const bool alt = (modifiers & Qt::AltModifier) != 0;
    const bool meta = (modifiers & Qt::MetaModifier) != 0;
    const bool shift = (modifiers & Qt::ShiftModifier) != 0;

    // Plain key with printable text: prefer the text so punctuation keeps its
    // literal form (">", ";", "0", "a", ...). Space is a named mpv key.
    if (!ctrl && !alt && !meta) {
        if (!shift && text.size() == 1) {
            const QChar ch = text.at(0);
            if (ch.isPrint() && !ch.isSpace())
                return text;
        }
        const QString base = baseKeyName(key);
        if (base.isEmpty())
            return QString();
        return withModifiers(base, false, false, false, shift);
    }

    // Modified key: build "ctrl+alt+meta+shift+<base>" from the key code; the
    // event text is a control character in this case and must not be used.
    const QString base = baseKeyName(key);
    if (base.isEmpty())
        return QString();
    return withModifiers(base, ctrl, alt, meta, shift);
}
