import QtQuick

// A small rounded button with a press squish.
Rectangle {
    id: b
    property string text
    property color tint: Ui.card
    property color textColor: Ui.text
    property bool primary: false
    property bool checked: false
    property bool enabledLook: true
    signal clicked()

    implicitWidth: label.implicitWidth + 24
    implicitHeight: 28
    radius: height / 2
    color: checked ? Qt.rgba(tint.r, tint.g, tint.b, 0.35)
                   : primary ? tint
                   : mouse.containsMouse ? Ui.cardHover : Ui.card
    border.width: checked ? 1 : 0
    border.color: tint
    opacity: enabledLook ? 1 : 0.45
    scale: mouse.pressed ? 0.94 : 1
    Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }
    Behavior on color { ColorAnimation { duration: 120 } }

    Text {
        id: label
        anchors.centerIn: parent
        text: b.text
        color: b.primary ? "#0A0A0C" : b.textColor
        font.pixelSize: Ui.body
        font.weight: b.primary ? Font.DemiBold : Font.Medium
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        enabled: b.enabledLook
        onClicked: b.clicked()
    }
}
