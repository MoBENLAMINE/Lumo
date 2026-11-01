import QtQuick
import QtQuick.Controls.Basic

// A file edit as +/− lines (from Diff::compute).
Rectangle {
    id: view
    property var lines: []
    property int maxHeight: 220

    color: "#0F0F12"
    radius: 8
    border.color: Ui.line
    implicitHeight: Math.min(maxHeight, list.contentHeight + 12)
    clip: true

    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: 6
        model: view.lines
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        delegate: Rectangle {
            required property var modelData
            width: ListView.view.width
            height: row.implicitHeight + 2
            color: modelData.t === "+" ? "#123222" : modelData.t === "-" ? "#3A1618" : "transparent"
            Text {
                id: row
                x: 6
                width: parent.width - 12
                text: modelData.t === "…" ? "⋯" : modelData.t + " " + modelData.s
                color: modelData.t === "+" ? "#7EE2A8" : modelData.t === "-" ? "#FF9A9A" : modelData.t === "…" ? Ui.faint : Ui.dim
                font.family: Ui.mono
                font.pixelSize: 11
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }
        }
    }
}
