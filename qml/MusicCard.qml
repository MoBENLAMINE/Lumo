import QtQuick
import QtQuick.Layouts

// Now playing, from any MPRIS player: cover, title, a progress bar you can
// click to seek, previous / play-pause / next, and shuffle, repeat and volume
// when the player offers them.
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
        property bool on: false
        Icon { anchors.centerIn: parent; path: btn.glyph; tint: btn.big ? "#0A0A0C" : btn.on ? Ui.accent : Ui.text; size: btn.big ? 18 : 15; stroke: btn.big ? 2.2 : 1.8 }
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
            // shuffle, repeat, volume
            RowLayout {
                Layout.fillWidth: true
                visible: music.hasShuffle || music.hasLoop || music.hasVolume
                spacing: 2
                Control {
                    visible: music.hasShuffle
                    anchors.verticalCenter: undefined
                    Layout.preferredWidth: 26; Layout.preferredHeight: 26
                    glyph: Ui.icons.shuffle
                    on: music.shuffle
                    onActivated: music.toggleShuffle()
                }
                Control {
                    visible: music.hasLoop
                    anchors.verticalCenter: undefined
                    Layout.preferredWidth: 26; Layout.preferredHeight: 26
                    glyph: Ui.icons.repeat
                    on: music.loop !== "None"
                    onActivated: music.cycleLoop()
                    // repeat one: a small "1"
                    Text {
                        visible: music.loop === "Track"
                        anchors.right: parent.right; anchors.top: parent.top
                        text: "1"; color: Ui.accent; font.pixelSize: 9; font.weight: Font.Bold
                    }
                }
                Item { Layout.fillWidth: true }
                Icon {
                    visible: music.hasVolume
                    path: music.volume > 0 ? Ui.icons.volume : Ui.icons.volumeOff
                    tint: Ui.dim; size: 14
                }
                // volume: click or drag along it
                Item {
                    visible: music.hasVolume
                    implicitWidth: 86; implicitHeight: 18
                    Rectangle {
                        id: volTrack
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width; height: 4; radius: 2
                        color: Ui.line
                        Rectangle { width: parent.width * music.volume; height: parent.height; radius: 2; color: Ui.dim }
                        Rectangle {
                            x: parent.width * music.volume - width / 2
                            anchors.verticalCenter: parent.verticalCenter
                            width: 10; height: 10; radius: 5
                            color: Ui.text
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onPressed: (m) => music.setVolume(m.x / width)
                        onPositionChanged: (m) => { if (pressed) music.setVolume(Math.max(0, Math.min(1, m.x / width))) }
                    }
                }
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
