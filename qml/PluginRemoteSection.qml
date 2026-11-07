import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// P8 v2 online-sources section of the plugin drawer (its own file so the
// PluginDrawer.qml change stays additive): entries fetched from
// manager.json-style manifest sources (the default upstream source plus any
// user-added ones), merged with the local install state by
// RemoteManifestSource. Install / update / uninstall reuse the drawer's
// confirmation dialog through |drawer|; enable/disable applies immediately.
//
// Network failures degrade gracefully: RemoteManifestSource keeps the last
// cached list and reports the failure via errorOccurred, which lands in the
// hint label — the section never blocks and never crashes offline.
ColumnLayout {
    id: remoteSection

    // The PluginDrawer hosting this section: beginOperation() opens the
    // shared confirmation dialog (an id from another file is not visible
    // inside this document, hence the reference).
    property var drawer: null
    property bool refreshedOnce: false

    // First drawer show refreshes once; later opens reuse the cached/last
    // result until the user hits Refresh (bounded network chatter, and the
    // CI smoke test never opens the drawer so it never hits the network).
    function refreshOnShow() {
        if (refreshedOnce)
            return
        refreshedOnce = true
        remotePluginSources.refreshAll()
    }

    function newestRefreshMs() {
        var best = 0
        var rows = remotePluginSources.sources
        for (var i = 0; i < rows.length; ++i)
            if (rows[i].lastRefreshMs > best)
                best = rows[i].lastRefreshMs
        return best
    }

    function addSourceFromInput() {
        if (remotePluginSources.addSource(sourceInput.text))
            sourceInput.text = ""
        // A rejection reports the precise reason via errorOccurred below;
        // keep the typed text so it can be corrected.
    }

    // Mirrors PluginDrawer.statusText() (same local-state fields).
    function statusText(entry) {
        if (!entry.installed)
            return qsTr("Not installed")
        if (entry.updateAvailable)
            return qsTr("Update available")
        if (entry.remoteHead.length > 0)
            return qsTr("Up to date")
        return qsTr("Installed")
    }

    spacing: 6

    RowLayout {
        Layout.fillWidth: true

        Label {
            text: qsTr("Online sources")
            font.pixelSize: 14
            font.weight: Font.DemiBold
            color: "#e8e8ee"
        }
        Item { Layout.fillWidth: true }
        Button {
            flat: true
            enabled: !remotePluginSources.refreshing && !pluginManager.busy
            text: remotePluginSources.refreshing ? qsTr("Refreshing…")
                                                 : qsTr("Refresh")
            onClicked: remotePluginSources.refreshAll()
        }
    }

    // Last refresh time (the newest across the configured sources).
    Label {
        Layout.fillWidth: true
        color: remotePluginSources.refreshing ? "#ffb454" : "#8a8a96"
        font.pixelSize: 11
        text: {
            if (remotePluginSources.refreshing)
                return qsTr("Fetching the online sources…")
            var ms = remoteSection.newestRefreshMs()
            return ms > 0
                ? qsTr("Last refreshed: %1").arg(new Date(ms).toLocaleString())
                : qsTr("Never refreshed")
        }
    }

    // Hint line: network/parse/cache failures land here (graceful
    // degradation is a message, not a crash).
    Label {
        id: remoteHint
        Layout.fillWidth: true
        visible: text.length > 0
        color: "#8a8a96"
        font.pixelSize: 11
        wrapMode: Text.WordWrap
        Connections {
            target: remotePluginSources
            function onErrorOccurred(message) {
                remoteHint.text = message
            }
        }
    }

    // The configured manifest sources; custom ones can be removed.
    Repeater {
        model: remotePluginSources.sources
        delegate: RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: modelData.url
                color: "#8a8a96"
                font.pixelSize: 10
                elide: Text.ElideMiddle
            }
            Label {
                visible: modelData.isDefault
                text: qsTr("default")
                color: "#5f6b7a"
                font.pixelSize: 10
            }
            Button {
                flat: true
                visible: !modelData.isDefault
                enabled: !remotePluginSources.refreshing
                text: qsTr("Remove")
                onClicked: remotePluginSources.removeSource(modelData.url)
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        TextField {
            id: sourceInput
            Layout.fillWidth: true
            placeholderText: qsTr("Add a manifest source URL (https://…)")
            font.pixelSize: 11
            onAccepted: remoteSection.addSourceFromInput()
        }
        Button {
            flat: true
            enabled: sourceInput.text.length > 0
            text: qsTr("Add")
            onClicked: remoteSection.addSourceFromInput()
        }
    }

    ListView {
        id: remoteList
        Layout.fillWidth: true
        Layout.preferredHeight: 170
        clip: true
        spacing: 6
        model: remotePluginSources.entries

        delegate: Rectangle {
            width: ListView.view.width
            height: cardBody.implicitHeight + 16
            color: "#1a1a22"
            radius: 6

            ColumnLayout {
                id: cardBody
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 8
                spacing: 3

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: modelData.name
                        font.weight: Font.DemiBold
                        color: "#e8e8ee"
                    }
                    Rectangle {
                        radius: 4
                        color: modelData.mode === "replace" ? "#5a3d1e"
                                                            : "#1e3a5a"
                        // Implicit (not explicit) sizes: the badge is managed
                        // by the surrounding RowLayout — explicit width/
                        // height on a layout child is undefined behavior
                        // (qmllint Quick.layout-positioning).
                        implicitWidth: modeLabel.implicitWidth + 10
                        implicitHeight: modeLabel.implicitHeight + 4
                        Label {
                            id: modeLabel
                            anchors.centerIn: parent
                            text: modelData.mode
                            font.pixelSize: 10
                            color: "#e8e8ee"
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: remoteSection.statusText(modelData)
                        color: modelData.updateAvailable ? "#ffb454"
                                                         : "#8a8a96"
                        font.pixelSize: 12
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: modelData.url + "  (" + modelData.branch + ")"
                    color: "#8a8a96"
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: modelData.isScript && modelData.installed
                    Label {
                        text: qsTr("Enable (next launch)")
                        color: "#cfcfd8"
                        font.pixelSize: 12
                    }
                    Switch {
                        id: enableSwitch
                        checked: modelData.enabled
                        enabled: modelData.installed && !pluginManager.busy
                        onToggled: {
                            // Reflect the model, not the flip (same pattern
                            // as PluginDrawer): the action derives from the
                            // re-asserted model value.
                            enableSwitch.checked = Qt.binding(function () {
                                return modelData.enabled
                            })
                            remoteSection.drawer.beginOperation(
                                modelData.name,
                                enableSwitch.checked ? "disable" : "enable")
                        }
                    }
                    Item { Layout.fillWidth: true }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Button {
                        flat: true
                        visible: !modelData.installed
                        enabled: !pluginManager.busy
                        text: qsTr("Install")
                        onClicked: remoteSection.drawer.beginOperation(
                                       modelData.name, "install")
                    }
                    Button {
                        flat: true
                        visible: modelData.installed
                        enabled: !pluginManager.busy
                        text: qsTr("Check")
                        onClicked: pluginManager.checkForUpdate(modelData.name)
                    }
                    Button {
                        flat: true
                        visible: modelData.installed
                        enabled: !pluginManager.busy
                        text: qsTr("Update")
                        onClicked: remoteSection.drawer.beginOperation(
                                       modelData.name, "update")
                    }
                    Button {
                        flat: true
                        visible: modelData.installed
                        enabled: !pluginManager.busy
                        text: qsTr("Uninstall")
                        onClicked: remoteSection.drawer.beginOperation(
                                       modelData.name, "uninstall")
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }

        ScrollIndicator.vertical: ScrollIndicator {}
    }

    Label {
        Layout.fillWidth: true
        visible: remoteList.count === 0
        text: qsTr("No online entries yet — refresh to fetch the manifest "
                   + "sources.")
        color: "#8a8a96"
        font.pixelSize: 11
        wrapMode: Text.WordWrap
    }
}
