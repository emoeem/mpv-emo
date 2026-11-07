import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// P5 v2 ICC display configuration section of the quality drawer: enable
// switch, display profile file picker and the redirected cache location.
//
// Self-contained: it talks only to the iccController context property.
// Values are synced imperatively (Connections + local sync()), per the
// architecture.md QML state-sync convention.
ColumnLayout {
    id: panel
    spacing: 2

    function sync() {
        iccSwitch.checked = iccController.enabled
        if (iccController.profilePath.length > 0)
            pathLabel.text = iccController.profilePath
        else
            pathLabel.text = qsTr("No profile selected")
    }

    Component.onCompleted: sync()

    Connections {
        target: iccController
        function onStateChanged() { panel.sync() }
    }

    Label {
        text: qsTr("ICC display profile")
        font.weight: Font.DemiBold
        color: "#cfcfd8"
    }

    RowLayout {
        Layout.fillWidth: true
        Switch {
            id: iccSwitch
            onToggled: iccController.setEnabled(checked)
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Color-manage playback with a display ICC profile")
            color: "#8a8a96"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
    }

    Label {
        id: pathLabel
        Layout.fillWidth: true
        color: "#cfcfd8"
        font.pixelSize: 12
        elide: Text.ElideMiddle
    }

    Button {
        text: qsTr("Choose profile…")
        onClicked: iccFileDialog.open()
    }

    Label {
        Layout.fillWidth: true
        visible: iccController.failed
        text: iccController.failureHint
        color: "#ffb454"
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }

    Label {
        Layout.fillWidth: true
        text: qsTr("3DLUT cache directory: %1").arg(iccController.cacheDir)
        color: "#8a8a96"
        font.pixelSize: 11
        elide: Text.ElideMiddle
    }

    FileDialog {
        id: iccFileDialog
        title: qsTr("Choose ICC display profile")
        nameFilters: [
            qsTr("ICC profiles (*.icc *.icm)"),
            qsTr("All files (*)")
        ]
        onAccepted: iccController.setProfilePath(iccFileDialog.selectedFile)
    }
}
