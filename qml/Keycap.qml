import QtQuick

// A keyboard key, for shortcut hints: Keycap { text: "Meta" }.
Rectangle {
    id: key
    property string text
    property bool small: false
    implicitWidth: Math.max(small ? 20 : 28, keyLabel.implicitWidth + (small ? 10 : 16))
    implicitHeight: small ? 18 : 26
    radius: small ? 4 : 6
    color: Ui.card
    border.color: Ui.line
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 2
        height: 2
        radius: 1
        color: "#0A0A0C"
    }
    Text {
        id: keyLabel
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -1
        text: key.text
        color: key.small ? Ui.dim : Ui.text
        font.pixelSize: key.small ? 10 : Ui.small + 1
        font.weight: Font.DemiBold
    }
}
