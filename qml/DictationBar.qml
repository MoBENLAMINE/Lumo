import QtQuick

// The island while you dictate: Lumo listening, nine bars following your
// voice, then the words that were pasted.
Item {
    id: bar
    property bool live: true
    readonly property string st: dictation.state
    readonly property bool recording: st === "listening" || st === "handsfree"
    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 10

        Mascot {
            size: 24
            anchors.verticalCenter: parent.verticalCenter
            color: bar.recording ? "#FF7A7A" : Ui.accent
            mood: bar.st === "transcribing" ? "working" : bar.st === "done" ? "happy" : bar.st === "error" ? "sad" : "waiting"
            alive: bar.live
        }

        // recording: red dot, voice bars, time
        Row {
            visible: bar.recording
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            Rectangle {
                width: 8; height: 8; radius: 4
                color: Ui.bad
                anchors.verticalCenter: parent.verticalCenter
                SequentialAnimation on opacity {
                    running: bar.live && bar.recording
                    loops: Animation.Infinite
                    OpacityAnimator { from: 1; to: 0.3; duration: 600; easing.type: Easing.InOutSine }
                    OpacityAnimator { from: 0.3; to: 1; duration: 600; easing.type: Easing.InOutSine }
                }
            }
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3
                height: 22
                Repeater {
                    model: 9
                    Rectangle {
                        required property int index
                        // the middle bars a touch taller, like a voice
                        readonly property real shape: 1 - Math.abs(index - 4) / 6
                        readonly property real level: dictation.levels[index] || 0
                        width: 3
                        radius: 1.5
                        anchors.verticalCenter: parent.verticalCenter
                        height: 3 + 19 * Math.min(1, level * (0.6 + 0.5 * shape))
                        color: "white"
                        opacity: 0.55 + 0.45 * level
                        Behavior on height { NumberAnimation { duration: 70; easing.type: Easing.OutQuad } }
                    }
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: {
                    const s = Math.floor(dictation.elapsed / 1000)
                    return Math.floor(s / 60) + ":" + String(s % 60).padStart(2, "0")
                }
                color: Ui.dim
                font.pixelSize: Ui.small
                font.family: Ui.mono
            }
            Text {
                visible: bar.st === "handsfree"
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("hands-free")
                color: Ui.warn
                font.pixelSize: Ui.small
            }
            // stop (keep) / cancel
            Text {
                visible: bar.st === "handsfree"
                anchors.verticalCenter: parent.verticalCenter
                text: "✓"; color: Ui.good; font.pixelSize: Ui.title; font.weight: Font.Bold
                TapHandler { onTapped: dictation.toggle() }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "✕"; color: Ui.faint; font.pixelSize: Ui.body
                TapHandler { onTapped: dictation.cancel() }
            }
        }

        Text {
            visible: bar.st === "transcribing"
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Writing it down…")
            color: Ui.dim
            font.pixelSize: Ui.body
        }

        Text {
            visible: bar.st === "done" || bar.st === "error"
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, 380)
            elide: Text.ElideRight
            text: dictation.message || dictation.lastText
            color: bar.st === "error" ? Ui.bad : Ui.text
            font.pixelSize: Ui.body
        }
    }
}
