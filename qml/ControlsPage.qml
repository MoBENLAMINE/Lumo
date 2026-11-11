import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

// Quick toggles and levels: Wi-Fi, Bluetooth, microphone, keep awake, Do Not
// Disturb and the power mode; volume (with the output to use), microphone and
// brightness; the battery on a laptop. Everything is read when the page opens.
ColumnLayout {
    id: page
    property bool live: false
    signal openPage(string name)
    // a control center over the page: "wifi", "bluetooth" or "mixer"
    property string sheet: ""
    spacing: 10
    onSheetChanged: {
        if (sheet === "bluetooth") controls.readDevices()
        else controls.setScanning(false)
        if (sheet === "wifi") controls.scanNetworks()
    }

    onLiveChanged: controls.live = live
    Component.onDestruction: { controls.setScanning(false); controls.live = false }

    function profileLabel(p) {
        return p === "power-saver" ? qsTr("Power saver") : p === "performance" ? qsTr("Performance") : qsTr("Balanced")
    }
    function nextProfile() {
        const list = controls.powerProfiles
        if (!list.length) return
        controls.setPowerProfile(list[(list.indexOf(controls.powerProfile) + 1) % list.length])
    }

    GridLayout {
        visible: page.sheet === ""
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
            onMoreRequested: page.sheet = "wifi"
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
            onMoreRequested: page.sheet = "bluetooth"
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
        visible: page.sheet === ""
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
                PillButton { text: qsTr("Mixer"); onClicked: page.sheet = "mixer" }
            }
        }
    }

    // the battery, on a laptop
    Rectangle {
        Layout.fillWidth: true
        visible: power.hasBattery && page.sheet === ""
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

    // ---- the control centers: Wi-Fi networks, Bluetooth devices, the mixer
    Rectangle {
        visible: page.sheet !== ""
        Layout.fillWidth: true
        implicitHeight: sheetCol.implicitHeight + 24
        radius: 14
        color: Ui.card
        ColumnLayout {
            id: sheetCol
            x: 12; y: 12
            width: parent.width - 24
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                IconButton { icon: "M15 6l-6 6l6 6"; tip: qsTr("Back"); onClicked: page.sheet = "" }
                Text {
                    Layout.fillWidth: true
                    text: page.sheet === "wifi" ? qsTr("Wi-Fi networks") : page.sheet === "bluetooth" ? qsTr("Bluetooth devices") : qsTr("Mixer")
                    color: Ui.text; font.pixelSize: Ui.title; font.weight: Font.DemiBold
                }
                Toggle {
                    visible: page.sheet === "wifi" || page.sheet === "bluetooth"
                    on: page.sheet === "wifi" ? controls.wifi : controls.bluetooth
                    onToggled: (v) => page.sheet === "wifi" ? controls.setWifi(v) : controls.setBluetooth(v)
                }
                PillButton {
                    visible: page.sheet === "bluetooth" && controls.bluetooth
                    text: controls.scanning ? qsTr("Searching…") : qsTr("Search")
                    onClicked: controls.setScanning(!controls.scanning)
                }
                PillButton {
                    visible: page.sheet === "wifi" && controls.wifi
                    text: qsTr("Refresh")
                    onClicked: controls.scanNetworks()
                }
            }

            // Wi-Fi
            Repeater {
                model: page.sheet === "wifi" && controls.wifi ? controls.networks : []
                ColumnLayout {
                    id: net
                    required property var modelData
                    property bool asking: false
                    Layout.fillWidth: true
                    spacing: 6
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 40
                        radius: 10
                        color: netHover.hovered ? Ui.cardHover : Ui.line
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10; anchors.rightMargin: 8
                            spacing: 10
                            // signal: the arcs that reach
                            Item {
                                implicitWidth: 18; implicitHeight: 18
                                Icon { anchors.centerIn: parent; path: Ui.icons.wifi; size: 18; tint: Ui.faint }
                                Icon {
                                    anchors.centerIn: parent; size: 18
                                    tint: net.modelData.active ? Ui.accent : Ui.text
                                    path: net.modelData.strength > 70 ? Ui.icons.wifi
                                        : net.modelData.strength > 45 ? "M5.5 12.5a9.5 9.5 0 0 1 13 0 M8.5 16a5 5 0 0 1 7 0 M12 19.5v0.01"
                                        : net.modelData.strength > 20 ? "M8.5 16a5 5 0 0 1 7 0 M12 19.5v0.01" : "M12 19.5v0.01"
                                }
                            }
                            Text { Layout.fillWidth: true; text: net.modelData.ssid; elide: Text.ElideRight; color: Ui.text; font.pixelSize: Ui.body; font.weight: net.modelData.active ? Font.DemiBold : Font.Normal }
                            Icon { visible: net.modelData.secured; path: Ui.icons.lock; size: 13; tint: Ui.dim }
                            Text {
                                visible: net.modelData.active || controls.wifiBusy === net.modelData.ssid
                                text: controls.wifiBusy === net.modelData.ssid ? qsTr("Joining…") : qsTr("Connected")
                                color: Ui.good; font.pixelSize: Ui.small
                            }
                            IconButton {
                                visible: net.modelData.saved && !net.modelData.active
                                size: 24; iconSize: 12; icon: Ui.icons.trash; tip: qsTr("Forget")
                                onClicked: controls.forgetNetwork(net.modelData.ssid)
                            }
                        }
                        HoverHandler { id: netHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            onTapped: {
                                if (net.modelData.active) controls.disconnectWifi()
                                else if (net.modelData.secured && !net.modelData.saved) net.asking = !net.asking
                                else controls.joinNetwork(net.modelData.ssid, "")
                            }
                        }
                    }
                    RowLayout {
                        visible: net.asking
                        Layout.fillWidth: true
                        spacing: 8
                        TextField {
                            id: pass
                            Layout.fillWidth: true
                            echoMode: TextInput.Password
                            placeholderText: qsTr("Password")
                            color: Ui.text
                            font.pixelSize: Ui.body
                            background: Rectangle { radius: 8; color: Ui.island; border.color: pass.activeFocus ? Ui.accent : Ui.line }
                            onAccepted: { controls.joinNetwork(net.modelData.ssid, text); net.asking = false; text = "" }
                            onVisibleChanged: if (visible) { shell.setKeyboard(true); forceActiveFocus() }
                        }
                        PillButton {
                            primary: true; tint: Ui.accent
                            text: qsTr("Join")
                            onClicked: { controls.joinNetwork(net.modelData.ssid, pass.text); net.asking = false; pass.text = "" }
                        }
                    }
                }
            }
            Text {
                visible: page.sheet === "wifi" && controls.wifiError.length > 0
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: controls.wifiError
                color: Ui.bad; font.pixelSize: Ui.small
            }
            Text {
                visible: page.sheet === "wifi" && (!controls.wifi || controls.networks.length === 0)
                text: controls.wifi ? qsTr("Looking for networks…") : qsTr("Wi-Fi is off.")
                color: Ui.dim; font.pixelSize: Ui.small
            }

            // Bluetooth: connected ones first, with their battery
            Repeater {
                model: page.sheet === "bluetooth" && controls.bluetooth ? controls.devices : []
                Rectangle {
                    id: dev
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 44
                    radius: 10
                    color: devHover.hovered ? Ui.cardHover : Ui.line
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10; anchors.rightMargin: 8
                        spacing: 10
                        Rectangle {
                            implicitWidth: 28; implicitHeight: 28; radius: 14
                            color: dev.modelData.connected ? Ui.accent : Ui.card
                            Icon {
                                anchors.centerIn: parent; size: 15
                                tint: dev.modelData.connected ? "#0A0A0C" : Ui.text
                                path: dev.modelData.icon.indexOf("audio") >= 0 ? Ui.icons.volume
                                    : dev.modelData.icon.indexOf("input") >= 0 ? Ui.icons.tools
                                    : dev.modelData.icon.indexOf("phone") >= 0 ? Ui.icons.chat : Ui.icons.bluetooth
                            }
                        }
                        Column {
                            Layout.fillWidth: true
                            Text { width: parent.width; text: dev.modelData.name; elide: Text.ElideRight; color: Ui.text; font.pixelSize: Ui.body }
                            Text {
                                text: (dev.modelData.connected ? qsTr("Connected") : dev.modelData.paired ? qsTr("Paired") : qsTr("New"))
                                      + (dev.modelData.battery >= 0 ? "  ·  " + dev.modelData.battery + "%" : "")
                                color: dev.modelData.connected ? Ui.good : Ui.dim; font.pixelSize: Ui.small
                            }
                        }
                        PillButton {
                            text: dev.modelData.connected ? qsTr("Disconnect") : dev.modelData.paired ? qsTr("Connect") : qsTr("Pair")
                            onClicked: dev.modelData.connected ? controls.disconnectDevice(dev.modelData.path) : controls.connectDevice(dev.modelData.path)
                        }
                        IconButton {
                            visible: dev.modelData.paired
                            size: 24; iconSize: 12; icon: Ui.icons.trash; tip: qsTr("Forget")
                            onClicked: controls.forgetDevice(dev.modelData.path)
                        }
                    }
                    HoverHandler { id: devHover }
                }
            }
            Text {
                visible: page.sheet === "bluetooth" && (!controls.bluetooth || controls.devices.length === 0)
                text: controls.bluetooth ? qsTr("No devices yet. Search to find new ones nearby.") : qsTr("Bluetooth is off.")
                color: Ui.dim; font.pixelSize: Ui.small
            }

            // mixer: each app making sound
            Level {
                visible: page.sheet === "mixer"
                Layout.fillWidth: true
                icon: controls.muted ? Ui.icons.volumeOff : Ui.icons.volume
                value: controls.volume
                muted: controls.muted
                label: qsTr("All")
                onMoved: (v) => controls.setVolume(v)
                onIconClicked: controls.toggleMute()
            }
            Repeater {
                model: page.sheet === "mixer" ? controls.apps : []
                ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 0
                    Text { text: modelData.name || qsTr("Unknown app"); color: Ui.dim; font.pixelSize: Ui.small; Layout.leftMargin: 42 }
                    Level {
                        Layout.fillWidth: true
                        icon: modelData.muted ? Ui.icons.volumeOff : Ui.icons.music
                        value: modelData.volume
                        muted: modelData.muted
                        onMoved: (v) => controls.setAppVolume(modelData.id, v)
                        onIconClicked: controls.toggleAppMute(modelData.id)
                    }
                }
            }
            Text {
                visible: page.sheet === "mixer" && controls.apps.length === 0
                text: qsTr("No app is making sound right now.")
                color: Ui.dim; font.pixelSize: Ui.small
            }
        }
    }
}
