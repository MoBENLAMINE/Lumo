import QtQuick

// Lumo: a soft squircle with big eyes. Everything is plain scene-graph items
// moved by Animators, so it stays smooth even when the GUI thread is busy,
// and every loop stops when `alive` is false (0 % CPU while hidden).
Item {
    id: mascot

    property real size: 32
    property color color: "#E8875F"
    // idle | working | alert | happy | sad | waiting | dizzy
    property string mood: "idle"
    property bool alive: true
    // Blink now and then even when not alive: a few frames every few seconds.
    property bool blinks: false
    // Where to look, in Lumo's own coordinates; null lets the eyes wander.
    property var lookAt: null

    // What Lumo wears (see Outfit.qml); "" for nothing.
    property string outfit: ""
    signal wardrobeRequested()

    // Music is playing: sway to it.
    property bool dancing: false
    SequentialAnimation on rotation {
        running: mascot.dancing
        loops: Animation.Infinite
        onRunningChanged: if (!running) mascot.rotation = 0
        RotationAnimator { from: 0; to: 9; duration: 240; easing.type: Easing.OutSine }
        RotationAnimator { from: 9; to: -9; duration: 480; easing.type: Easing.InOutSine }
        RotationAnimator { from: -9; to: 0; duration: 240; easing.type: Easing.InSine }
    }

    // A "!" badge on the corner: something needs you.
    property bool badge: false

    signal poked()

    // Claude finished: a jump with a full spin.
    function celebrate() { spin.restart() }

    width: size
    height: size

    readonly property string face: dizzyTimer.running ? "dizzy" : mood
    property int pokes: 0
    property bool annoyed: false

    function poke() {
        pokes += 1
        pokeReset.restart()
        squash.restart()
        annoyed = true
        annoyedTimer.restart()
        if (pokes >= 5) {
            pokes = 0
            dizzyTimer.restart()
        }
        poked()
    }

    Timer { id: pokeReset; interval: 2500; onTriggered: mascot.pokes = 0 }
    Timer { id: annoyedTimer; interval: 1600; onTriggered: mascot.annoyed = false }
    Timer { id: dizzyTimer; interval: 2600 }

    // ------------------------------------------------------------ body
    Item {
        id: holder
        anchors.fill: parent
        transformOrigin: Item.Bottom

        Rectangle {
            id: body
            anchors.fill: parent
            anchors.margins: mascot.size * 0.04
            radius: width * 0.38
            antialiasing: true
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.lighter(mascot.face === "sad" ? Qt.darker(mascot.color, 1.4) : mascot.color, 1.18) }
                GradientStop { position: 1; color: mascot.face === "sad" ? Qt.darker(mascot.color, 1.5) : mascot.color }
            }
            Behavior on radius { NumberAnimation { duration: 200 } }

            // cheeks
            Rectangle {
                width: mascot.size * 0.14; height: width * 0.6; radius: height / 2
                color: "#FF8FA3"; opacity: mascot.face === "happy" ? 0.7 : 0.25
                x: mascot.size * 0.1; y: mascot.size * 0.58
                Behavior on opacity { OpacityAnimator { duration: 200 } }
            }
            Rectangle {
                width: mascot.size * 0.14; height: width * 0.6; radius: height / 2
                color: "#FF8FA3"; opacity: mascot.face === "happy" ? 0.7 : 0.25
                x: body.width - width - mascot.size * 0.1; y: mascot.size * 0.58
                Behavior on opacity { OpacityAnimator { duration: 200 } }
            }
        }

        // ------------------------------------------------------------ eyes
        Item {
            id: eyes
            width: mascot.size * 0.6
            height: mascot.size * 0.32
            anchors.horizontalCenter: parent.horizontalCenter
            y: mascot.size * 0.28

            // Pupil offset in -1..1, from lookAt or the wander timer.
            property real gx: 0
            property real gy: 0
            Behavior on gx { SmoothedAnimation { velocity: 6 } }
            Behavior on gy { SmoothedAnimation { velocity: 6 } }

            function aim() {
                if (mascot.face === "working") return
                if (mascot.lookAt) {
                    const dx = mascot.lookAt.x - mascot.width / 2, dy = mascot.lookAt.y - mascot.height * 0.42
                    const d = Math.max(1, Math.sqrt(dx * dx + dy * dy))
                    const k = Math.min(1, d / (mascot.size * 2.5))
                    gx = dx / d * k
                    gy = dy / d * k
                }
            }
            Connections { target: mascot; function onLookAtChanged() { eyes.aim() } }

            Timer {
                // Wander, or read left-to-right while working.
                running: mascot.alive && (!mascot.lookAt || mascot.face === "working")
                repeat: true
                interval: mascot.face === "working" ? 650 : 1800 + Math.random() * 1800
                onTriggered: {
                    if (mascot.face === "working") {
                        eyes.gx = eyes.gx > 0 ? -0.7 : 0.7
                        eyes.gy = 0.45
                    } else if (mascot.face === "waiting" || mascot.face === "alert") {
                        eyes.gx = 0; eyes.gy = 0.15
                    } else {
                        eyes.gx = Math.random() * 1.6 - 0.8
                        eyes.gy = Math.random() * 1.0 - 0.4
                    }
                }
            }

            Repeater {
                model: 2
                Item {
                    id: eye
                    required property int index
                    width: eyes.width * 0.36
                    height: eyes.height
                    x: index === 0 ? 0 : eyes.width - width

                    // open eye
                    Rectangle {
                        id: white
                        anchors.fill: parent
                        radius: width / 2
                        color: "white"
                        antialiasing: true
                        visible: mascot.face !== "happy"
                        transformOrigin: Item.Center
                        scale: 1

                        Rectangle {
                            id: pupil
                            width: parent.width * 0.58
                            height: width * 1.1
                            radius: width / 2
                            color: "#141418"
                            antialiasing: true
                            x: (parent.width - width) / 2 + eyes.gx * (parent.width - width) * 0.45
                            y: (parent.height - height) / 2 + eyes.gy * (parent.height - height) * 0.45

                            Rectangle { // glint
                                width: parent.width * 0.32; height: width; radius: width / 2
                                color: "white"; x: parent.width * 0.52; y: parent.height * 0.14
                            }
                            RotationAnimator on rotation {
                                running: mascot.alive && mascot.face === "dizzy"
                                from: eye.index === 0 ? 0 : 360; to: eye.index === 0 ? 360 : 0
                                duration: 500; loops: Animation.Infinite
                            }
                            transform: Translate {
                                x: mascot.face === "dizzy" ? (eye.index === 0 ? 1 : -1) * white.width * 0.12 : 0
                            }
                        }

                        // eyelid, for blinks and annoyance
                        Rectangle {
                            width: parent.width + 2
                            x: -1
                            height: mascot.annoyed || mascot.face === "sad" ? parent.height * 0.42 : 0
                            color: Qt.lighter(mascot.color, 1.12)
                            Behavior on height { NumberAnimation { duration: 140; easing.type: Easing.OutQuad } }
                        }
                    }

                    // happy eye: ∩
                    Item {
                        anchors.fill: parent
                        visible: mascot.face === "happy"
                        clip: true
                        Rectangle {
                            width: parent.width; height: width
                            y: parent.height * 0.2
                            radius: width / 2
                            color: "transparent"
                            border.color: "#141418"
                            border.width: Math.max(2, mascot.size * 0.06)
                            antialiasing: true
                        }
                        Rectangle { // hide the lower half of the ring
                            width: parent.width; height: parent.height
                            y: parent.height * 0.2 + parent.width / 2
                            color: Qt.lighter(mascot.color, 1.05)
                        }
                    }
                }
            }

            // blink: squash both eyes vertically, on the render thread
            transform: Scale {
                id: blinkScale
                origin.y: eyes.height / 2
                yScale: 1
            }
            SequentialAnimation {
                id: blink
                NumberAnimation { target: blinkScale; property: "yScale"; to: 0.08; duration: 70 }
                NumberAnimation { target: blinkScale; property: "yScale"; to: 1; duration: 110 }
            }
            Timer {
                running: (mascot.alive || mascot.blinks) && mascot.face !== "happy" && mascot.face !== "dizzy"
                repeat: true
                interval: 2600 + Math.random() * 2600
                onTriggered: { interval = 2200 + Math.random() * 3200; blink.restart() }
            }
        }

        // mouth
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: mascot.size * 0.66
            width: mascot.face === "alert" ? mascot.size * 0.14 : mascot.face === "happy" ? mascot.size * 0.22 : mascot.size * 0.12
            height: mascot.face === "alert" ? width : mascot.face === "happy" ? mascot.size * 0.1 : mascot.size * 0.04
            radius: mascot.face === "happy" ? height / 2 : width / 2
            color: "#141418"
            opacity: 0.85
            Behavior on width { NumberAnimation { duration: 160 } }
            Behavior on height { NumberAnimation { duration: 160 } }
        }

        Outfit {
            z: 5
            s: mascot.size
            name: mascot.outfit
            visible: name.length > 0
        }

        // ------------------------------------------------------------ motion
        // breathing
        SequentialAnimation on scale {
            running: mascot.alive && mascot.face !== "dizzy"
            loops: Animation.Infinite
            NumberAnimation { to: 1.035; duration: 1400; easing.type: Easing.InOutSine }
            NumberAnimation { to: 1.0; duration: 1400; easing.type: Easing.InOutSine }
        }
        // dizzy wobble
        SequentialAnimation on rotation {
            running: mascot.alive && mascot.face === "dizzy"
            loops: Animation.Infinite
            NumberAnimation { to: 9; duration: 180; easing.type: Easing.InOutSine }
            NumberAnimation { to: -9; duration: 360; easing.type: Easing.InOutSine }
            NumberAnimation { to: 0; duration: 180; easing.type: Easing.InOutSine }
        }
    }

    // hops: happy jump and alert bounce
    transform: [
        Translate { id: hop; y: 0 },
        Scale { id: squashScale; origin.x: mascot.width / 2; origin.y: mascot.height; xScale: 1; yScale: 1 }
    ]
    SequentialAnimation {
        running: mascot.alive && (mascot.face === "happy" || mascot.face === "alert")
        loops: mascot.face === "alert" ? Animation.Infinite : 2
        NumberAnimation { target: hop; property: "y"; to: -mascot.size * (mascot.face === "alert" ? 0.12 : 0.25); duration: 170; easing.type: Easing.OutQuad }
        NumberAnimation { target: hop; property: "y"; to: 0; duration: 220; easing.type: Easing.OutBounce }
        PauseAnimation { duration: mascot.face === "alert" ? 500 : 120 }
    }
    SequentialAnimation {
        id: squash
        ParallelAnimation {
            NumberAnimation { target: squashScale; property: "xScale"; to: 1.18; duration: 80 }
            NumberAnimation { target: squashScale; property: "yScale"; to: 0.8; duration: 80 }
        }
        ParallelAnimation {
            SpringAnimation { target: squashScale; property: "xScale"; to: 1; spring: 5; damping: 0.18; epsilon: 0.005 }
            SpringAnimation { target: squashScale; property: "yScale"; to: 1; spring: 5; damping: 0.18; epsilon: 0.005 }
        }
    }

    SequentialAnimation {
        id: spin
        ParallelAnimation {
            NumberAnimation { target: holder; property: "rotation"; from: 0; to: 360; duration: 620; easing.type: Easing.OutBack }
            SequentialAnimation {
                NumberAnimation { target: hop; property: "y"; to: -mascot.size * 0.35; duration: 220; easing.type: Easing.OutQuad }
                NumberAnimation { target: hop; property: "y"; to: 0; duration: 380; easing.type: Easing.OutBounce }
            }
        }
        PropertyAction { target: holder; property: "rotation"; value: 0 }
    }

    Rectangle {
        id: badgeDot
        width: Math.max(9, mascot.size * 0.42)
        height: width
        radius: width / 2
        x: mascot.width - width * 0.7
        y: -height * 0.25
        color: "#F5C451"
        border.color: "#0A0A0C"
        border.width: 1.5
        scale: mascot.badge ? 1 : 0
        visible: scale > 0
        Behavior on scale { NumberAnimation { duration: 260; easing.type: Easing.OutBack } }
        Text {
            anchors.centerIn: parent
            text: "!"
            color: "#0A0A0C"
            font.pixelSize: parent.width * 0.75
            font.weight: Font.Black
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: (mouse) => mouse.button === Qt.RightButton ? mascot.wardrobeRequested() : mascot.poke()
    }
}
