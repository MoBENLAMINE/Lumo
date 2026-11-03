import QtQuick

// Pick Lumo's outfit. Resting on a tile lets you try it on (`trying`).
// "Auto" puts the headphones on while music plays, so its tile wears them.
Flow {
    id: wardrobe
    property string current: "auto"
    property string trying: ""
    property color mascotColor: Ui.accent
    signal picked(string outfit)

    readonly property var outfits: Ui.outfits

    spacing: 8

    Repeater {
        model: wardrobe.outfits
        Rectangle {
            id: tile
            required property var modelData
            readonly property bool chosen: Ui.outfitChoice(wardrobe.current) === modelData.id
            width: 78; height: 82; radius: 12
            color: chosen ? Qt.rgba(Ui.accent.r, Ui.accent.g, Ui.accent.b, 0.18) : tileHover.hovered ? Ui.cardHover : "#121216"
            border.width: chosen ? 1.5 : 1
            border.color: chosen ? Ui.accent : Ui.line
            scale: tileTap.pressed ? 0.95 : 1
            Behavior on scale { NumberAnimation { duration: 80 } }
            Behavior on color { ColorAnimation { duration: 120 } }

            Mascot {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 18
                size: 34
                color: wardrobe.mascotColor
                alive: false
                outfit: tile.modelData.id === "auto" ? "headphones" : tile.modelData.id
                enabled: false
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 6
                text: tile.modelData.label
                color: tile.chosen ? Ui.text : Ui.dim
                font.pixelSize: 10
            }
            HoverHandler {
                id: tileHover
                cursorShape: Qt.PointingHandCursor
                onHoveredChanged: wardrobe.trying = hovered ? tile.modelData.id : (wardrobe.trying === tile.modelData.id ? "" : wardrobe.trying)
            }
            TapHandler { id: tileTap; onTapped: wardrobe.picked(tile.modelData.id) }
        }
    }
}
