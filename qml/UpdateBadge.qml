// P9 update badge — a standalone QML file per the UI convention. It becomes
// visible only when updateChecker (UpdateChecker) confirmed a release newer
// than the running build; every other outcome (up to date, offline, API
// error) keeps it hidden. Clicking opens the release page in the system
// browser — the checker itself never downloads or installs anything.
import QtQuick
import QtQuick.Controls

Button {
    id: badge

    visible: updateChecker.hasUpdate
    flat: true
    hoverEnabled: true
    leftPadding: 8
    rightPadding: 8
    topPadding: 3
    bottomPadding: 3

    contentItem: Text {
        // Explicit concatenation: adjacent string literals are not joined
        // in QML bindings.
        text: "\u2191 " + updateChecker.latestVersion
        font.pixelSize: 11
        color: badge.hovered ? "#ffffff" : "#8fc1ff"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: 6
        border.color: "#4f8cff"
        border.width: 1
        color: badge.hovered ? "#2a3c5e" : "transparent"
    }

    ToolTip.visible: hovered
    ToolTip.delay: 600
    ToolTip.text: qsTr("Version %1 is available — click to open the release page")
        .arg(updateChecker.latestVersion)

    onClicked: {
        if (updateChecker.releaseUrl.toString().length > 0)
            Qt.openUrlExternally(updateChecker.releaseUrl)
    }
}
