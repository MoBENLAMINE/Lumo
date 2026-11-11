import QtQuick

// A quick-toggle tile: an icon, a name and a line under it. Click to toggle;
// right-click (or the arrow) for more.
Rectangle {
    id: tile
    property string icon
    property string label
    property string detail
    property bool on: false
    property bool available: true
    property bool more: false
    signal toggled()
    signal moreRequested()

    implicitWidth: 120
    implicitHeight: 56
    radius: 14
    opacity: available ? 1 : 0.4
    color: on ? Qt.rgba(Ui.accent.r, Ui.accent.g, Ui.accent.b, hover.hovered ? 0.3 : 0.22)
              : hover.hovered ? Ui.cardHover : Ui.card
    border.width: 1
    border.color: on ? Qt.rgba(Ui.accent.r, Ui.accent.g, Ui.accent.b, 0.45) : "transparent"
    scale: tap.pressed ? 0.97 : 1
    Behavior on color { ColorAnimation { duration: 140 } }
    Behavior on scale { NumberAnimation { duration: 80 } }

    Rectangle {
        id: badge
        x: 10; anchors.verticalCenter: parent.verticalCenter
        width: 32; height: 32; radius: 16
        color: tile.on ? Ui.accent : Ui.line
        Icon { anchors.centerIn: parent; path: tile.icon; size: 17; tint: tile.on ? "#0A0A0C" : Ui.text }
    }
    Column {
        anchors.left: badge.right; anchors.leftMargin: 9
        anchors.right: moreIcon.visible ? moreIcon.left : parent.right; anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        spacing: 1
        Text { width: parent.width; text: tile.label; color: Ui.text; font.pixelSize: Ui.body; font.weight: Font.DemiBold; elide: Text.ElideRight }
        Text { width: parent.width; visible: text.length > 0; text: tile.detail; color: Ui.dim; font.pixelSize: Ui.small; elide: Text.ElideRight }
    }
    Icon {
        id: moreIcon
        visible: tile.more
        anchors.right: parent.right; anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        path: Ui.icons.chevronRight; size: 14; tint: Ui.dim
        TapHandler { onTapped: tile.moreRequested() }
    }
    HoverHandler { id: hover; cursorShape: tile.available ? Qt.PointingHandCursor : Qt.ArrowCursor }
    TapHandler { id: tap; enabled: tile.available; onTapped: tile.toggled() }
    TapHandler { acceptedButtons: Qt.RightButton; enabled: tile.more; onTapped: tile.moreRequested() }
}
