import QtQuick
import QtQuick.Shapes

// Vector line icons drawn with QtQuick.Shapes — no icon font, no emoji,
// consistent 2px rounded strokes at any size. Usage:
//   AppIcon { name: "play"; size: 20; color: Theme.fg }
// Supported names: play, pause, stop, prev, next, list, gear, quality,
// library, plugins, volume, mute, fullscreen, fullscreenExit, close,
// folder, camera, subtitles.
Item {
    id: root

    property string name: ""
    property color color: "#f8f8f2"
    property real size: 20

    implicitWidth: size
    implicitHeight: size

    // All shapes draw into a 24x24 box scaled to `size`.
    Shape {
        id: canvas
        anchors.fill: parent
        anchors.margins: root.size * 0.04
        layer.enabled: true
        layer.smooth: true

        readonly property real u: width / 24

        // ---- play (filled triangle) ----
        Shape {
            visible: root.name === "play"
            anchors.fill: parent
            ShapePath {
                fillColor: root.color
                strokeColor: "transparent"
                strokeWidth: -1
                startX: 8 * canvas.u; startY: 5 * canvas.u
                PathLine { x: 19 * canvas.u; y: 12 * canvas.u }
                PathLine { x: 8 * canvas.u; y: 19 * canvas.u }
                PathLine { x: 8 * canvas.u; y: 5 * canvas.u }
            }
        }

        // ---- pause (two bars) ----
        Shape {
            visible: root.name === "pause"
            anchors.fill: parent
            ShapePath {
                fillColor: root.color
                strokeColor: "transparent"
                strokeWidth: -1
                startX: 7 * canvas.u; startY: 5 * canvas.u
                PathLine { x: 10.4 * canvas.u; y: 5 * canvas.u }
                PathLine { x: 10.4 * canvas.u; y: 19 * canvas.u }
                PathLine { x: 7 * canvas.u; y: 19 * canvas.u }
                PathLine { x: 7 * canvas.u; y: 5 * canvas.u }
            }
            ShapePath {
                fillColor: root.color
                strokeColor: "transparent"
                strokeWidth: -1
                startX: 13.6 * canvas.u; startY: 5 * canvas.u
                PathLine { x: 17 * canvas.u; y: 5 * canvas.u }
                PathLine { x: 17 * canvas.u; y: 19 * canvas.u }
                PathLine { x: 13.6 * canvas.u; y: 19 * canvas.u }
                PathLine { x: 13.6 * canvas.u; y: 5 * canvas.u }
            }
        }

        // ---- stop (rounded square) ----
        Rectangle {
            visible: root.name === "stop"
            anchors.centerIn: parent
            width: root.size * 0.5
            height: width
            radius: 3
            color: root.color
        }

        // ---- prev (bar + left triangle) ----
        Shape {
            visible: root.name === "prev"
            anchors.fill: parent
            ShapePath {
                fillColor: root.color
                strokeColor: "transparent"
                strokeWidth: -1
                startX: 6 * canvas.u; startY: 5 * canvas.u
                PathLine { x: 8.6 * canvas.u; y: 5 * canvas.u }
                PathLine { x: 8.6 * canvas.u; y: 19 * canvas.u }
                PathLine { x: 6 * canvas.u; y: 19 * canvas.u }
                PathLine { x: 6 * canvas.u; y: 5 * canvas.u }
            }
            ShapePath {
                fillColor: root.color
                strokeColor: "transparent"
                strokeWidth: -1
                startX: 18 * canvas.u; startY: 5 * canvas.u
                PathLine { x: 9.5 * canvas.u; y: 12 * canvas.u }
                PathLine { x: 18 * canvas.u; y: 19 * canvas.u }
                PathLine { x: 18 * canvas.u; y: 5 * canvas.u }
            }
        }

        // ---- next (right triangle + bar) ----
        Shape {
            visible: root.name === "next"
            anchors.fill: parent
            ShapePath {
                fillColor: root.color
                strokeColor: "transparent"
                strokeWidth: -1
                startX: 15.4 * canvas.u; startY: 5 * canvas.u
                PathLine { x: 18 * canvas.u; y: 5 * canvas.u }
                PathLine { x: 18 * canvas.u; y: 19 * canvas.u }
                PathLine { x: 15.4 * canvas.u; y: 19 * canvas.u }
                PathLine { x: 15.4 * canvas.u; y: 5 * canvas.u }
            }
            ShapePath {
                fillColor: root.color
                strokeColor: "transparent"
                strokeWidth: -1
                startX: 6 * canvas.u; startY: 5 * canvas.u
                PathLine { x: 14.5 * canvas.u; y: 12 * canvas.u }
                PathLine { x: 6 * canvas.u; y: 19 * canvas.u }
                PathLine { x: 6 * canvas.u; y: 5 * canvas.u }
            }
        }

        // ---- list (three stroked lines) ----
        Shape {
            visible: root.name === "list"
            anchors.fill: parent
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                fillColor: "transparent"
                startX: 4 * canvas.u; startY: 7 * canvas.u
                PathLine { x: 20 * canvas.u; y: 7 * canvas.u }
            }
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                fillColor: "transparent"
                startX: 4 * canvas.u; startY: 12 * canvas.u
                PathLine { x: 20 * canvas.u; y: 12 * canvas.u }
            }
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                fillColor: "transparent"
                startX: 4 * canvas.u; startY: 17 * canvas.u
                PathLine { x: 20 * canvas.u; y: 17 * canvas.u }
            }
        }

        // ---- gear (stroke circle + 8 ticks) ----
        Shape {
            visible: root.name === "gear"
            anchors.fill: parent
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                fillColor: "transparent"
                startX: 12 * canvas.u; startY: 5 * canvas.u
                PathArc { x: 12 * canvas.u; y: 19 * canvas.u
                          radiusX: 7 * canvas.u; radiusY: 7 * canvas.u; useLargeArc: true }
                PathArc { x: 12 * canvas.u; y: 5 * canvas.u
                          radiusX: 7 * canvas.u; radiusY: 7 * canvas.u; useLargeArc: true }
            }
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                fillColor: "transparent"
                startX: 12 * canvas.u; startY: 2.5 * canvas.u
                PathLine { x: 12 * canvas.u; y: 5.5 * canvas.u }
            }
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                fillColor: "transparent"
                startX: 12 * canvas.u; startY: 18.5 * canvas.u
                PathLine { x: 12 * canvas.u; y: 21.5 * canvas.u }
            }
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                fillColor: "transparent"
                startX: 2.5 * canvas.u; startY: 12 * canvas.u
                PathLine { x: 5.5 * canvas.u; y: 12 * canvas.u }
            }
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                fillColor: "transparent"
                startX: 18.5 * canvas.u; startY: 12 * canvas.u
                PathLine { x: 21.5 * canvas.u; y: 12 * canvas.u }
            }
        }

        // ---- quality (four-point star, filled) ----
        Shape {
            visible: root.name === "quality"
            anchors.fill: parent
            ShapePath {
                fillColor: root.color
                strokeColor: "transparent"
                strokeWidth: -1
                startX: 12 * canvas.u; startY: 3 * canvas.u
                PathLine { x: 14.6 * canvas.u; y: 9.4 * canvas.u }
                PathLine { x: 21 * canvas.u; y: 12 * canvas.u }
                PathLine { x: 14.6 * canvas.u; y: 14.6 * canvas.u }
                PathLine { x: 12 * canvas.u; y: 21 * canvas.u }
                PathLine { x: 9.4 * canvas.u; y: 14.6 * canvas.u }
                PathLine { x: 3 * canvas.u; y: 12 * canvas.u }
                PathLine { x: 9.4 * canvas.u; y: 9.4 * canvas.u }
                PathLine { x: 12 * canvas.u; y: 3 * canvas.u }
            }
        }

        // ---- library (three vertical bars) ----
        Shape {
            visible: root.name === "library"
            anchors.fill: parent
            ShapePath {
                fillColor: root.color
                strokeColor: "transparent"
                strokeWidth: -1
                startX: 5 * canvas.u; startY: 4 * canvas.u
                PathLine { x: 9 * canvas.u; y: 4 * canvas.u }
                PathLine { x: 9 * canvas.u; y: 20 * canvas.u }
                PathLine { x: 5 * canvas.u; y: 20 * canvas.u }
                PathLine { x: 5 * canvas.u; y: 4 * canvas.u }
            }
            ShapePath {
                fillColor: root.color
                strokeColor: "transparent"
                strokeWidth: -1
                startX: 11.5 * canvas.u; startY: 4 * canvas.u
                PathLine { x: 15.5 * canvas.u; y: 4 * canvas.u }
                PathLine { x: 15.5 * canvas.u; y: 20 * canvas.u }
                PathLine { x: 11.5 * canvas.u; y: 20 * canvas.u }
                PathLine { x: 11.5 * canvas.u; y: 4 * canvas.u }
            }
            ShapePath {
                fillColor: root.color
                strokeColor: "transparent"
                strokeWidth: -1
                startX: 17.5 * canvas.u; startY: 5.5 * canvas.u
                PathLine { x: 20.5 * canvas.u; y: 6 * canvas.u }
                PathLine { x: 18.8 * canvas.u; y: 20 * canvas.u }
                PathLine { x: 16 * canvas.u; y: 19.6 * canvas.u }
                PathLine { x: 17.5 * canvas.u; y: 5.5 * canvas.u }
            }
        }

        // ---- plugins (2x2 grid) ----
        Shape {
            visible: root.name === "plugins"
            anchors.fill: parent
            ShapePath {
                fillColor: root.color
                strokeColor: "transparent"
                strokeWidth: -1
                startX: 4 * canvas.u; startY: 4 * canvas.u
                PathLine { x: 11 * canvas.u; y: 4 * canvas.u }
                PathLine { x: 11 * canvas.u; y: 11 * canvas.u }
                PathLine { x: 4 * canvas.u; y: 11 * canvas.u }
                PathLine { x: 4 * canvas.u; y: 4 * canvas.u }
            }
            ShapePath {
                fillColor: root.color
                strokeColor: "transparent"
                strokeWidth: -1
                startX: 13 * canvas.u; startY: 4 * canvas.u
                PathLine { x: 20 * canvas.u; y: 4 * canvas.u }
                PathLine { x: 20 * canvas.u; y: 11 * canvas.u }
                PathLine { x: 13 * canvas.u; y: 11 * canvas.u }
                PathLine { x: 13 * canvas.u; y: 4 * canvas.u }
            }
            ShapePath {
                fillColor: root.color
                strokeColor: "transparent"
                strokeWidth: -1
                startX: 4 * canvas.u; startY: 13 * canvas.u
                PathLine { x: 11 * canvas.u; y: 13 * canvas.u }
                PathLine { x: 11 * canvas.u; y: 20 * canvas.u }
                PathLine { x: 4 * canvas.u; y: 20 * canvas.u }
                PathLine { x: 4 * canvas.u; y: 13 * canvas.u }
            }
            ShapePath {
                fillColor: root.color
                strokeColor: "transparent"
                strokeWidth: -1
                startX: 13 * canvas.u; startY: 13 * canvas.u
                PathLine { x: 20 * canvas.u; y: 13 * canvas.u }
                PathLine { x: 20 * canvas.u; y: 20 * canvas.u }
                PathLine { x: 13 * canvas.u; y: 20 * canvas.u }
                PathLine { x: 13 * canvas.u; y: 13 * canvas.u }
            }
        }

        // ---- volume (speaker + waves) ----
        Shape {
            visible: root.name === "volume"
            anchors.fill: parent
            ShapePath {
                fillColor: root.color
                strokeColor: "transparent"
                strokeWidth: -1
                startX: 4 * canvas.u; startY: 9 * canvas.u
                PathLine { x: 8 * canvas.u; y: 9 * canvas.u }
                PathLine { x: 13 * canvas.u; y: 5 * canvas.u }
                PathLine { x: 13 * canvas.u; y: 19 * canvas.u }
                PathLine { x: 8 * canvas.u; y: 15 * canvas.u }
                PathLine { x: 4 * canvas.u; y: 15 * canvas.u }
                PathLine { x: 4 * canvas.u; y: 9 * canvas.u }
            }
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                fillColor: "transparent"
                startX: 16 * canvas.u; startY: 9 * canvas.u
                PathArc { x: 16 * canvas.u; y: 15 * canvas.u
                          radiusX: 4.5 * canvas.u; radiusY: 4.5 * canvas.u; useLargeArc: false }
            }
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                fillColor: "transparent"
                startX: 18.5 * canvas.u; startY: 7 * canvas.u
                PathArc { x: 18.5 * canvas.u; y: 17 * canvas.u
                          radiusX: 7.5 * canvas.u; radiusY: 7.5 * canvas.u; useLargeArc: false }
            }
        }

        // ---- mute (speaker + cross) ----
        Shape {
            visible: root.name === "mute"
            anchors.fill: parent
            ShapePath {
                fillColor: root.color
                strokeColor: "transparent"
                strokeWidth: -1
                startX: 4 * canvas.u; startY: 9 * canvas.u
                PathLine { x: 8 * canvas.u; y: 9 * canvas.u }
                PathLine { x: 13 * canvas.u; y: 5 * canvas.u }
                PathLine { x: 13 * canvas.u; y: 19 * canvas.u }
                PathLine { x: 8 * canvas.u; y: 15 * canvas.u }
                PathLine { x: 4 * canvas.u; y: 15 * canvas.u }
                PathLine { x: 4 * canvas.u; y: 9 * canvas.u }
            }
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                fillColor: "transparent"
                startX: 16 * canvas.u; startY: 9.5 * canvas.u
                PathLine { x: 21 * canvas.u; y: 14.5 * canvas.u }
            }
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                fillColor: "transparent"
                startX: 21 * canvas.u; startY: 9.5 * canvas.u
                PathLine { x: 16 * canvas.u; y: 14.5 * canvas.u }
            }
        }

        // ---- close (X) ----
        Shape {
            visible: root.name === "close"
            anchors.fill: parent
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                fillColor: "transparent"
                startX: 6 * canvas.u; startY: 6 * canvas.u
                PathLine { x: 18 * canvas.u; y: 18 * canvas.u }
            }
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                fillColor: "transparent"
                startX: 18 * canvas.u; startY: 6 * canvas.u
                PathLine { x: 6 * canvas.u; y: 18 * canvas.u }
            }
        }

        // ---- folder (filled) ----
        Shape {
            visible: root.name === "folder"
            anchors.fill: parent
            ShapePath {
                fillColor: root.color
                strokeColor: "transparent"
                strokeWidth: -1
                startX: 3.5 * canvas.u; startY: 6 * canvas.u
                PathLine { x: 10 * canvas.u; y: 6 * canvas.u }
                PathLine { x: 12 * canvas.u; y: 8.5 * canvas.u }
                PathLine { x: 20.5 * canvas.u; y: 8.5 * canvas.u }
                PathLine { x: 20.5 * canvas.u; y: 18.5 * canvas.u }
                PathLine { x: 3.5 * canvas.u; y: 18.5 * canvas.u }
                PathLine { x: 3.5 * canvas.u; y: 6 * canvas.u }
            }
        }

        // ---- subtitles (stroked rect + two lines) ----
        Shape {
            visible: root.name === "subtitles"
            anchors.fill: parent
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                fillColor: "transparent"
                startX: 3.5 * canvas.u; startY: 5 * canvas.u
                PathLine { x: 20.5 * canvas.u; y: 5 * canvas.u }
                PathLine { x: 20.5 * canvas.u; y: 19 * canvas.u }
                PathLine { x: 3.5 * canvas.u; y: 19 * canvas.u }
                PathLine { x: 3.5 * canvas.u; y: 5 * canvas.u }
            }
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                fillColor: "transparent"
                startX: 6.5 * canvas.u; startY: 14.5 * canvas.u
                PathLine { x: 13 * canvas.u; y: 14.5 * canvas.u }
            }
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                fillColor: "transparent"
                startX: 15 * canvas.u; startY: 14.5 * canvas.u
                PathLine { x: 17.5 * canvas.u; y: 14.5 * canvas.u }
            }
        }

        // ---- fullscreen (four corner brackets, stroked) ----
        Shape {
            visible: root.name === "fullscreen"
            anchors.fill: parent
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                fillColor: "transparent"
                startX: 4 * canvas.u; startY: 9 * canvas.u
                PathLine { x: 4 * canvas.u; y: 4 * canvas.u }
                PathLine { x: 9 * canvas.u; y: 4 * canvas.u }
            }
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                fillColor: "transparent"
                startX: 15 * canvas.u; startY: 4 * canvas.u
                PathLine { x: 20 * canvas.u; y: 4 * canvas.u }
                PathLine { x: 20 * canvas.u; y: 9 * canvas.u }
            }
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                fillColor: "transparent"
                startX: 20 * canvas.u; startY: 15 * canvas.u
                PathLine { x: 20 * canvas.u; y: 20 * canvas.u }
                PathLine { x: 15 * canvas.u; y: 20 * canvas.u }
            }
            ShapePath {
                strokeColor: root.color
                strokeWidth: 2 * canvas.u
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                fillColor: "transparent"
                startX: 9 * canvas.u; startY: 20 * canvas.u
                PathLine { x: 4 * canvas.u; y: 20 * canvas.u }
                PathLine { x: 4 * canvas.u; y: 15 * canvas.u }
            }
        }
    }
}
