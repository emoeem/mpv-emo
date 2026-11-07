import QtQuick
import QtQuick.Controls

// Round accent-filled button for the primary action (play/pause). The icon
// is an AppIcon glyph on a colored disc that scales slightly when pressed.
Button {
    id: control

    property string glyph: "play"
    property real diameter: 46

    implicitWidth: diameter
    implicitHeight: diameter
    flat: true
    hoverEnabled: true

    contentItem: AppIcon {
        anchors.centerIn: parent
        name: control.glyph
        size: control.diameter * 0.42
        color: "#191a21"
    }

    background: Rectangle {
        radius: control.diameter / 2
        color: control.hovered ? Theme.accentSoft : Theme.accent
        scale: control.pressed ? 0.92 : 1

        Behavior on color { ColorAnimation { duration: Theme.animFast } }
        Behavior on scale { NumberAnimation { duration: 100 } }
    }

    ToolTip.visible: hovered && ToolTip.text.length > 0
    ToolTip.delay: 600
}
