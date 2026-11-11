import QtQuick

// A level slider with an icon you can click (mute), for volume, microphone
// and brightness. Drag or click along the bar; the wheel nudges it.
Item {
    id: level
    property string icon
    property real value: 0          // 0..max
    property real max: 100
    property bool muted: false
    property string label
    property bool iconClickable: true
    signal moved(real value)
    signal iconClicked()

    implicitHeight: 36

    Rectangle {
        id: knob
        width: 32; height: 32; radius: 16
        anchors.verticalCenter: parent.verticalCenter
        color: iconHover.hovered && level.iconClickable ? Ui.cardHover : Ui.card
        Icon { anchors.centerIn: parent; path: level.icon; size: 16; tint: level.muted ? Ui.faint : Ui.text }
        HoverHandler { id: iconHover; cursorShape: level.iconClickable ? Qt.PointingHandCursor : Qt.ArrowCursor }
        TapHandler { enabled: level.iconClickable; onTapped: level.iconClicked() }
    }
    Item {
        id: track
        anchors.left: knob.right; anchors.leftMargin: 10
        anchors.right: valueText.left; anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        height: 24
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width; height: 6; radius: 3
            color: Ui.line
            Rectangle {
                width: parent.width * Math.min(1, level.value / level.max); height: parent.height; radius: 3
                color: level.muted ? Ui.faint : Ui.accent
            }
        }
        Rectangle {
            x: track.width * Math.min(1, level.value / level.max) - width / 2
            anchors.verticalCenter: parent.verticalCenter
            width: dragArea.pressed || trackHover.hovered ? 14 : 10; height: width; radius: width / 2
            color: Ui.text
            Behavior on width { NumberAnimation { duration: 100 } }
        }
        HoverHandler { id: trackHover; cursorShape: Qt.PointingHandCursor }
        MouseArea {
            id: dragArea
            anchors.fill: parent
            function set(x) { level.moved(Math.round(Math.max(0, Math.min(1, x / width)) * level.max)) }
            onPressed: (m) => set(m.x)
            onPositionChanged: (m) => { if (pressed) set(m.x) }
            onWheel: (w) => level.moved(Math.max(0, Math.min(level.max, level.value + (w.angleDelta.y > 0 ? 5 : -5))))
        }
    }
    Text {
        id: valueText
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 38
        horizontalAlignment: Text.AlignRight
        text: level.label || Math.round(level.value) + "%"
        color: Ui.dim
        font.pixelSize: Ui.small
    }
}
