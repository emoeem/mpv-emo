import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// First-run wizard for the P4 mpv compatibility layer. Shown when a user mpv
// configuration directory was detected and no decision has been recorded.
// Three choices (full import / selective import / standalone) plus a
// "don't ask again" escape hatch; every outcome persists to settings.json
// and the imported engine options take effect on the next launch.
// EmoPlay never writes into the user's mpv directory in any mode.
Dialog {
    id: wizard

    property string mode: "full"
    // relativePath -> checked state for the selective mode checklist.
    property var checkedMap: ({})

    function resetChecklist() {
        var map = {}
        var files = compatController.importFiles
        for (var i = 0; i < files.length; ++i)
            map[files[i].relativePath] = true
        checkedMap = map
    }

    function confirm() {
        if (mode === "full") {
            compatController.chooseFullImport()
        } else if (mode === "standalone") {
            compatController.chooseStandalone()
        } else {
            var selected = []
            for (var path in checkedMap)
                if (checkedMap[path])
                    selected.push(path)
            compatController.chooseSelectiveImport(selected)
        }
        wizard.close()
    }

    anchors.centerIn: Overlay.overlay
    width: Overlay.overlay ? Math.min(Overlay.overlay.width * 0.75, 580) : 560
    modal: true
    title: qsTr("Import mpv configuration")

    onAboutToShow: resetChecklist()

    footer: RowLayout {
        Layout.fillWidth: true
        spacing: 6

        CheckBox {
            id: dontAskAgain
            Layout.leftMargin: 8
            text: qsTr("Don't ask again")
        }
        Item { Layout.fillWidth: true }
        Button {
            flat: true
            text: qsTr("Not now")
            onClicked: {
                if (dontAskAgain.checked)
                    compatController.skipAndDontAskAgain()
                wizard.close()
            }
        }
        Button {
            highlighted: true
            text: wizard.mode === "standalone"
                  ? qsTr("Start standalone")
                  : wizard.mode === "selective"
                    ? qsTr("Import selection")
                    : qsTr("Import everything")
            onClicked: wizard.confirm()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#cfcfd8"
            text: compatController.configDetected
                  ? qsTr("Found an mpv configuration in %1. "
                         "How should EmoPlay use it?")
                    .arg(compatController.sourceDir)
                  : qsTr("No existing mpv configuration was found.")
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            RadioButton {
                text: qsTr("Full import — options, key bindings, scripts, fonts")
                checked: true
                onToggled: wizard.mode = "full"
            }
            RadioButton {
                text: qsTr("Selective import — pick files below")
                onToggled: wizard.mode = "selective"
            }
            RadioButton {
                text: qsTr("Standalone — do not use the existing mpv configuration")
                onToggled: wizard.mode = "standalone"
            }
        }

        // Selective checklist over the scanned files (entry counts from the
        // parser, sizes from the scan).
        ListView {
            id: fileList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            opacity: wizard.mode === "selective" ? 1 : 0.35
            enabled: wizard.mode === "selective"
            model: compatController.importFiles
            delegate: CheckBox {
                width: ListView.view.width
                checked: wizard.checkedMap[modelData.relativePath] === true
                text: modelData.relativePath
                      + "  (" + modelData.kind
                      + (modelData.entryCount > 0
                         ? ", " + modelData.entryCount : "")
                      + ")"
                onToggled: {
                    var map = wizard.checkedMap
                    map[modelData.relativePath] = checked
                    wizard.checkedMap = map
                }
            }
            ScrollIndicator.vertical: ScrollIndicator {}
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: "#8a8a96"
            text: qsTr("Imported settings apply on the next launch. Your mpv "
                       "directory is never modified — EmoPlay only reads from "
                       "it, and writes stay inside its own data folder.")
        }
    }
}
