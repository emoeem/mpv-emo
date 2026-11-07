pragma Singleton
import QtQuick

// Design tokens for the Dracula-derived dark theme. Single source of truth:
// components reference Theme.* instead of hardcoding colors, so the whole
// look can be retuned in one place.
QtObject {
    // Surfaces
    readonly property color bg0: "#191a21"   // window background
    readonly property color bg1: "#21222c"   // floating surfaces (bar, drawers)
    readonly property color bg2: "#282a36"   // raised elements (cards, hover)
    readonly property color bg3: "#343746"   // pressed / strongest hover

    // Text
    readonly property color fg: "#f8f8f2"
    readonly property color fgDim: "#bdbdc8"
    readonly property color muted: "#6272a4"

    // Accents
    readonly property color accent: "#bd93f9"
    readonly property color accentSoft: "#d6b9ff"
    readonly property color accent2: "#ff79c6"
    readonly property color green: "#50fa7b"
    readonly property color orange: "#ffb86c"
    readonly property color red: "#ff5555"
    readonly property color cyan: "#8be9fd"

    // Lines and translucency
    readonly property color border: "#1fffffff"
    readonly property color barTint: "#e621222c"

    // Metrics
    readonly property int radiusS: 8
    readonly property int radiusM: 12
    readonly property int radiusL: 18
    readonly property int barHeight: 84
    readonly property int barMargin: 18

    // Motion
    readonly property int animFast: 120
    readonly property int animNormal: 220
    readonly property int animSlow: 300
}
