// P7 music mode page. Mounted by Main.qml's Loader while
// musicController.audioMode is true (exposed there as the read-only
// musicMode binding): covers the video surface with a lyrics-centric
// layout. Playback controls stay in the bottom bar (per the P7 v1 scope);
// this page adds the big media-title header, the scrolling lyric list and
// the collapsible 10-band equalizer panel.
//
// All slider/switch values follow the imperative-sync convention
// (docs/architecture.md "QML 状态同步模式"): user edits call controller
// setters, controller updates are pulled back in Connections handlers —
// declarative bindings would be destroyed by the first user drag.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: musicPage

    // Injected by the Main.qml loader (onLoaded: item.mpvItem = mpvItem):
    // a separate file cannot see Main.qml ids, and the header mirrors
    // mpv's media-title from there.
    property var mpvItem: null

    // Slider values snapped back from the controller while a drag is not
    // in progress. var (not list<QtObject>): property list<T> returns a
    // copy on read, so push() would mutate the copy only.
    property var bandSliders: []
    property bool eqPanelVisible: false

    // Short band labels for the 10 fixed frequencies (31.25 Hz .. 16 kHz).
    readonly property var bandLabels: ["31", "62", "125", "250", "500",
                                       "1k", "2k", "4k", "8k", "16k"]

    function syncBandSliders() {
        var gains = musicController.equalizerGains
        for (var i = 0; i < bandSliders.length; ++i)
            if (bandSliders[i] && !bandSliders[i].pressed)
                bandSliders[i].value = gains[i]
    }

    function syncPresetCombo() {
        presetCombo.currentIndex = -1
        for (var i = 0; i < presetCombo.count; ++i) {
            if (presetCombo.valueAt(i) === musicController.equalizerPreset) {
                presetCombo.currentIndex = i
                return
            }
        }
    }

    function subtitleLine() {
        var parts = []
        var lrc = musicController.lrcMetadata
        if (lrc.artist && lrc.artist.length > 0)
            parts.push(lrc.artist)
        if (lrc.album && lrc.album.length > 0)
            parts.push(lrc.album)
        if (parts.length === 0) {
            var meta = musicController.mediaMetadata
            if (meta.artist && meta.artist.length > 0)
                parts.push(meta.artist)
            if (meta.album && meta.album.length > 0)
                parts.push(meta.album)
        }
        return parts.join("  ·  ")
    }

    // Absorb clicks so they never reach the video MouseArea in Main.qml:
    // the music page has no click-to-pause; the bottom bar owns playback.
    MouseArea {
        anchors.fill: parent
        onClicked: function (mouse) { mouse.accepted = true }
    }

    Rectangle {
        anchors.fill: parent
        color: "#101014"
    }

    ColumnLayout {
        anchors.fill: parent
        // The bottom control bar (Main.qml) overlays ~96px; keep clear.
        anchors.bottomMargin: 104
        anchors.margins: 28
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: (musicPage.mpvItem && musicPage.mpvItem.mediaTitle.length > 0)
                  ? musicPage.mpvItem.mediaTitle : qsTr("Music")
            font.pixelSize: 30
            font.weight: Font.DemiBold
            color: "#e8e8ee"
            elide: Text.ElideRight
        }

        Label {
            Layout.fillWidth: true
            visible: musicPage.subtitleLine().length > 0
            text: musicPage.subtitleLine()
            color: "#8a8a96"
            elide: Text.ElideMiddle
        }

        // Lyric list: the controller highlights the current line from
        // time-pos updates; scrolling follows imperatively.
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: lyricsList
                anchors.fill: parent
                clip: true
                model: musicController.lyrics
                currentIndex: -1
                // Leave some breathing room above/below the centered line.
                topMargin: height / 2 - 20
                bottomMargin: height / 2 - 20

                delegate: Item {
                    width: ListView.view.width
                    height: lyricText.implicitHeight + 12

                    Text {
                        id: lyricText
                        anchors.horizontalCenter: parent.horizontalCenter
                        // Timestamp-only (interlude) lines render as a dot.
                        text: modelData.text.length > 0 ? modelData.text : "·"
                        font.pixelSize: index === musicController.lyricIndex ? 22 : 16
                        font.weight: index === musicController.lyricIndex
                                     ? Font.DemiBold : Font.Normal
                        color: index === musicController.lyricIndex
                               ? "#4f8cff" : "#6f6f7a"
                        Behavior on font.pixelSize { NumberAnimation { duration: 120 } }
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }
                }

                Connections {
                    target: musicController
                    function onLyricIndexChanged(index) {
                        lyricsList.currentIndex = index
                        if (index >= 0)
                            lyricsList.positionViewAtIndex(index, ListView.Center)
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: lyricsList.count === 0
                text: qsTr("No lyrics found — put a same-name .lrc file "
                           + "next to the audio file")
                color: "#8a8a96"
            }
        }

        // Collapsible equalizer panel.
        Button {
            id: eqToggle
            flat: true
            hoverEnabled: true
            contentItem: Row {
                spacing: 8
                Text {
                    text: musicPage.eqPanelVisible ? "▾" : "▸"
                    color: "#cfcfd8"
                    font.pixelSize: 12
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: qsTr("Equalizer")
                    color: "#e8e8ee"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            background: Rectangle {
                radius: 6
                color: eqToggle.hovered ? "#33333f" : "transparent"
            }
            onClicked: musicPage.eqPanelVisible = !musicPage.eqPanelVisible
        }

        ColumnLayout {
            id: eqPanel
            visible: musicPage.eqPanelVisible
            Layout.fillWidth: true
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                Switch {
                    id: eqSwitch
                    text: qsTr("Enable")
                    onToggled: musicController.setEqualizerEnabled(checked)

                    Connections {
                        target: musicController
                        function onEqualizerChanged() {
                            eqSwitch.checked = musicController.equalizerEnabled
                        }
                    }
                }

                ComboBox {
                    id: presetCombo
                    textRole: "label"
                    valueRole: "value"
                    enabled: musicController.equalizerEnabled
                    implicitWidth: 160
                    model: [
                        { label: qsTr("Flat"), value: "flat" },
                        { label: qsTr("Bass boost"), value: "bass" },
                        { label: qsTr("Vocal"), value: "vocal" },
                        { label: qsTr("Live"), value: "live" }
                    ]
                    onActivated: function (index) {
                        musicController.applyEqualizerPreset(
                                    presetCombo.valueAt(index))
                    }

                    Connections {
                        target: musicController
                        function onEqualizerChanged() {
                            musicPage.syncPresetCombo()
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: musicController.equalizerEnabled
                          ? qsTr("Applied to mpv's audio filter chain")
                          : qsTr("Disabled — the audio filter chain is "
                                 "left untouched")
                    color: "#8a8a96"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }

            RowLayout {
                id: bandRow
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                spacing: 8

                Repeater {
                    model: musicController.equalizerGains.length

                    delegate: ColumnLayout {
                        id: bandColumn
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 4

                        Slider {
                            id: bandSlider
                            Layout.alignment: Qt.AlignHCenter
                            Layout.fillHeight: true
                            orientation: Qt.Vertical
                            enabled: musicController.equalizerEnabled
                            from: -musicController.maxGain
                            to: musicController.maxGain
                            stepSize: 1
                            snapMode: Slider.SnapAlways
                            onMoved: musicController.setEqualizerBand(index, value)
                            onPressedChanged: if (!pressed)
                                                  musicPage.syncBandSliders()

                            Component.onCompleted: musicPage.bandSliders.push(
                                                       bandSlider)
                        }

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: musicPage.bandLabels[index]
                            color: "#8a8a96"
                            font.pixelSize: 10
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        musicPage.syncBandSliders()
        musicPage.syncPresetCombo()
    }

    Connections {
        target: musicController
        function onEqualizerChanged() {
            musicPage.syncBandSliders()
            musicPage.syncPresetCombo()
        }
    }
}
