import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Window

// The player, filling the screen: the cover over a blurred copy of itself, a
// glow that moves with the music, and the song's lyrics lighting up line by
// line (click a line to go there). Esc, a right-click or the cover leaves.
// Space plays or pauses, ← and → seek, [ and ] nudge the lyrics.
Window {
    id: fs
    title: qsTr("Lumo — Now playing")
    color: "#000000"
    flags: Qt.Window | Qt.FramelessWindowHint
    visible: false

    readonly property color tint: music.hasColor ? music.color : Ui.accent
    property real pos: 0
    property bool idle: false
    readonly property int current: lyrics.state === "synced" ? lyrics.lineAt(pos) : -1

    function open() {
        lyrics.setSong(music.title, music.artist, music.length)
        lyrics.active = true
        fs.showFullScreen()
        fs.requestActivate()
        idleTimer.restart()
    }
    function leave() {
        lyrics.active = false
        fs.close()
    }
    onVisibleChanged: if (!visible) lyrics.active = false

    Connections {
        target: music
        enabled: fs.visible
        function onChanged() { lyrics.setSong(music.title, music.artist, music.length); fs.pos = music.position() }
    }
    Timer {
        running: fs.visible && music.playing
        interval: 200
        repeat: true
        triggeredOnStart: true
        onTriggered: fs.pos = music.position()
    }
    // the controls fade while the mouse rests
    Timer { id: idleTimer; interval: 2500; onTriggered: fs.idle = true }

    Item {
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: fs.leave()
        Keys.onSpacePressed: music.playPause()
        Keys.onLeftPressed: music.seek(Math.max(0, fs.pos - 5000))
        Keys.onRightPressed: music.seek(Math.min(music.length, fs.pos + 5000))
        Keys.onPressed: (e) => {
            if (e.text === "[") { lyrics.offset -= 250; e.accepted = true }
            else if (e.text === "]") { lyrics.offset += 250; e.accepted = true }
        }

        // the cover, blurred, behind everything
        Image {
            id: backdrop
            anchors.fill: parent
            source: music.artUrl
            fillMode: Image.PreserveAspectCrop
            sourceSize: Qt.size(256, 256)
            visible: false
        }
        MultiEffect {
            anchors.fill: parent
            source: backdrop
            visible: music.artUrl.length > 0
            blurEnabled: true
            blur: 1.0
            blurMax: 64
            brightness: -0.45
            saturation: 0.2
        }
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0.25) }
                GradientStop { position: 1; color: Qt.rgba(fs.tint.r * 0.25, fs.tint.g * 0.25, fs.tint.b * 0.25, 0.85) }
            }
        }

        // the glow that moves with the music
        Row {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            visible: prefs.visualizer
            spacing: 18
            Repeater {
                model: 6
                Rectangle {
                    required property int index
                    readonly property real level: beat.bands.length > index && music.playing ? beat.bands[index] : 0
                    anchors.bottom: parent.bottom
                    width: (fs.width - 5 * 18) / 6
                    height: 40 + level * fs.height * 0.22
                    radius: width / 2
                    opacity: 0.06 + level * 0.18
                    gradient: Gradient {
                        GradientStop { position: 0; color: Qt.rgba(fs.tint.r, fs.tint.g, fs.tint.b, 0) }
                        GradientStop { position: 1; color: fs.tint }
                    }
                    Behavior on height { enabled: !Ui.reduceMotion; NumberAnimation { duration: 120 } }
                }
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: Math.min(fs.width, fs.height) * 0.08
            spacing: fs.width * 0.05

            // cover, song, controls
            ColumnLayout {
                readonly property bool withLyrics: lyrics.state === "synced" || lyrics.state === "plain" || lyrics.state === "looking"
                Layout.preferredWidth: withLyrics ? fs.width * 0.34 : fs.width * 0.84
                Layout.maximumWidth: Layout.preferredWidth
                Layout.fillWidth: !withLyrics
                Layout.fillHeight: true
                spacing: 18
                Item { Layout.fillHeight: true }
                Rectangle {
                    readonly property real side: Math.min(fs.height * 0.46, fs.width * 0.32)
                    Layout.preferredWidth: side
                    Layout.preferredHeight: side
                    Layout.alignment: Qt.AlignHCenter
                    radius: 18
                    color: Ui.line
                    clip: true
                    Image { anchors.fill: parent; source: music.artUrl; fillMode: Image.PreserveAspectCrop; sourceSize: Qt.size(800, 800); asynchronous: true }
                    Icon { anchors.centerIn: parent; visible: !music.artUrl; path: Ui.icons.music; size: 96; tint: Ui.dim }
                    TapHandler { onTapped: fs.leave() }
                }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: music.title
                    color: "white"; font.pixelSize: Math.max(22, fs.height * 0.034); font.weight: Font.Bold
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: music.artist
                    color: Qt.rgba(1, 1, 1, 0.7); font.pixelSize: Math.max(15, fs.height * 0.02)
                    elide: Text.ElideRight
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    opacity: fs.idle ? 0 : 1
                    Behavior on opacity { NumberAnimation { duration: 400 } }
                    Item {
                        Layout.fillWidth: true
                        implicitHeight: 14
                        visible: music.length > 0
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width; height: 5; radius: 2.5
                            color: Qt.rgba(1, 1, 1, 0.2)
                            Rectangle { width: parent.width * Math.min(1, fs.pos / Math.max(1, music.length)); height: 5; radius: 2.5; color: "white" }
                        }
                        MouseArea {
                            anchors.fill: parent
                            enabled: music.canSeek
                            cursorShape: Qt.PointingHandCursor
                            onClicked: (m) => music.seek(m.x / width * music.length)
                        }
                    }
                    Row {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 18
                        IconButton { size: 46; iconSize: 22; filled: false; tint: "white"; icon: Ui.icons.skipBack; onClicked: music.previous() }
                        Rectangle {
                            width: 64; height: 64; radius: 32
                            color: "white"
                            Icon { anchors.centerIn: parent; path: music.playing ? Ui.icons.pause : Ui.icons.play; tint: "#0A0A0C"; size: 26; stroke: 2.4 }
                            TapHandler { onTapped: music.playPause() }
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                        }
                        IconButton { size: 46; iconSize: 22; filled: false; tint: "white"; icon: Ui.icons.skipForward; onClicked: music.next() }
                    }
                }
                Item { Layout.fillHeight: true }
            }

            // the lyrics: the line playing bright, the others soft
            Item {
                Layout.fillWidth: true
                Layout.preferredWidth: fs.width * 0.5
                Layout.fillHeight: true
                visible: lyrics.state === "synced" || lyrics.state === "plain" || lyrics.state === "looking"
                ListView {
                    id: lines
                    anchors.fill: parent
                    visible: lyrics.state === "synced"
                    model: lyrics.lines
                    spacing: fs.height * 0.022
                    clip: true
                    interactive: true
                    preferredHighlightBegin: height * 0.38
                    preferredHighlightEnd: height * 0.38
                    highlightRangeMode: ListView.ApplyRange
                    highlightMoveDuration: Ui.reduceMotion ? 0 : 450
                    currentIndex: Math.max(0, fs.current)
                    delegate: Text {
                        required property int index
                        required property var modelData
                        readonly property int distance: Math.abs(index - fs.current)
                        width: lines.width
                        wrapMode: Text.Wrap
                        text: modelData.text || "♪"
                        color: "white"
                        opacity: index === fs.current ? 1 : Math.max(0.18, 0.55 - distance * 0.09)
                        font.pixelSize: Math.max(22, fs.height * 0.036) * (index === fs.current ? 1.06 : 1)
                        font.weight: Font.Bold
                        Behavior on opacity { NumberAnimation { duration: 300 } }
                        TapHandler { onTapped: music.seek(Math.max(0, modelData.time - lyrics.offset)) }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                    }
                }
                Flickable {
                    anchors.fill: parent
                    visible: lyrics.state === "plain"
                    contentHeight: plainText.implicitHeight
                    clip: true
                    Text {
                        id: plainText
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: lyrics.plain
                        color: Qt.rgba(1, 1, 1, 0.8)
                        font.pixelSize: Math.max(18, fs.height * 0.026)
                        lineHeight: 1.3
                    }
                }
                Text {
                    anchors.centerIn: parent
                    visible: lyrics.state === "looking"
                    text: qsTr("Finding the lyrics…")
                    color: Qt.rgba(1, 1, 1, 0.6)
                    font.pixelSize: 18
                }
            }
        }

        // top right: the lyrics nudge, and leave
        Row {
            anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 24
            spacing: 10
            opacity: fs.idle ? 0 : 1
            Behavior on opacity { NumberAnimation { duration: 400 } }
            Text {
                visible: lyrics.offset !== 0
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Lyrics %1 s").arg((lyrics.offset > 0 ? "+" : "") + (lyrics.offset / 1000).toFixed(2))
                color: Qt.rgba(1, 1, 1, 0.6); font.pixelSize: 13
            }
            IconButton { size: 38; iconSize: 18; icon: Ui.icons.lyrics; tip: lyrics.enabled ? qsTr("Hide lyrics") : qsTr("Show lyrics"); active: lyrics.enabled; activeTint: "white"; onClicked: lyrics.enabled = !lyrics.enabled }
            IconButton { size: 38; iconSize: 18; icon: Ui.icons.close; tip: qsTr("Leave (Esc)"); onClicked: fs.leave() }
        }

        // any move of the mouse brings the controls back; a right-click leaves
        MouseArea {
            anchors.fill: parent
            z: -1
            hoverEnabled: true
            acceptedButtons: Qt.RightButton
            onPositionChanged: { fs.idle = false; idleTimer.restart() }
            onClicked: fs.leave()
        }
    }
}
