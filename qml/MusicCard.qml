import QtQuick
import QtQuick.Layouts

// Now playing, from any MPRIS player: cover, title, a progress bar you can
// click to seek, and previous / play-pause / next.
Rectangle {
    id: card

    component Control: Rectangle {
        id: btn
        property string glyph
        property bool big: false
        signal activated()
        width: big ? 38 : 30; height: width; radius: width / 2
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        color: big ? Ui.text : (btnHover.hovered ? Ui.line : "transparent")
        scale: btnTap.pressed ? 0.9 : 1
        Behavior on scale { NumberAnimation { duration: 80 } }
        Icon { anchors.centerIn: parent; path: btn.glyph; tint: btn.big ? "#0A0A0C" : Ui.text; size: btn.big ? 18 : 15; stroke: btn.big ? 2.2 : 1.8 }
        HoverHandler { id: btnHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { id: btnTap; onTapped: btn.activated() }
    }
    property bool live: true
    property real pos: 0

    implicitHeight: row.implicitHeight + 20
    radius: 14
    color: Ui.card

    Timer {
        running: card.live && music.playing
        interval: 500
        repeat: true
        triggeredOnStart: true
        onTriggered: card.pos = music.position()
    }
    Connections { target: music; function onChanged() { card.pos = music.position() } }

    RowLayout {
        id: row
        x: 10; y: 10
        width: parent.width - 20
        spacing: 12

        Rectangle {
            implicitWidth: 52; implicitHeight: 52; radius: 9
            color: Ui.line
            clip: true
            Image {
                anchors.fill: parent
                source: music.artUrl
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                sourceSize: Qt.size(104, 104)
            }
            Icon { anchors.centerIn: parent; visible: !music.artUrl; path: Ui.icons.music; tint: Ui.dim; size: 24 }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3
            Text {
                Layout.fillWidth: true
                text: music.title
                color: Ui.text
                font.pixelSize: Ui.body
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                text: music.artist ? music.artist + "  ·  " + music.player : music.player
                color: Ui.dim
                font.pixelSize: Ui.small
                elide: Text.ElideRight
            }
            // progress
            Item {
                Layout.fillWidth: true
                implicitHeight: 14
                visible: music.length > 0
                Rectangle {
                    id: track
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width; height: 4; radius: 2
                    color: Ui.line
                    Rectangle {
                        width: parent.width * Math.min(1, card.pos / Math.max(1, music.length))
                        height: parent.height; radius: 2
                        color: Ui.accent
                    }
                }
                TapHandler {
                    enabled: music.canSeek
                    onTapped: (point) => music.seek(Math.round(point.position.x / track.width * music.length))
                }
                HoverHandler { cursorShape: music.canSeek ? Qt.PointingHandCursor : Qt.ArrowCursor }
            }
        }

        Row {
            spacing: 4
            Control { glyph: Ui.icons.skipBack; onActivated: music.previous() }
            Control { glyph: music.playing ? Ui.icons.pause : Ui.icons.play; big: true; onActivated: music.playPause() }
            Control { glyph: Ui.icons.skipForward; onActivated: music.next() }
        }
    }
}
