import QtQuick
import QtQuick.Controls

// Slim pill slider: 5px track with an accent fill and a handle that grows
// while pressed. All colors come from Theme.
Slider {
    id: control

    implicitHeight: 22

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: 5
        radius: 2.5
        color: Theme.bg3

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 2.5
            color: control.enabled ? Theme.accent : Theme.muted
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.pressed ? 18 : 13
        height: width
        radius: width / 2
        color: control.pressed ? Theme.fg : Theme.accentSoft
        border.color: control.pressed ? Theme.accent : "transparent"
        border.width: 2

        Behavior on width { NumberAnimation { duration: 100 } }
    }
}
