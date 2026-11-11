import QtQuick

// An on/off switch.
Item {
    id: t
    property bool on: false
    signal toggled(bool on)
    implicitWidth: 42
    implicitHeight: 24
    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: t.on ? Ui.accent : toggleHover.hovered ? "#3A3A44" : "#2E2E36"
        Behavior on color { ColorAnimation { duration: 150 } }
    }
    Rectangle {
        width: 18; height: 18; radius: 9; y: 3
        x: t.on ? t.width - width - 3 : 3
        color: "white"
        scale: toggleTap.pressed ? 0.9 : 1
        Behavior on x { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 90 } }
    }
    HoverHandler { id: toggleHover; cursorShape: Qt.PointingHandCursor }
    TapHandler { id: toggleTap; onTapped: t.toggled(!t.on) }
}
