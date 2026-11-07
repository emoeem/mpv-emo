import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs
import EmoPlay.Mpv

ApplicationWindow {
    id: root

    readonly property bool isFullscreen: visibility === Window.FullScreen
    // P7 music mode: read-only binding over the controller's audio
    // detection (track-list has no video AND the filename is an audio
    // extension); drives the MusicPage loader below.
    readonly property bool musicMode: musicController.audioMode
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

    // Forward a key event to mpv's input layer so bindings from an imported
    // input.conf (P4 compat layer) and script-registered bindings fire.
    // Keys already consumed by EmoPlay's own Shortcuts/controls never get
    // here — those keep their built-in behavior.
    function forwardKeyEvent(event) {
        if (event.isAutoRepeat)
            return
        var name = mpvEngine.mpvKeyName(event.key, event.modifiers, event.text)
        if (name.length > 0) {
            mpvEngine.keyPress(name)
            event.accepted = true
        }
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
    color: Theme.bg0

    palette.window: Theme.bg0
    palette.windowText: Theme.fg
    palette.base: Theme.bg2
    palette.button: Theme.bg2
    palette.buttonText: Theme.fg
    palette.text: Theme.fg
    palette.highlight: Theme.accent
    palette.highlightedText: "#191a21"
    palette.toolTipBase: Theme.bg3
    palette.toolTipText: Theme.fg

    component IconButton: Button {
        id: iconButton
        property string glyph: ""
        property real iconSize: 20
        implicitWidth: 40
        implicitHeight: 40
        flat: true
        hoverEnabled: true
        contentItem: AppIcon {
            anchors.centerIn: parent
            name: iconButton.glyph
            size: iconButton.iconSize
            color: iconButton.hovered ? Theme.fg : Theme.fgDim
        }
        background: Rectangle {
            radius: Theme.radiusS
            color: iconButton.pressed ? Theme.bg3
                 : (iconButton.hovered ? Theme.bg2 : "transparent")
            Behavior on color { ColorAnimation { duration: Theme.animFast } }
        }
        ToolTip.visible: hovered && ToolTip.text.length > 0
        ToolTip.delay: 600
    }

    // Quality drawers share one combo shape (label/value pairs backed by
    // the controller's persisted string values).
    component QualityCombo: ComboBox {
        textRole: "label"
        valueRole: "value"
        Layout.fillWidth: true
    }

    // Section header inside drawers.
    component SectionHeader: Label {
        font.pixelSize: 12
        font.weight: Font.DemiBold
        font.letterSpacing: 0.8
        color: Theme.muted
    }

    function selectQualityValue(combo, value) {
        for (var i = 0; i < combo.count; ++i) {
            if (combo.valueAt(i) === value) {
                combo.currentIndex = i
                return
            }
        }
    }

    function syncQualityControls() {
        selectQualityValue(contentModeCombo, qualityController.contentMode)
        selectQualityValue(tierCombo, qualityController.qualityTier)
        selectQualityValue(scaleCombo, qualityController.scaleOverride)
        selectQualityValue(debandCombo, qualityController.debandMode)
        selectQualityValue(toneMappingCombo, qualityController.toneMappingMode)
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
        onClicked: function (mouse) {
            mouse.accepted = true
            compatKeys.forceActiveFocus()
            mpvItem.togglePlayPause()
        }
        onDoubleClicked: function (mouse) { mouse.accepted = true; root.toggleFullscreen() }
        onPositionChanged: root.showControlsTemporarily()
    }

    // Default key-focus holder while the video surface is active: keys that
    // nothing else consumed are forwarded to mpv (see forwardKeyEvent).
    Item {
        id: compatKeys
        focus: true
        Keys.onPressed: function (event) { root.forwardKeyEvent(event) }
    }

    // Center pause indicator: soft accent glow behind two bars.
    Item {
        anchors.centerIn: parent
        opacity: (mpvItem.paused && !mpvItem.idle) ? 1 : 0
        visible: opacity > 0
        scale: mpvItem.paused ? 1 : 0.8

        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
        Behavior on scale { NumberAnimation {
            duration: Theme.animNormal; easing.type: Easing.OutBack } }

        Rectangle {
            anchors.centerIn: parent
            width: 132
            height: 132
            radius: Theme.radiusL
            color: "#b3191a21"
            border.color: Theme.border
        }
        Row {
            anchors.centerIn: parent
            spacing: 14
            Repeater {
                model: 2
                Rectangle { width: 14; height: 52; radius: 4; color: Theme.accent }
            }
        }
    }

    // Idle state hint: wordmark + hint + recently played card.
    Column {
        anchors.centerIn: parent
        spacing: 14
        visible: mpvItem.idle

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 0
            Text {
                text: qsTr("Emo")
                font.pixelSize: 44
                font.weight: Font.Bold
                font.letterSpacing: 1
                color: Theme.fg
            }
            Text {
                text: qsTr("Play")
                font.pixelSize: 44
                font.weight: Font.Bold
                font.letterSpacing: 1
                color: Theme.accent
            }
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Open a file to start playback  (O)")
            color: Theme.muted
            font.pixelSize: 13
        }

        // Recently played, persisted by HistoryStore; click to replay.
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: historyStore.entries.length > 0
            width: Math.min(root.width * 0.6, 560)
            height: recentsColumn.implicitHeight + 24
            radius: Theme.radiusM
            color: Theme.bg1
            border.color: Theme.border

            Column {
                id: recentsColumn
                anchors.fill: parent
                anchors.margins: 12
                spacing: 2

                Text {
                    text: qsTr("Recently played")
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.8
                    color: Theme.muted
                }
                ListView {
                    id: historyList
                    width: parent.width
                    height: Math.min(contentHeight, 168)
                    clip: true
                    model: historyStore.entries
                    delegate: ItemDelegate {
                        width: ListView.view.width
                        highlighted: false
                        text: (modelData.title && modelData.title.length > 0)
                              ? modelData.title : modelData.path
                        onClicked: mpvEngine.loadFile(modelData.path)
                    }
                    ScrollIndicator.vertical: ScrollIndicator {}
                }
            }
        }
    }

    // P7 music mode: while the current media is pure audio, the video area
    // is covered by the music page (big title, lyrics, equalizer). Playback
    // controls stay in the bottom bar. Declared before the control bar so
    // the bar still renders on top.
    Loader {
        anchors.fill: parent
        active: root.musicMode
        source: "MusicPage.qml"
        onLoaded: item.mpvItem = mpvItem
    }

    // Floating pill control bar: layered soft shadow behind a translucent
    // rounded surface; auto-hides during playback when the mouse is idle.
    Item {
        id: controls
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.barMargin
        width: Math.min(parent.width - 40, 1080)
        height: Theme.barHeight
        opacity: 1

        Behavior on opacity { NumberAnimation { duration: Theme.animSlow } }

        HoverHandler { id: controlsHover }

        // Forward keys not consumed by the sliders/buttons themselves.
        Keys.onPressed: function (event) { root.forwardKeyEvent(event) }

        // Layered fake shadow (three stacked rounded rects).
        Rectangle {
            anchors.fill: parent
            anchors.margins: -6
            radius: Theme.radiusL + 6
            color: "#14000000"
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: Theme.radiusL + 3
            color: "#22000000"
        }

        // Consume clicks on the bar so they do not reach the video MouseArea.
        MouseArea {
            anchors.fill: parent
            onClicked: function (mouse) { mouse.accepted = true }
        }

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusL
            color: Theme.barTint
            border.color: Theme.border
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                anchors.topMargin: 8
                anchors.bottomMargin: 10
                spacing: 0

                PillSlider {
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
                    spacing: 4

                    IconButton {
                        glyph: "folder"
                        iconSize: 18
                        ToolTip.text: qsTr("Open")
                        onClicked: fileDialog.open()
                    }
                    Label {
                        text: root.formatTime(root.currentTime)
                        color: Theme.fg
                        font.pixelSize: 12
                        font.family: "monospace"
                        Layout.preferredWidth: 52
                        horizontalAlignment: Text.AlignRight
                    }
                    Label {
                        text: "/"
                        color: Theme.muted
                        font.pixelSize: 12
                    }
                    Label {
                        text: root.formatTime(mpvItem.duration)
                        color: Theme.muted
                        font.pixelSize: 12
                        font.family: "monospace"
                        Layout.preferredWidth: 52
                    }

                    AccentButton {
                        glyph: (mpvItem.paused || mpvItem.idle) ? "play" : "pause"
                        diameter: 44
                        ToolTip.text: mpvItem.paused ? qsTr("Play") : qsTr("Pause")
                        onClicked: mpvItem.togglePlayPause()
                    }
                    IconButton {
                        glyph: "stop"
                        ToolTip.text: qsTr("Stop")
                        onClicked: mpvItem.stop()
                    }
                    IconButton {
                        glyph: "prev"
                        ToolTip.text: qsTr("Previous")
                        onClicked: mpvItem.playlistPrev()
                    }
                    IconButton {
                        glyph: "next"
                        ToolTip.text: qsTr("Next")
                        onClicked: mpvItem.playlistNext()
                    }

                    Item { Layout.fillWidth: true }

                    IconButton {
                        glyph: "list"
                        ToolTip.text: qsTr("Playlist")
                        onClicked: playlistDrawer.opened ? playlistDrawer.close()
                                                         : playlistDrawer.open()
                    }
                    IconButton {
                        glyph: "subtitles"
                        ToolTip.text: qsTr("Tracks")
                        onClicked: tracksDrawer.opened ? tracksDrawer.close()
                                                       : tracksDrawer.open()
                    }
                    IconButton {
                        glyph: "quality"
                        ToolTip.text: qsTr("Quality")
                        onClicked: qualityDrawer.opened ? qualityDrawer.close()
                                                        : qualityDrawer.open()
                    }
                    IconButton {
                        glyph: "library"
                        ToolTip.text: qsTr("Library")
                        onClicked: libraryDrawer.opened ? libraryDrawer.close()
                                                        : libraryDrawer.open()
                    }
                    IconButton {
                        glyph: "plugins"
                        ToolTip.text: qsTr("Plugins")
                        onClicked: pluginDrawer.opened ? pluginDrawer.close()
                                                       : pluginDrawer.open()
                    }
                    UpdateBadge {
                    }
                    IconButton {
                        glyph: mpvItem.muted ? "mute" : "volume"
                        ToolTip.text: qsTr("Mute")
                        onClicked: mpvItem.muted = !mpvItem.muted
                    }
                    PillSlider {
                        id: volumeSlider
                        from: 0
                        to: 100
                        implicitWidth: 96
                        onMoved: mpvItem.volume = value
                    }
                    IconButton {
                        glyph: root.isFullscreen ? "fullscreenExit" : "fullscreen"
                        ToolTip.text: qsTr("Fullscreen")
                        onClicked: root.toggleFullscreen()
                    }
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
        // P5 auto-downgrade input: MpvItem samples the drop counter every
        // 2s; the quality controller demotes the active tier when the rate
        // explodes (the panel shows the hint, no dialog).
        function onDroppedFrameRateChanged(rate) {
            qualityController.notifyDropFrameRate(rate)
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
        background: Rectangle { color: Theme.bg1 }
        onClosed: compatKeys.forceActiveFocus()
        Keys.onPressed: function (event) { root.forwardKeyEvent(event) }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 4

            Label {
                text: qsTr("Audio tracks")
                font.pixelSize: 12
                font.weight: Font.DemiBold
                font.letterSpacing: 0.8
                color: Theme.muted
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

            SectionHeader {
                text: qsTr("Subtitles")
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

    // P5 quality settings drawer. All controls follow the imperative-sync
    // convention: user changes call qualityController setters, and
    // controller updates are pulled back via Connections (declarative
    // bindings would be destroyed by the first user interaction).
    // Tone-mapping offers only values that work under vo=libmpv (classic
    // gpu renderer in mpv 0.37): "spline"/"bt.2446a" are gpu-next only.
    Drawer {
        id: qualityDrawer
        edge: Qt.RightEdge
        width: Math.min(root.width * 0.46, 460)
        background: Rectangle { color: Theme.bg1 }
        onOpened: root.syncQualityControls()
        onClosed: compatKeys.forceActiveFocus()
        Keys.onPressed: function (event) { root.forwardKeyEvent(event) }

        Connections {
            target: qualityController
            function onPrefsChanged() { root.syncQualityControls() }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 8

            Label {
                text: qsTr("Quality")
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: Theme.fg
            }

            Label {
                Layout.fillWidth: true
                visible: qualityController.autoDowngraded
                text: qsTr("Auto-downgraded to Low: dropped frames detected")
                color: Theme.orange
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            SectionHeader {
                text: qsTr("Content type").toUpperCase()
            }
            QualityCombo {
                id: contentModeCombo
                model: [
                    { label: qsTr("Auto (detect)"), value: "auto" },
                    { label: qsTr("Animation"), value: "anime" },
                    { label: qsTr("Live action"), value: "live" }
                ]
                onActivated: function (index) {
                    qualityController.setContentMode(contentModeCombo.valueAt(index))
                }
            }

            SectionHeader {
                text: qsTr("Quality tier").toUpperCase()
            }
            QualityCombo {
                id: tierCombo
                model: [
                    { label: qsTr("Off (mpv defaults)"), value: "off" },
                    { label: qsTr("Low"), value: "low" },
                    { label: qsTr("Medium"), value: "medium" },
                    { label: qsTr("High"), value: "high" }
                ]
                onActivated: function (index) {
                    qualityController.setQualityTier(tierCombo.valueAt(index))
                }
            }

            SectionHeader {
                text: qsTr("Scaling algorithm").toUpperCase()
            }
            QualityCombo {
                id: scaleCombo
                model: [
                    { label: qsTr("Auto (by tier)"), value: "" },
                    { label: "ewa_lanczossharp", value: "ewa_lanczossharp" },
                    { label: "lanczos", value: "lanczos" },
                    { label: "bilinear", value: "bilinear" }
                ]
                onActivated: function (index) {
                    qualityController.setScaleOverride(scaleCombo.valueAt(index))
                }
            }

            SectionHeader {
                text: qsTr("Deband").toUpperCase()
            }
            QualityCombo {
                id: debandCombo
                model: [
                    { label: qsTr("Auto"), value: "auto" },
                    { label: qsTr("On"), value: "yes" },
                    { label: qsTr("Off"), value: "no" }
                ]
                onActivated: function (index) {
                    qualityController.setDebandMode(debandCombo.valueAt(index))
                }
            }

            SectionHeader {
                text: qsTr("HDR tone-mapping").toUpperCase()
            }
            QualityCombo {
                id: toneMappingCombo
                model: [
                    { label: qsTr("Auto (by tier)"), value: "auto" },
                    { label: "BT.2390", value: "bt.2390" },
                    { label: "Mobius", value: "mobius" },
                    { label: "Hable", value: "hable" }
                ]
                onActivated: function (index) {
                    qualityController.setToneMappingMode(toneMappingCombo.valueAt(index))
                }
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("For HDR sources; shaders are not mounted while "
                           + "HDR tone-mapping is active. spline / bt.2446a "
                           + "require gpu-next and are unavailable here.")
                color: Theme.muted
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }

            // P5 v2 sections live in their own files (additive-only
            // Main.qml changes); they talk to their controllers directly.
            InterpolationPanel {
                Layout.fillWidth: true
            }
            IccPanel {
                Layout.fillWidth: true
            }

            SectionHeader {
                text: qsTr("Shaders").toUpperCase()
            }
            Label {
                Layout.fillWidth: true
                text: qualityController.shadersDir
                color: Theme.muted
                font.pixelSize: 11
                elide: Text.ElideMiddle
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: shaderList
                    anchors.fill: parent
                    clip: true
                    model: qualityController.availableShaders
                    delegate: CheckBox {
                        id: shaderCheck
                        width: ListView.view.width
                        text: modelData
                        font.pixelSize: 12

                        // Imperative sync: reflect the controller's manual
                        // list; user toggles push back through the setter.
                        function syncShader() {
                            shaderCheck.checked =
                                qualityController.manualShaders.indexOf(
                                    qualityController.shadersDir + "/"
                                    + modelData) >= 0
                        }
                        Component.onCompleted: syncShader()
                        onToggled: qualityController.setShaderMounted(
                                       modelData, shaderCheck.checked)
                        Connections {
                            target: qualityController
                            function onShadersChanged() { shaderCheck.syncShader() }
                        }
                    }
                    ScrollIndicator.vertical: ScrollIndicator {}
                }

                Label {
                    anchors.centerIn: parent
                    visible: shaderList.count === 0
                    text: qsTr("No .glsl files found in the shaders directory")
                    color: Theme.muted
                }
            }
        }
    }

    // P6 media library drawer (folder list, scan, search, entries). Size,
    // focus return and key forwarding need Main.qml ids, so they are set
    // here; everything else lives in LibraryDrawer.qml.
    LibraryDrawer {
        id: libraryDrawer
        width: Math.min(root.width * 0.46, 460)
        onClosed: compatKeys.forceActiveFocus()
        Keys.onPressed: function (event) { root.forwardKeyEvent(event) }
    }

    // Playlist side drawer; the list itself is the engine's mirrored
    // "playlist" property, so the playing entry marker comes from mpv.
    Drawer {
        id: playlistDrawer
        edge: Qt.RightEdge
        width: Math.min(root.width * 0.42, 440)
        background: Rectangle { color: Theme.bg1 }
        onClosed: compatKeys.forceActiveFocus()
        Keys.onPressed: function (event) { root.forwardKeyEvent(event) }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 6

            Label {
                text: qsTr("Playlist")
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: Theme.fg
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
                    color: Theme.muted
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
                    glyph: "prev"
                    ToolTip.text: qsTr("Previous")
                    onClicked: mpvItem.playlistPrev()
                }
                IconButton {
                    glyph: "next"
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

    // P4 compat: first-run import wizard (opens when a user mpv config was
    // detected and no decision is recorded yet), also reachable via
    // compatController.requestWizard() from a settings page later.
    ImportWizard {
        id: importWizard
    }

    // P8 plugin drawer: install/update/uninstall upstream plugins; every
    // operation asks for confirmation inside the drawer itself.
    PluginDrawer {
        id: pluginDrawer
        forwardKeyEvent: root.forwardKeyEvent
        keyFocusTarget: compatKeys
    }
    Component.onCompleted: {
        if (compatController.firstRunPromptNeeded)
            importWizard.open()
    }
    Connections {
        target: compatController
        function onStateChanged() {
            if (compatController.wizardRequested && !importWizard.opened)
                importWizard.open()
        }
    }
}
