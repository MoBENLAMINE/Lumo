import QtQuick
import QtQuick.Layouts

// Timer, stopwatch and focus; a color picker, a screenshot and keep awake;
// the shelf; and the session's lock, sleep, log out, restart and shut down
// (the last three are press-and-hold, so a slip can't end your session).
ColumnLayout {
    id: page
    property bool live: false
    property Timers timers
    property string clockTab: "timer"
    spacing: 10

    component Chip: Rectangle {
        id: chip
        property string icon
        property string text
        property bool on: false
        signal clicked()
        implicitHeight: 30
        implicitWidth: chipRow.implicitWidth + 22
        radius: 15
        color: on ? Qt.rgba(Ui.accent.r, Ui.accent.g, Ui.accent.b, 0.22) : chipHover.hovered ? Ui.cardHover : Ui.line
        Row {
            id: chipRow
            anchors.centerIn: parent
            spacing: 6
            Icon { visible: chip.icon.length > 0; path: chip.icon; size: 14; tint: chip.on ? Ui.accent : Ui.text; anchors.verticalCenter: parent.verticalCenter }
            Text { text: chip.text; color: chip.on ? Ui.text : Ui.dim; font.pixelSize: Ui.small; font.weight: chip.on ? Font.DemiBold : Font.Normal; anchors.verticalCenter: parent.verticalCenter }
        }
        HoverHandler { id: chipHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: chip.clicked() }
    }

    // A round action that fires only after being held for a moment.
    component HoldAction: Item {
        id: hold
        property string icon
        property string label
        property bool needsHold: false
        signal fired()
        implicitWidth: 74
        implicitHeight: 64
        property real progress: 0
        NumberAnimation on progress {
            id: holdAnim
            running: false
            from: 0; to: 1; duration: 900
            onFinished: if (hold.progress >= 1) { hold.fired(); hold.progress = 0 }
        }
        Rectangle {
            id: disc
            anchors.horizontalCenter: parent.horizontalCenter
            width: 42; height: 42; radius: 21
            color: holdHover.hovered ? Ui.cardHover : Ui.line
            Rectangle {
                anchors.fill: parent
                radius: 21
                color: "transparent"
                border.width: 2
                border.color: Ui.bad
                opacity: hold.progress
            }
            Icon { anchors.centerIn: parent; path: hold.icon; size: 18; tint: hold.progress > 0 ? Ui.bad : Ui.text }
        }
        Text {
            anchors.top: disc.bottom; anchors.topMargin: 4
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: hold.label
            elide: Text.ElideRight
            color: Ui.dim
            font.pixelSize: 10
        }
        HoverHandler { id: holdHover; cursorShape: Qt.PointingHandCursor }
        TapHandler {
            onPressedChanged: {
                if (!hold.needsHold) { if (!pressed) hold.fired(); return }
                if (pressed) holdAnim.restart()
                else if (hold.progress < 1) { holdAnim.stop(); hold.progress = 0 }
            }
        }
        ToolTipLabel { visible: holdHover.hovered && hold.needsHold; text: qsTr("Hold to confirm") }
    }

    component ToolTipLabel: Rectangle {
        property alias text: tipText.text
        z: 10
        anchors.bottom: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: tipText.implicitWidth + 14; height: tipText.implicitHeight + 8
        radius: 6
        color: "#1E1E26"
        Text { id: tipText; anchors.centerIn: parent; color: Ui.text; font.pixelSize: 11 }
    }

    // ---- timer · stopwatch · focus
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: clockCol.implicitHeight + 24
        radius: 14
        color: Ui.card
        ColumnLayout {
            id: clockCol
            x: 12; y: 12
            width: parent.width - 24
            spacing: 10
            Row {
                spacing: 6
                Chip { icon: Ui.icons.timer; text: qsTr("Timer"); on: page.clockTab === "timer"; onClicked: page.clockTab = "timer" }
                Chip { icon: Ui.icons.stopwatch; text: qsTr("Stopwatch"); on: page.clockTab === "stopwatch"; onClicked: page.clockTab = "stopwatch" }
                Chip { icon: Ui.icons.focus; text: qsTr("Focus"); on: page.clockTab === "focus"; onClicked: page.clockTab = "focus" }
            }

            // timer
            RowLayout {
                visible: page.clockTab === "timer"
                Layout.fillWidth: true
                spacing: 8
                IconButton { icon: Ui.icons.minus; enabled: !page.timers.timerRunning; onClicked: page.timers.setTimer(page.timers.timerSet - 60) }
                Text {
                    text: page.timers.clock(page.timers.timerRemaining)
                    color: Ui.text; font.pixelSize: 30; font.weight: Font.DemiBold
                    Layout.preferredWidth: 110; horizontalAlignment: Text.AlignHCenter
                }
                IconButton { icon: Ui.icons.plus; enabled: !page.timers.timerRunning; onClicked: page.timers.setTimer(page.timers.timerSet + 60) }
                Item { Layout.fillWidth: true }
                Repeater {
                    model: [1, 5, 15, 25]
                    Chip { required property int modelData; text: qsTr("%1 min").arg(modelData); onClicked: { page.timers.resetTimer(); page.timers.setTimer(modelData * 60) } }
                }
            }
            RowLayout {
                visible: page.clockTab === "timer"
                spacing: 8
                PillButton {
                    primary: true; tint: Ui.accent
                    text: page.timers.timerRunning ? qsTr("Pause") : qsTr("Start")
                    onClicked: page.timers.timerRunning ? page.timers.pauseTimer() : page.timers.startTimer()
                }
                PillButton { text: qsTr("Reset"); onClicked: page.timers.resetTimer() }
            }

            // stopwatch
            Text {
                visible: page.clockTab === "stopwatch"
                text: page.timers.watchText(page.timers.watchRunning ? page.timers.now - page.timers.watchStart + page.timers.watchKept : page.timers.watchKept)
                color: Ui.text; font.pixelSize: 30; font.weight: Font.DemiBold
            }
            RowLayout {
                visible: page.clockTab === "stopwatch"
                spacing: 8
                PillButton {
                    primary: true; tint: Ui.accent
                    text: page.timers.watchRunning ? qsTr("Pause") : qsTr("Start")
                    onClicked: page.timers.watchRunning ? page.timers.pauseWatch() : page.timers.startWatch()
                }
                PillButton { text: qsTr("Lap"); enabled: page.timers.watchRunning; onClicked: page.timers.lap() }
                PillButton { text: qsTr("Reset"); onClicked: page.timers.resetWatch() }
            }
            Flow {
                visible: page.clockTab === "stopwatch" && page.timers.laps.length > 0
                Layout.fillWidth: true
                spacing: 12
                Repeater {
                    model: page.timers.laps
                    Text {
                        required property int index
                        required property var modelData
                        text: qsTr("Lap %1").arg(page.timers.laps.length - index) + "  " + page.timers.watchText(modelData)
                        color: index === 0 ? Ui.text : Ui.dim
                        font.pixelSize: Ui.small
                    }
                }
            }

            // focus
            RowLayout {
                visible: page.clockTab === "focus"
                Layout.fillWidth: true
                spacing: 14
                Column {
                    Text {
                        text: page.timers.clock(page.timers.focusRemaining)
                        color: page.timers.onBreak ? Ui.good : Ui.text; font.pixelSize: 30; font.weight: Font.DemiBold
                    }
                    Text {
                        text: !page.timers.focusActive ? qsTr("%1 min focus, %2 min break, %3 rounds").arg(page.timers.focusMinutes).arg(page.timers.breakMinutes).arg(page.timers.rounds)
                            : (page.timers.onBreak ? qsTr("Break") : qsTr("Focus")) + " · " + qsTr("round %1 of %2").arg(page.timers.round).arg(page.timers.rounds)
                        color: Ui.dim; font.pixelSize: Ui.small
                    }
                }
                Item { Layout.fillWidth: true }
                Column {
                    visible: !page.timers.focusActive
                    spacing: 4
                    Row {
                        spacing: 4
                        Text { text: qsTr("Focus"); color: Ui.dim; font.pixelSize: Ui.small; width: 54; anchors.verticalCenter: parent.verticalCenter }
                        IconButton { size: 22; iconSize: 12; icon: Ui.icons.minus; onClicked: page.timers.focusMinutes = Math.max(5, page.timers.focusMinutes - 5) }
                        IconButton { size: 22; iconSize: 12; icon: Ui.icons.plus; onClicked: { page.timers.focusMinutes = Math.min(120, page.timers.focusMinutes + 5); page.timers.focusLeft = page.timers.focusMinutes * 60 } }
                    }
                    Row {
                        spacing: 4
                        Text { text: qsTr("Break"); color: Ui.dim; font.pixelSize: Ui.small; width: 54; anchors.verticalCenter: parent.verticalCenter }
                        IconButton { size: 22; iconSize: 12; icon: Ui.icons.minus; onClicked: page.timers.breakMinutes = Math.max(1, page.timers.breakMinutes - 1) }
                        IconButton { size: 22; iconSize: 12; icon: Ui.icons.plus; onClicked: page.timers.breakMinutes = Math.min(30, page.timers.breakMinutes + 1) }
                    }
                }
            }
            RowLayout {
                visible: page.clockTab === "focus"
                spacing: 8
                PillButton {
                    primary: true; tint: Ui.accent
                    text: page.timers.focusRunning ? qsTr("Pause") : page.timers.focusActive ? qsTr("Resume") : qsTr("Start focus")
                    onClicked: page.timers.focusRunning ? page.timers.pauseFocus() : page.timers.startFocus()
                }
                PillButton { visible: page.timers.focusActive; text: qsTr("Stop"); onClicked: page.timers.stopFocus() }
            }
        }
    }

    // ---- quick tools
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        Tile { Layout.fillWidth: true; icon: Ui.icons.picker; label: qsTr("Color picker"); detail: tools.lastColor || qsTr("Copies the hex code"); onToggled: tools.pickColor() }
        Tile { Layout.fillWidth: true; icon: Ui.icons.camera; label: qsTr("Screenshot"); detail: qsTr("Pick a region"); onToggled: tools.screenshot() }
        Tile { Layout.fillWidth: true; icon: Ui.icons.coffee; label: qsTr("Keep awake"); detail: power.keepAwake ? qsTr("On") : qsTr("Off"); on: power.keepAwake; onToggled: power.keepAwake = !power.keepAwake }
    }

    // ---- the shelf
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: shelfCol.implicitHeight + 20
        radius: 14
        color: Ui.card
        ColumnLayout {
            id: shelfCol
            x: 12; y: 10
            width: parent.width - 24
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Icon { path: Ui.icons.shelf; size: 14; tint: Ui.dim }
                Text { text: qsTr("Shelf"); color: Ui.dim; font.pixelSize: Ui.small; Layout.fillWidth: true }
                Text {
                    visible: tools.shelf.length > 0
                    text: qsTr("Clear")
                    color: clearHover.hovered ? Ui.text : Ui.faint
                    font.pixelSize: Ui.small
                    HoverHandler { id: clearHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: tools.clearShelf() }
                }
            }
            Text {
                visible: tools.shelf.length === 0
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("Drop files or text on the island to keep them here. Drag them out again to drop them anywhere, or click one to copy it.")
                color: Ui.faint
                font.pixelSize: Ui.small
            }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: tools.shelf
                    Rectangle {
                        id: item
                        required property int index
                        required property var modelData
                        width: 100; height: 76; radius: 10
                        color: itemHover.hovered ? Ui.cardHover : Ui.line
                        clip: true
                        Image {
                            anchors.fill: parent
                            visible: item.modelData.isImage
                            source: item.modelData.isImage ? item.modelData.value : ""
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            sourceSize: Qt.size(200, 152)
                            opacity: 0.85
                        }
                        Icon {
                            visible: !item.modelData.isImage
                            x: 10; y: 10
                            path: item.modelData.kind === "text" ? Ui.icons.note : item.modelData.isDir ? Ui.icons.folder : Ui.icons.file
                            size: 20; tint: Ui.dim
                        }
                        Rectangle {
                            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                            height: 24
                            color: Qt.rgba(0, 0, 0, item.modelData.isImage ? 0.55 : 0)
                            Text {
                                anchors.fill: parent; anchors.margins: 6
                                verticalAlignment: Text.AlignVCenter
                                text: item.modelData.name
                                elide: item.modelData.kind === "text" ? Text.ElideRight : Text.ElideMiddle
                                color: Ui.text
                                font.pixelSize: 10
                            }
                        }
                        IconButton {
                            visible: itemHover.hovered
                            anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 4
                            size: 20; iconSize: 10
                            icon: Ui.icons.close
                            onClicked: tools.removeFromShelf(item.index)
                        }
                        HoverHandler { id: itemHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: tools.copyShelfItem(item.index) }
                        TapHandler { acceptedButtons: Qt.LeftButton; onDoubleTapped: tools.openShelfItem(item.index) }
                        // drag it out to drop it anywhere
                        Drag.active: dragHandler.active
                        Drag.dragType: Drag.Automatic
                        Drag.supportedActions: Qt.CopyAction
                        Drag.mimeData: item.modelData.kind === "file" ? { "text/uri-list": item.modelData.value } : { "text/plain": item.modelData.value }
                        DragHandler { id: dragHandler; target: null }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                visible: tools.shelf.length > 0
                Text { text: qsTr("Keep after restart"); color: Ui.dim; font.pixelSize: Ui.small; Layout.fillWidth: true }
                Toggle { on: tools.keepShelf; onToggled: (v) => tools.keepShelf = v }
            }
        }
    }

    // ---- session
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 84
        radius: 14
        color: Ui.card
        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 0
            HoldAction { Layout.fillWidth: true; icon: Ui.icons.lock; label: qsTr("Lock"); onFired: power.lock() }
            HoldAction { Layout.fillWidth: true; icon: Ui.icons.moon; label: qsTr("Sleep"); onFired: power.suspend() }
            HoldAction { Layout.fillWidth: true; icon: Ui.icons.logout; label: qsTr("Log out"); needsHold: true; onFired: power.logout() }
            HoldAction { Layout.fillWidth: true; icon: Ui.icons.restart; label: qsTr("Restart"); needsHold: true; onFired: power.reboot() }
            HoldAction { Layout.fillWidth: true; icon: Ui.icons.power; label: qsTr("Shut down"); needsHold: true; onFired: power.shutdown() }
        }
    }
}
