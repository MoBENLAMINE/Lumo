import QtQuick

// Hands a text to Instinct through you (WhatsApp, or the clipboard).
Text {
    id: b
    property string payload
    property string done: ""
    text: done || qsTr("↗ Send to Instinct")
    color: done ? Ui.good : (hover.hovered ? Ui.text : Ui.info)
    font.pixelSize: 10
    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: { b.done = shell.sendToInstinct(b.payload); reset.restart() } }
    Timer { id: reset; interval: 4000; onTriggered: b.done = "" }
}
