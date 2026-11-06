#pragma once

#include <QString>

// Maps Qt key events to mpv key names, so key presses that EmoPlay's own QML
// shortcuts do not consume can be fed to mpv's input layer ("keypress"
// command). This is what makes bindings imported from a user's input.conf
// (P4 compat) actually fire in the embedded UI.
//
// Pure logic, unit-testable. Returns an empty string for keys that cannot be
// represented in mpv's key grammar (the caller then simply does not forward).
class MpvKeyName
{
public:
    // |key| is a Qt::Key value, |modifiers| a Qt::KeyboardModifiers mask,
    // |text| the event's text (used for printable punctuation like ">").
    static QString fromQt(int key, int modifiers, const QString &text);
};
