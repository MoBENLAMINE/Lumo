import QtQuick
import QtQuick.Layouts

// The player, bigger: the cover turns over like a card when the song changes,
// the card takes the cover's color, and a soft light swells up from below
// with the music, bass to treble.
ColumnLayout {
    id: page
    property bool live: false
    signal fullscreenRequested()
    spacing: 10

    readonly property color tint: music.hasColor && prefs.albumColors ? music.color : Ui.accent
    property real pos: 0

    Timer {
        running: page.live && music.playing
        interval: 500
        repeat: true
        triggeredOnStart: true
        onTriggered: page.pos = music.position()
    }
    Connections { target: music; function onChanged() { page.pos = music.position() } }

    // The cover turns over on a new song: the old one swings away, the new one in.
    property string shownArt: music.artUrl
    property string shownTitle: music.title
    Connections {
        target: music
        function onChanged() {
            if (music.title === page.shownTitle && music.artUrl === page.shownArt) return
            if (Ui.reduceMotion || !page.live) { page.shownArt = music.artUrl; page.shownTitle = music.title; return }
            flip.restart()
        }
    }
    SequentialAnimation {
        id: flip
        NumberAnimation { target: coverTurn; property: "angle"; to: 90; duration: 180; easing.type: Easing.InQuad }
        ScriptAction { script: { page.shownArt = music.artUrl; page.shownTitle = music.title; coverTurn.angle = -90 } }
        NumberAnimation { target: coverTurn; property: "angle"; to: 0; duration: 260; easing.type: Easing.OutBack }
    }

    Rectangle {
        id: card
        Layout.fillWidth: true
        implicitHeight: 196
        radius: 16
        clip: true
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.rgba(page.tint.r * 0.35, page.tint.g * 0.35, page.tint.b * 0.35, 1) }
            GradientStop { position: 1; color: Ui.card }
        }
        Behavior on implicitHeight { NumberAnimation { duration: 160 } }

        // the light that swells with the music
        Row {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 6
            visible: prefs.visualizer
            Repeater {
                model: 6
                Rectangle {
                    required property int index
                    readonly property real level: beat.bands.length > index && music.playing ? beat.bands[index] : 0
                    anchors.bottom: parent.bottom
                    width: (card.width - 6 * 5) / 6
                    height: 8 + level * 80
                    radius: width / 2
                    opacity: 0.12 + level * 0.3
                    gradient: Gradient {
                        GradientStop { position: 0; color: Qt.rgba(page.tint.r, page.tint.g, page.tint.b, 0) }
                        GradientStop { position: 1; color: page.tint }
                    }
                    Behavior on height { enabled: !Ui.reduceMotion; NumberAnimation { duration: 90 } }
                }
            }
        }

        RowLayout {
            x: 16; y: 16
            width: parent.width - 32
            spacing: 16
            Rectangle {
                id: cover
                implicitWidth: 104; implicitHeight: 104
                radius: 12
                color: Ui.line
                clip: true
                transform: Rotation { id: coverTurn; origin.x: 52; origin.y: 52; axis { x: 0; y: 1; z: 0 } angle: 0 }
                Image {
                    anchors.fill: parent
                    source: page.shownArt
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    sourceSize: Qt.size(208, 208)
                }
                Icon { anchors.centerIn: parent; visible: !page.shownArt; path: Ui.icons.music; tint: Ui.dim; size: 36 }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: page.fullscreenRequested() }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text {
                    Layout.fillWidth: true
                    text: page.shownTitle || qsTr("Nothing playing")
                    color: Ui.text; font.pixelSize: 17; font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: [music.artist, music.player].filter(s => s).join("  ·  ")
                    color: Ui.dim; font.pixelSize: Ui.body
                    elide: Text.ElideRight
                }
                // the seek bar
                Item {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    implicitHeight: 16
                    visible: music.length > 0
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width; height: 4; radius: 2
                        color: Qt.rgba(1, 1, 1, 0.12)
                        Rectangle { width: parent.width * Math.min(1, page.pos / Math.max(1, music.length)); height: 4; radius: 2; color: page.tint }
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: music.canSeek
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: (m) => music.seek(m.x / width * music.length)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: music.length > 0
                    Text { text: page.fmt(page.pos); color: Ui.faint; font.pixelSize: 10; Layout.fillWidth: true }
                    Text { text: page.fmt(music.length); color: Ui.faint; font.pixelSize: 10 }
                }
                RowLayout {
                    spacing: 6
                    IconButton { filled: false; icon: Ui.icons.shuffle; visible: music.hasShuffle; active: music.shuffle; activeTint: page.tint; onClicked: music.toggleShuffle(); tip: qsTr("Shuffle") }
                    IconButton { filled: false; icon: Ui.icons.skipBack; onClicked: music.previous(); tip: qsTr("Previous") }
                    Rectangle {
                        implicitWidth: 40; implicitHeight: 40; radius: 20
                        color: Ui.text
                        scale: playTap.pressed ? 0.92 : 1
                        Icon { anchors.centerIn: parent; path: music.playing ? Ui.icons.pause : Ui.icons.play; tint: "#0A0A0C"; size: 18; stroke: 2.2 }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler { id: playTap; onTapped: music.playPause() }
                    }
                    IconButton { filled: false; icon: Ui.icons.skipForward; onClicked: music.next(); tip: qsTr("Next") }
                    IconButton { filled: false; icon: Ui.icons.repeat; visible: music.hasLoop; active: music.loop !== "None"; activeTint: page.tint; onClicked: music.cycleLoop(); tip: qsTr("Repeat") }
                    Item { Layout.fillWidth: true }
                    IconButton { filled: false; icon: Ui.icons.expand; onClicked: page.fullscreenRequested(); tip: qsTr("Fullscreen") }
                }
            }
        }
    }

    function fmt(ms) {
        const s = Math.floor(ms / 1000)
        return Math.floor(s / 60) + ":" + String(s % 60).padStart(2, "0")
    }
}
