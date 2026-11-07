import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// P5 v2 frame interpolation section of the quality drawer: enable switch,
// VapourSynth MEMC preset picker and the honest-availability hints.
//
// Self-contained: it talks only to the interpolationController context
// property. Values are synced imperatively (Connections + local sync()),
// per the architecture.md QML state-sync convention — declarative
// bindings would be destroyed by the first user interaction.
ColumnLayout {
    id: panel
    spacing: 2

    function syncSwitch() {
        interpSwitch.checked = interpolationController.enabled
    }

    function syncPreset() {
        for (var i = 0; i < presetCombo.count; ++i) {
            if (presetCombo.valueAt(i) === interpolationController.preset) {
                presetCombo.currentIndex = i
                return
            }
        }
        presetCombo.currentIndex = 0
    }

    // Stable block-reason ids from InterpolationDecision::blockToString.
    function blockText(reason) {
        if (reason === "fps-unknown")
            return qsTr("Waiting for media parameters — will engage once "
                        + "the file's fps is known")
        if (reason === "high-fps")
            return qsTr("Not applied: the source is already high frame rate")
        if (reason === "hdr")
            return qsTr("Not applied: HDR sources are excluded for safety")
        if (reason === "script-missing")
            return qsTr("Script file missing — place the preset's .vpy "
                        + "script in the directory below")
        if (reason === "foreign-vapoursynth")
            return qsTr("Not applied: your imported config already runs a "
                        + "vapoursynth filter")
        return ""
    }

    Component.onCompleted: {
        syncSwitch()
        syncPreset()
    }

    Connections {
        target: interpolationController
        function onStateChanged() {
            panel.syncSwitch()
            panel.syncPreset()
        }
    }

    Label {
        text: qsTr("Frame interpolation (VapourSynth)")
        font.weight: Font.DemiBold
        color: "#cfcfd8"
    }

    RowLayout {
        Layout.fillWidth: true
        Switch {
            id: interpSwitch
            onToggled: interpolationController.setEnabled(checked)
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Motion-compensated frame interpolation via an "
                       + "external VapourSynth script")
            color: "#8a8a96"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
    }

    Label {
        text: qsTr("Preset")
        font.weight: Font.DemiBold
        color: "#cfcfd8"
    }
    ComboBox {
        id: presetCombo
        Layout.fillWidth: true
        enabled: interpSwitch.checked
        textRole: "label"
        valueRole: "value"
        // Preset ids mirror the documented vs/ MEMC presets; the matching
        // .vpy script must exist in the scripts directory shown below.
        model: [
            { label: "MEMC_RIFE_STD", value: "MEMC_RIFE_STD" },
            { label: "MEMC_RIFE_NV", value: "MEMC_RIFE_NV" },
            { label: "MEMC_RIFE_DML", value: "MEMC_RIFE_DML" },
            { label: "MEMC_SVP_PRO", value: "MEMC_SVP_PRO" },
            { label: "MEMC_MVT_LQ", value: "MEMC_MVT_LQ" },
            { label: "MEMC_DRBA_NV", value: "MEMC_DRBA_NV" }
        ]
        onActivated: function (index) {
            interpolationController.setPreset(presetCombo.valueAt(index))
        }
    }

    Label {
        Layout.fillWidth: true
        text: interpolationController.expectedScriptPath
        color: "#8a8a96"
        font.pixelSize: 11
        elide: Text.ElideMiddle
    }
    Label {
        Layout.fillWidth: true
        text: qsTr("Scripts directory: %1").arg(
                  interpolationController.scriptsDir)
        color: "#8a8a96"
        font.pixelSize: 11
        elide: Text.ElideMiddle
    }

    // The honest external-dependency hints: one for the engine capability,
    // one for the current media/script admission state.
    Label {
        Layout.fillWidth: true
        visible: interpolationController.vsUnavailable
        text: qsTr("Requires system vapoursynth support — this libmpv "
                   + "build rejected the vapoursynth filter, so "
                   + "interpolation was switched off")
        color: "#ffb454"
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }
    Label {
        Layout.fillWidth: true
        visible: !interpolationController.vsUnavailable
                 && panel.blockText(interpolationController.blockReason)
                    .length > 0
        text: panel.blockText(interpolationController.blockReason)
        color: "#ffb454"
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }
}
