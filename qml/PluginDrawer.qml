import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// P8 plugin drawer: install / update / uninstall upstream plugins declared in
// the built-in manager.json-compatible catalog (uosc, uosc_danmaku, Anime4K,
// file_browser, hdr-toys, trakt_scrobble).
//
// Install / update / uninstall show their change list in a confirmation
// dialog before they run — PluginManager does nothing without that consent.
// The enable/disable switch is non-destructive and reversible (the scripts
// option is only assembled at the next launch), so it applies immediately.
// Check-mode plugins ("本地定制永不覆盖"): updates only compare the remote
// HEAD with the manifest baseline via git ls-remote; local customizations
// are never overwritten. Replace-mode updates re-clone and swap, but are
// refused when local modifications are detected.
Drawer {
    id: pluginDrawer

    // Injected by Main.qml so keys not consumed here still reach mpv's input
    // layer (P4 compat forwarding) and focus returns to the video surface.
    property var forwardKeyEvent: null
    property Item keyFocusTarget: null
    // The pending operation awaiting confirmation: {name, action, title,
    // details: [..]} — as returned by pluginManager.planOperation().
    property var pendingPlan: null

    function beginOperation(name, action) {
        // Enable/disable applies immediately: it is non-destructive and
        // reversible, and planOperation only plans the three destructive
        // actions (an enable/disable plan would be refused as an unknown
        // action, leaving the switch stuck on the model value).
        if (action === "enable" || action === "disable") {
            pluginManager.setEnabled(name, action === "enable")
            return
        }
        var plan = pluginManager.planOperation(name, action)
        if (plan.error !== undefined) {
            statusLabel.text = plan.error
            return
        }
        pendingPlan = plan
        confirmDialog.open()
    }

    function executePending() {
        if (!pendingPlan)
            return
        var name = pendingPlan.name
        var action = pendingPlan.action
        if (action === "install")
            pluginManager.install(name)
        else if (action === "uninstall")
            pluginManager.uninstall(name)
        else if (action === "update")
            pluginManager.update(name)
        pendingPlan = null
    }

    function statusText(entry) {
        if (!entry.installed)
            return qsTr("Not installed")
        if (entry.updateAvailable)
            return qsTr("Update available")
        if (entry.remoteHead.length > 0)
            return qsTr("Up to date")
        return qsTr("Installed")
    }

    edge: Qt.RightEdge
    width: Overlay.overlay ? Math.min(Overlay.overlay.width * 0.46, 460) : 440
    onClosed: {
        if (keyFocusTarget)
            keyFocusTarget.forceActiveFocus()
    }
    Keys.onPressed: function (event) {
        if (forwardKeyEvent)
            forwardKeyEvent(event)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: qsTr("Plugins")
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: "#e8e8ee"
            }
            Item { Layout.fillWidth: true }
            Button {
                flat: true
                enabled: !pluginManager.busy
                text: qsTr("Check all")
                onClicked: pluginManager.checkAll()
            }
        }

        Label {
            id: statusLabel
            Layout.fillWidth: true
            visible: text.length > 0
            color: "#8a8a96"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            elide: Text.ElideRight
            Connections {
                target: pluginManager
                function onOperationFinished(name, action, success, message) {
                    statusLabel.text = message
                }
                function onErrorOccurred(message) {
                    statusLabel.text = message
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Local customizations are never overwritten: check-mode "
                       + "plugins only compare the remote HEAD with their "
                       + "baseline; replace-mode updates are refused when "
                       + "local modifications are detected. Enabled plugins "
                       + "are appended to the scripts option on the next "
                       + "launch.")
            color: "#8a8a96"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        ListView {
            id: pluginList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: pluginManager.plugins

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
                            width: modeLabel.implicitWidth + 10
                            height: modeLabel.implicitHeight + 4
                            Label {
                                id: modeLabel
                                anchors.centerIn: parent
                                text: modelData.mode
                                font.pixelSize: 10
                                color: "#e8e8ee"
                            }
                        }
                        Rectangle {
                            visible: modelData.locallyModified
                            radius: 4
                            color: "#5a3d1e"
                            width: customizedLabel.implicitWidth + 10
                            height: customizedLabel.implicitHeight + 4
                            Label {
                                id: customizedLabel
                                anchors.centerIn: parent
                                text: qsTr("customized")
                                font.pixelSize: 10
                                color: "#e8e8ee"
                            }
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: pluginDrawer.statusText(modelData)
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

                    Label {
                        Layout.fillWidth: true
                        visible: modelData.baseline.length > 0
                        text: qsTr("Baseline %1 (local customizations are "
                                   + "based on this upstream version)")
                                  .arg(modelData.baseline)
                        color: "#8a8a96"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: modelData.installed
                                 && modelData.head.length > 0
                        text: qsTr("Installed from %1")
                                  .arg(modelData.head.substring(0, 12))
                        color: "#8a8a96"
                        font.pixelSize: 11
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: modelData.isScript
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
                                // Reflect the model, not the flip: the action
                                // is derived from the re-asserted model value,
                                // and setEnabled + changed() update the entry.
                                enableSwitch.checked = Qt.binding(function () {
                                    return modelData.enabled
                                })
                                pluginDrawer.beginOperation(
                                    modelData.name,
                                    enableSwitch.checked ? "disable" : "enable")
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: !modelData.isScript
                        text: qsTr("Shader pack — mounted manually from the "
                                   + "quality panel, not via scripts")
                        color: "#8a8a96"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Button {
                            flat: true
                            visible: !modelData.installed
                            enabled: !pluginManager.busy
                            text: qsTr("Install")
                            onClicked: pluginDrawer.beginOperation(
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
                            onClicked: pluginDrawer.beginOperation(
                                           modelData.name, "update")
                        }
                        Button {
                            flat: true
                            visible: modelData.installed
                            enabled: !pluginManager.busy
                            text: qsTr("Uninstall")
                            onClicked: pluginDrawer.beginOperation(
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
            visible: pluginList.count === 0
            text: qsTr("No plugins in the built-in catalog")
            color: "#8a8a96"
        }
    }

    Dialog {
        id: confirmDialog
        anchors.centerIn: Overlay.overlay
        width: Overlay.overlay ? Math.min(Overlay.overlay.width * 0.62, 460) : 440
        modal: true
        title: pendingPlan ? pendingPlan.title : qsTr("Confirm operation")

        footer: RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Item { Layout.fillWidth: true }
            Button {
                flat: true
                text: qsTr("Cancel")
                onClicked: confirmDialog.reject()
            }
            Button {
                highlighted: true
                text: qsTr("Confirm")
                onClicked: confirmDialog.accept()
            }
        }

        onRejected: pendingPlan = null
        onAccepted: executePending()

        ColumnLayout {
            anchors.fill: parent
            spacing: 4

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#cfcfd8"
                text: {
                    if (!pendingPlan)
                        return ""
                    var lines = pendingPlan.details
                    var out = ""
                    for (var i = 0; i < lines.length; ++i)
                        out += "\u2022 " + lines[i] + "\n"
                    return out
                }
            }
        }
    }
}
