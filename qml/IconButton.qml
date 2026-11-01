import QtQuick

// A round button with an Icon, and a tooltip below it while hovered.
Rectangle {
    id: b
    property string icon
    property string tip
    property bool active: false
    property bool filled: true
    property color tint: Ui.dim
    property color activeTint: Ui.accent
    property real size: 30
    property real iconSize: 16
    signal clicked()

    implicitWidth: size
    implicitHeight: size
    radius: size / 2
    color: hover.hovered ? (filled ? Ui.cardHover : Ui.line) : filled ? Ui.card : "transparent"
    scale: tap.pressed ? 0.9 : 1
    Behavior on color { ColorAnimation { duration: 120 } }
    Behavior on scale { NumberAnimation { duration: 80 } }

    Icon {
        anchors.centerIn: parent
        size: b.iconSize
        path: b.icon
        tint: b.active ? b.activeTint : hover.hovered ? Ui.text : b.tint
    }
    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { id: tap; onTapped: b.clicked() }

    Rectangle {
        visible: hover.hovered && b.tip.length > 0
        z: 10
        anchors.top: parent.bottom
        anchors.topMargin: 6
        anchors.horizontalCenter: parent.horizontalCenter
        width: tipText.implicitWidth + 16
        height: tipText.implicitHeight + 10
        radius: 6
        color: Ui.cardHover
        border.color: Ui.line
        Text {
            id: tipText
            anchors.centerIn: parent
            text: b.tip
            color: Ui.text
            font.pixelSize: Ui.small
        }
    }
}
