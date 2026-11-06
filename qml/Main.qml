import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs
import EmoPlay.Mpv

ApplicationWindow {
    id: root

    readonly property bool isFullscreen: visibility === Window.FullScreen
    readonly property double currentTime: seekSlider.pressed
                                          ? seekSlider.value : mpvItem.position

    function formatTime(seconds) {
        if (!isFinite(seconds) || seconds < 0)
            seconds = 0
        var h = Math.floor(seconds / 3600)
        var m = Math.floor((seconds % 3600) / 60)
        var s = Math.floor(seconds % 60)
        var mm = (h > 0 && m < 10) ? "0" + m : "" + m
        var ss = (s < 10) ? "0" + s : "" + s
        return (h > 0 ? h + ":" : "") + mm + ":" + ss
    }

    // Human-readable label for a track-list entry, e.g. "Title [en] ".
    function trackLabel(track, fallback) {
        var name = (track.title && track.title.length > 0)
                   ? track.title : fallback + " " + track.id
        if (track.lang && track.lang.length > 0)
            name += "  [" + track.lang + "]"
        return name
    }

    function showControlsTemporarily() {
        controls.opacity = 1
        hideTimer.restart()
    }

    function toggleFullscreen() {
        if (root.isFullscreen)
            root.showNormal()
        else
            root.showFullScreen()
    }

    width: 1280
    height: 720
    minimumWidth: 960
    minimumHeight: 540
    visible: true
    title: mpvItem.mediaTitle.length > 0 ? mpvItem.mediaTitle : qsTr("EmoPlay")
    color: "#0d0d11"

    palette.window: "#0d0d11"
    palette.windowText: "#e8e8ee"
    palette.base: "#17171d"
    palette.button: "#22222b"
    palette.buttonText: "#e8e8ee"
    palette.text: "#e8e8ee"
    palette.highlight: "#4f8cff"
    palette.highlightedText: "#ffffff"

    component IconButton: Button {
        id: iconButton
        property string glyph: ""
        flat: true
        hoverEnabled: true
        leftPadding: 10
        rightPadding: 10
        contentItem: Text {
            text: iconButton.glyph
            font.pixelSize: 16
            color: iconButton.hovered ? "#ffffff" : "#cfcfd8"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 6
            color: iconButton.hovered ? "#33333f" : "transparent"
        }
        ToolTip.visible: hovered && ToolTip.text.length > 0
        ToolTip.delay: 600
    }

    component FullscreenIcon: Item {
        implicitWidth: 16
        implicitHeight: 16
        property color ink: "#cfcfd8"
        Rectangle { x: 0; y: 0; width: 7; height: 2; color: ink }
        Rectangle { x: 0; y: 0; width: 2; height: 7; color: ink }
        Rectangle { x: 9; y: 0; width: 7; height: 2; color: ink }
        Rectangle { x: 14; y: 0; width: 2; height: 7; color: ink }
        Rectangle { x: 0; y: 14; width: 7; height: 2; color: ink }
        Rectangle { x: 0; y: 9; width: 2; height: 7; color: ink }
        Rectangle { x: 9; y: 14; width: 7; height: 2; color: ink }
        Rectangle { x: 14; y: 9; width: 2; height: 7; color: ink }
    }

    MpvItem {
        id: mpvItem
        anchors.fill: parent
    }

    // Video surface interactions: click toggles pause, double click toggles
    // fullscreen, mouse movement reveals the control bar.
    MouseArea {
        id: videoMouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        onClicked: function (mouse) { mouse.accepted = true; mpvItem.togglePlayPause() }
        onDoubleClicked: function (mouse) { mouse.accepted = true; root.toggleFullscreen() }
        onPositionChanged: root.showControlsTemporarily()
    }

    // Center pause indicator (pure shapes, no font dependency).
    Row {
        anchors.centerIn: parent
        spacing: 12
        opacity: (mpvItem.paused && !mpvItem.idle) ? 0.28 : 0
        visible: opacity > 0
        Repeater {
            model: 2
            Rectangle { width: 16; height: 58; radius: 3; color: "#ffffff" }
        }
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

    // Idle state hint.
    Column {
        anchors.centerIn: parent
        spacing: 10
        visible: mpvItem.idle
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("EmoPlay")
            font.pixelSize: 40
            font.weight: Font.DemiBold
            color: "#e8e8ee"
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Open a file to start playback  (O)")
            color: "#8a8a96"
        }

        // Recently played, persisted by HistoryStore; click to replay.
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: historyStore.entries.length > 0
            text: qsTr("Recently played")
            font.weight: Font.DemiBold
            color: "#cfcfd8"
        }
        ListView {
            id: historyList
            anchors.horizontalCenter: parent.horizontalCenter
            visible: historyStore.entries.length > 0
            width: Math.min(root.width * 0.6, 560)
            height: Math.min(contentHeight, 180)
            clip: true
            model: historyStore.entries
            delegate: ItemDelegate {
                width: ListView.view.width
                text: (modelData.title && modelData.title.length > 0)
                      ? modelData.title : modelData.path
                onClicked: mpvEngine.loadFile(modelData.path)
            }
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }

    // Bottom control bar; auto-hides during playback when the mouse is idle.
    Rectangle {
        id: controls
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 96
        color: "#d014141a"
        opacity: 1

        Behavior on opacity { NumberAnimation { duration: 250 } }

        HoverHandler { id: controlsHover }

        // Consume clicks on the bar so they do not reach the video MouseArea.
        MouseArea {
            anchors.fill: parent
            onClicked: function (mouse) { mouse.accepted = true }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.topMargin: 8
            anchors.bottomMargin: 10
            spacing: 2

            Label {
                text: root.formatTime(root.currentTime)
                      + " / " + root.formatTime(mpvItem.duration)
                color: "#cfcfd8"
                font.pixelSize: 12
            }

            Slider {
                id: seekSlider
                Layout.fillWidth: true

                property bool userSeeking: false

                from: 0
                to: mpvItem.duration > 0 ? mpvItem.duration : 1
                enabled: mpvItem.duration > 0

                onPressedChanged: {
                    if (pressed) {
                        userSeeking = true
                    } else {
                        userSeeking = false
                        if (mpvItem.duration > 0)
                            mpvItem.seek(seekSlider.value)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Button {
                    text: qsTr("Open")
                    flat: true
                    onClicked: fileDialog.open()
                }
                IconButton {
                    glyph: mpvItem.paused ? "\u25B6" : "\u25AE\u25AE"
                    ToolTip.text: mpvItem.paused ? qsTr("Play") : qsTr("Pause")
                    onClicked: mpvItem.togglePlayPause()
                }
                IconButton {
                    glyph: "\u25A0"
                    ToolTip.text: qsTr("Stop")
                    onClicked: mpvItem.stop()
                }
                Item { Layout.fillWidth: true }

                IconButton {
                    glyph: "\u2630"
                    ToolTip.text: qsTr("Playlist")
                    onClicked: playlistDrawer.opened ? playlistDrawer.close()
                                                     : playlistDrawer.open()
                }
                IconButton {
                    // Gear, not a note: the mute button next to it uses
                    // "\u266A" and note glyphs are too easy to mix up.
                    glyph: "\u2699"
                    ToolTip.text: qsTr("Tracks")
                    onClicked: tracksDrawer.opened ? tracksDrawer.close()
                                                   : tracksDrawer.open()
                }
                IconButton {
                    glyph: "\u266A"
                    opacity: mpvItem.muted ? 0.4 : 1
                    ToolTip.text: qsTr("Mute")
                    onClicked: mpvItem.muted = !mpvItem.muted
                }
                Slider {
                    id: volumeSlider
                    from: 0
                    to: 100
                    implicitWidth: 120
                    onMoved: mpvItem.volume = value
                }
                IconButton {
                    contentItem: FullscreenIcon { ink: "#cfcfd8" }
                    ToolTip.text: qsTr("Fullscreen")
                    onClicked: root.toggleFullscreen()
                }
            }
        }
    }

    Timer {
        id: hideTimer
        interval: 2500
        onTriggered: {
            if (!mpvItem.paused && !controlsHover.hovered && !seekSlider.pressed)
                controls.opacity = 0
        }
    }

    Connections {
        target: mpvItem
        // Slider values are synced imperatively: declarative bindings would be
        // destroyed the first time the user drags the handle.
        function onPositionChanged() {
            if (!seekSlider.userSeeking && !seekSlider.pressed)
                seekSlider.value = mpvItem.position
        }
        function onVolumeChanged() {
            if (!volumeSlider.pressed)
                volumeSlider.value = mpvItem.volume
        }
        function onDurationChanged() {
            if (!seekSlider.pressed)
                seekSlider.value = 0
        }
        function onPausedChanged() {
            if (mpvItem.paused) {
                controls.opacity = 1
                hideTimer.stop()
            }
        }
        function onIdleChanged() {
            if (mpvItem.idle) {
                controls.opacity = 1
                hideTimer.stop()
                seekSlider.value = 0
            }
        }
    }

    Connections {
        target: mpvEngine
        function onErrorOccurred(message) {
            console.warn("mpv error:", message)
        }
        function onFileLoaded() {
            root.showControlsTemporarily()
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Open media file")
        nameFilters: [
            qsTr("Media files (*.mkv *.mp4 *.m4v *.avi *.mov *.webm *.ts *.flac *.mp3 *.m4a *.ogg *.opus *.wav)"),
            qsTr("All files (*)")
        ]
        onAccepted: mpvEngine.loadFile(selectedFile)
    }

    // Track selection side drawer: audio tracks and subtitles come from the
    // engine's mirrored track-list; the marked entry is mpv's "selected".
    Drawer {
        id: tracksDrawer
        edge: Qt.RightEdge
        width: Math.min(root.width * 0.38, 380)

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 4

            Label {
                text: qsTr("Audio tracks")
                font.weight: Font.DemiBold
                color: "#cfcfd8"
            }
            Column {
                Layout.fillWidth: true
                Repeater {
                    model: mpvItem.trackList.filter(function (t) { return t.type === "audio" })
                    delegate: ItemDelegate {
                        width: parent.width
                        highlighted: modelData.selected === true
                        text: root.trackLabel(modelData, qsTr("Audio"))
                        onClicked: mpvItem.selectAudioTrack(modelData.id)
                    }
                }
                ItemDelegate {
                    width: parent.width
                    text: qsTr("Audio off")
                    highlighted: !mpvItem.trackList.some(function (t) {
                        return t.type === "audio" && t.selected === true
                    })
                    onClicked: mpvItem.selectAudioTrack("no")
                }
            }

            Label {
                text: qsTr("Subtitles")
                font.weight: Font.DemiBold
                color: "#cfcfd8"
            }
            Column {
                Layout.fillWidth: true
                Repeater {
                    model: mpvItem.trackList.filter(function (t) { return t.type === "sub" })
                    delegate: ItemDelegate {
                        width: parent.width
                        highlighted: modelData.selected === true
                        text: root.trackLabel(modelData, qsTr("Subtitle"))
                        onClicked: mpvItem.selectSubtitleTrack(modelData.id)
                    }
                }
                ItemDelegate {
                    width: parent.width
                    text: qsTr("Subtitles off")
                    highlighted: !mpvItem.trackList.some(function (t) {
                        return t.type === "sub" && t.selected === true
                    })
                    onClicked: mpvItem.selectSubtitleTrack("no")
                }
            }

            Item { Layout.fillHeight: true }

            Button {
                Layout.fillWidth: true
                text: qsTr("Load external subtitle…")
                onClicked: subtitleDialog.open()
            }
        }
    }

    FileDialog {
        id: subtitleDialog
        title: qsTr("Load external subtitle")
        nameFilters: [
            qsTr("Subtitle files (*.srt *.ass *.ssa *.sub *.vtt *.idx *.sup)"),
            qsTr("All files (*)")
        ]
        onAccepted: mpvItem.loadExternalSubtitle(subtitleDialog.selectedFile)
    }

    // Playlist side drawer; the list itself is the engine's mirrored
    // "playlist" property, so the playing entry marker comes from mpv.
    Drawer {
        id: playlistDrawer
        edge: Qt.RightEdge
        width: Math.min(root.width * 0.42, 440)

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 6

            Label {
                text: qsTr("Playlist")
                font.weight: Font.DemiBold
                color: "#cfcfd8"
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: playlistList
                    anchors.fill: parent
                    clip: true
                    model: mpvItem.playlist
                    delegate: ItemDelegate {
                        width: ListView.view.width
                        highlighted: modelData.playing === true
                        text: (modelData.title && modelData.title.length > 0)
                              ? modelData.title : modelData.filename
                        onClicked: mpvItem.playlistPlayIndex(index)
                    }
                    ScrollIndicator.vertical: ScrollIndicator {}
                }

                Label {
                    anchors.centerIn: parent
                    visible: playlistList.count === 0
                    text: qsTr("Playlist is empty — add files below")
                    color: "#8a8a96"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Button {
                    text: qsTr("Add files")
                    onClicked: playlistFileDialog.open()
                }
                Button {
                    text: qsTr("Clear")
                    onClicked: mpvItem.playlistClear()
                }
                Item { Layout.fillWidth: true }
                IconButton {
                    glyph: "\u25C0"
                    ToolTip.text: qsTr("Previous")
                    onClicked: mpvItem.playlistPrev()
                }
                IconButton {
                    glyph: "\u25B6"
                    ToolTip.text: qsTr("Next")
                    onClicked: mpvItem.playlistNext()
                }
            }
        }
    }

    FileDialog {
        id: playlistFileDialog
        title: qsTr("Add files to playlist")
        fileMode: FileDialog.OpenFiles
        nameFilters: [
            qsTr("Media files (*.mkv *.mp4 *.m4v *.avi *.mov *.webm *.ts *.flac *.mp3 *.m4a *.ogg *.opus *.wav)"),
            qsTr("All files (*)")
        ]
        onAccepted: {
            for (var i = 0; i < playlistFileDialog.selectedFiles.length; ++i)
                mpvItem.appendToPlaylist(playlistFileDialog.selectedFiles[i])
        }
    }

    Shortcut { sequence: "Space"; onActivated: mpvItem.togglePlayPause() }
    Shortcut { sequence: "Left"; onActivated: mpvItem.seekBy(-5) }
    Shortcut { sequence: "Right"; onActivated: mpvItem.seekBy(5) }
    Shortcut {
        sequence: "Up"
        onActivated: mpvItem.volume = Math.min(130, mpvItem.volume + 5)
    }
    Shortcut {
        sequence: "Down"
        onActivated: mpvItem.volume = Math.max(0, mpvItem.volume - 5)
    }
    Shortcut { sequence: "M"; onActivated: mpvItem.muted = !mpvItem.muted }
    Shortcut { sequence: "F"; onActivated: root.toggleFullscreen() }
    Shortcut { sequence: "O"; onActivated: fileDialog.open() }
    Shortcut {
        sequence: "T"
        onActivated: tracksDrawer.opened ? tracksDrawer.close() : tracksDrawer.open()
    }
    Shortcut {
        sequence: "S"
        onActivated: if (!mpvItem.idle) subtitleDialog.open()
    }
    Shortcut { sequence: "<"; onActivated: mpvItem.playlistPrev() }
    Shortcut { sequence: ">"; onActivated: mpvItem.playlistNext() }
    Shortcut { sequence: "Q"; onActivated: root.close() }
    Shortcut {
        sequence: "Esc"
        onActivated: if (root.isFullscreen) root.showNormal()
    }
}
