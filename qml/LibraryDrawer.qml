import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// P6 media library drawer: folder management (FolderDialog add / remove),
// scan button with progress hint, search filter and the entry list.
//
// Self-contained: it talks only to the libraryStore / libraryModel context
// properties and to mpvEngine. Main.qml supplies the width, focus return on
// close and key forwarding (those need Main.qml ids), like the other drawers.
// v1 lists name + type icon; no thumbnails, no metadata lookup.
Drawer {
    id: libraryDrawer
    edge: Qt.RightEdge

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        Label {
            text: qsTr("Library")
            font.pixelSize: 18
            font.weight: Font.DemiBold
            color: "#e8e8ee"
        }

        // Scan progress: the store scans on its worker thread and reports
        // how many media files it has found so far.
        Label {
            Layout.fillWidth: true
            visible: libraryStore.scanning
            text: qsTr("Scanning… %1 media files found")
                  .arg(libraryStore.scanFileCount)
            color: "#ffb454"
            font.pixelSize: 12
        }

        Label {
            text: qsTr("Folders")
            font.weight: Font.DemiBold
            color: "#cfcfd8"
        }
        Column {
            Layout.fillWidth: true
            spacing: 2
            Repeater {
                model: libraryStore.directories
                delegate: RowLayout {
                    width: parent.width
                    ItemDelegate {
                        Layout.fillWidth: true
                        hoverEnabled: true
                        contentItem: Text {
                            text: modelData
                            color: "#cfcfd8"
                            font.pixelSize: 12
                            elide: Text.ElideMiddle
                            verticalAlignment: Text.AlignVCenter
                        }
                        ToolTip.visible: hovered
                        ToolTip.delay: 600
                        ToolTip.text: modelData
                    }
                    ToolButton {
                        hoverEnabled: true
                        flat: true
                        text: "\u2715"
                        ToolTip.visible: hovered
                        ToolTip.delay: 600
                        ToolTip.text: qsTr("Remove folder")
                        onClicked: libraryStore.removeDirectory(modelData)
                    }
                }
            }
            Label {
                visible: libraryStore.directories.length === 0
                text: qsTr("No folders yet — add one below")
                color: "#8a8a96"
                font.pixelSize: 12
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: qsTr("Add folder…")
                onClicked: folderDialog.open()
            }
            Button {
                text: qsTr("Rescan")
                enabled: !libraryStore.scanning
                onClicked: libraryStore.requestScan()
            }
            Item { Layout.fillWidth: true }
        }

        TextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: qsTr("Search library")
            // Imperative write per the QML state-sync convention (the model
            // never writes the filter back, but the pattern stays uniform).
            onTextChanged: libraryModel.filter = text
        }

        ListView {
            id: entryList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: libraryModel
            delegate: ItemDelegate {
                width: ListView.view.width
                hoverEnabled: true
                contentItem: Text {
                    text: (model.type === "video" ? "\u25B6" : "\u266A")
                          + "  " + model.title
                    color: "#e8e8ee"
                    font.pixelSize: 13
                    elide: Text.ElideMiddle
                    verticalAlignment: Text.AlignVCenter
                }
                ToolTip.visible: hovered
                ToolTip.delay: 600
                ToolTip.text: model.path
                onClicked: mpvEngine.loadFile(model.path)
            }
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        Label {
            visible: entryList.count === 0 && !libraryStore.scanning
            text: libraryStore.directories.length === 0
                  ? qsTr("Add a folder and rescan to fill the library")
                  : qsTr("No media files found")
            color: "#8a8a96"
            font.pixelSize: 12
        }
    }

    // QtQuick.Dialogs gained FolderDialog in Qt 6.3 (qtdeclarative
    // src/quickdialogs2 ships it inside the QtQuick.Dialogs module), so it is
    // available under the project's Qt 6.4.
    FolderDialog {
        id: folderDialog
        title: qsTr("Add library folder")
        onAccepted: libraryStore.addDirectory(selectedFolder)
    }
}
