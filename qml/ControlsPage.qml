import QtQuick
import QtQuick.Layouts

// Quick toggles and levels: Wi-Fi, Bluetooth, microphone, keep awake, Do Not
// Disturb and the power mode; volume (with the output to use), microphone and
// brightness; the battery on a laptop. Everything is read when the page opens.
ColumnLayout {
    id: page
    property bool live: false
    signal openPage(string name)   // "wifi", "bluetooth", "mixer"
    spacing: 10

    onLiveChanged: controls.live = live
    Component.onDestruction: controls.live = false

    function profileLabel(p) {
        return p === "power-saver" ? qsTr("Power saver") : p === "performance" ? qsTr("Performance") : qsTr("Balanced")
    }
    function nextProfile() {
        const list = controls.powerProfiles
        if (!list.length) return
        controls.setPowerProfile(list[(list.indexOf(controls.powerProfile) + 1) % list.length])
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 3
        columnSpacing: 8
        rowSpacing: 8

        Tile {
            Layout.fillWidth: true
            icon: controls.wifi ? Ui.icons.wifi : Ui.icons.wifiOff
            label: qsTr("Wi-Fi")
            detail: !controls.hasWifi ? qsTr("No Wi-Fi") : controls.wifi ? qsTr("On") : qsTr("Off")
            available: controls.hasWifi
            on: controls.wifi
            more: true
            onToggled: controls.setWifi(!controls.wifi)
            onMoreRequested: page.openPage("wifi")
        }
        Tile {
            Layout.fillWidth: true
            icon: Ui.icons.bluetooth
            label: qsTr("Bluetooth")
            detail: !controls.hasBluetooth ? qsTr("No adapter") : controls.bluetooth ? qsTr("On") : qsTr("Off")
            available: controls.hasBluetooth
            on: controls.bluetooth
            more: true
            onToggled: controls.setBluetooth(!controls.bluetooth)
            onMoreRequested: page.openPage("bluetooth")
        }
        Tile {
            Layout.fillWidth: true
            icon: controls.micMuted ? Ui.icons.micOff : Ui.icons.mic
            label: qsTr("Microphone")
            detail: controls.micMuted ? qsTr("Muted") : qsTr("On")
            available: controls.hasAudio
            on: !controls.micMuted
            onToggled: controls.toggleMicMute()
        }
        Tile {
            Layout.fillWidth: true
            icon: Ui.icons.coffee
            label: qsTr("Keep awake")
            detail: power.keepAwake ? qsTr("Screen stays on") : qsTr("Off")
            on: power.keepAwake
            onToggled: power.keepAwake = !power.keepAwake
        }
        Tile {
            Layout.fillWidth: true
            icon: notifications.doNotDisturb ? Ui.icons.bellOff : Ui.icons.bell
            label: qsTr("Do Not Disturb")
            detail: notifications.doNotDisturb ? qsTr("Pills are quiet") : qsTr("Off")
            on: notifications.doNotDisturb
            onToggled: notifications.doNotDisturb = !notifications.doNotDisturb
        }
        Tile {
            Layout.fillWidth: true
            icon: controls.powerProfile === "power-saver" ? Ui.icons.leaf : controls.powerProfile === "performance" ? Ui.icons.bolt : Ui.icons.gauge
            label: qsTr("Power mode")
            detail: controls.hasPowerProfiles ? page.profileLabel(controls.powerProfile) : qsTr("Not available")
            available: controls.hasPowerProfiles
            on: controls.powerProfile === "power-saver"
            onToggled: page.nextProfile()
        }
    }

    // levels
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: levels.implicitHeight + 20
        radius: 14
        color: Ui.card
        ColumnLayout {
            id: levels
            x: 12; y: 10
            width: parent.width - 24
            spacing: 4
            Level {
                Layout.fillWidth: true
                visible: controls.hasAudio
                icon: controls.muted || controls.volume === 0 ? Ui.icons.volumeOff : Ui.icons.volume
                value: controls.volume
                max: 100
                muted: controls.muted
                onMoved: (v) => controls.setVolume(v)
                onIconClicked: controls.toggleMute()
            }
            Level {
                Layout.fillWidth: true
                visible: controls.hasAudio
                icon: controls.micMuted ? Ui.icons.micOff : Ui.icons.mic
                value: controls.micVolume
                max: 100
                muted: controls.micMuted
                onMoved: (v) => controls.setMicVolume(v)
                onIconClicked: controls.toggleMicMute()
            }
            Level {
                Layout.fillWidth: true
                visible: controls.hasBrightness
                icon: Ui.icons.sun
                iconClickable: false
                value: controls.brightness
                onMoved: (v) => controls.setBrightness(v)
            }
            // where the sound goes
            Flow {
                Layout.fillWidth: true
                Layout.topMargin: 4
                visible: controls.outputs.length > 1
                spacing: 6
                Repeater {
                    model: controls.outputs
                    Rectangle {
                        required property var modelData
                        height: 26; radius: 13
                        width: Math.min(outLabel.implicitWidth + 42, levels.width)
                        color: modelData.current ? Qt.rgba(Ui.accent.r, Ui.accent.g, Ui.accent.b, 0.22) : outHover.hovered ? Ui.cardHover : Ui.line
                        Icon { x: 9; anchors.verticalCenter: parent.verticalCenter; path: Ui.icons.output; size: 13; tint: parent.modelData.current ? Ui.accent : Ui.dim }
                        Text {
                            id: outLabel
                            x: 27; anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 33
                            text: parent.modelData.label
                            elide: Text.ElideRight
                            color: parent.modelData.current ? Ui.text : Ui.dim
                            font.pixelSize: Ui.small
                        }
                        HoverHandler { id: outHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: controls.setOutput(parent.modelData.name) }
                    }
                }
            }
            Text {
                visible: !controls.hasAudio && !controls.hasBrightness
                text: qsTr("No sound server or screen brightness here.")
                color: Ui.dim
                font.pixelSize: Ui.small
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 2
                visible: controls.apps.length > 0
                Text { text: controls.apps.length === 1 ? qsTr("1 app playing sound") : qsTr("%1 apps playing sound").arg(controls.apps.length); color: Ui.dim; font.pixelSize: Ui.small; Layout.fillWidth: true }
                PillButton { text: qsTr("Mixer"); onClicked: page.openPage("mixer") }
            }
        }
    }

    // the battery, on a laptop
    Rectangle {
        Layout.fillWidth: true
        visible: power.hasBattery
        implicitHeight: 56
        radius: 14
        color: Ui.card
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12; anchors.rightMargin: 12
            spacing: 12
            Item {
                implicitWidth: 34; implicitHeight: 18
                Rectangle { anchors.fill: parent; anchors.rightMargin: 3; radius: 4; color: "transparent"; border.color: Ui.dim; border.width: 1.5 }
                Rectangle { x: parent.width - 3; anchors.verticalCenter: parent.verticalCenter; width: 3; height: 7; radius: 1; color: Ui.dim }
                Rectangle {
                    x: 3; y: 3; height: parent.height - 6
                    width: (parent.width - 9) * power.percent / 100
                    radius: 2
                    color: power.percent <= power.lowThreshold && !power.pluggedIn ? Ui.bad : power.charging ? Ui.good : Ui.text
                }
            }
            Column {
                Layout.fillWidth: true
                Text {
                    text: power.percent + "%" + (power.charging ? "  ·  " + qsTr("Charging") : power.pluggedIn ? "  ·  " + qsTr("Plugged in") : "")
                    color: Ui.text; font.pixelSize: Ui.body; font.weight: Font.DemiBold
                }
                Text {
                    text: {
                        const parts = []
                        if (power.minutesLeft > 0)
                            parts.push((power.charging ? qsTr("Full in %1") : qsTr("%1 left")).arg(Math.floor(power.minutesLeft / 60) + ":" + String(power.minutesLeft % 60).padStart(2, "0")))
                        if (Math.abs(power.watts) > 0.1) parts.push(Math.abs(power.watts).toFixed(1) + " W")
                        if (power.capacity > 0) parts.push(qsTr("Health %1%").arg(power.capacity))
                        return parts.join("  ·  ")
                    }
                    color: Ui.dim; font.pixelSize: Ui.small
                }
            }
        }
    }
}
