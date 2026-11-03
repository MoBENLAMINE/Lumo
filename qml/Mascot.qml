import QtQuick
import QtQuick.Shapes

// Lumo: a small glowing companion, a cousin of Claude's own Clawd. A glossy,
// blocky body in the agent's colour with little arms and four stubby legs, a
// soft aura, and a dark screen for a face, where the eyes and mouth are drawn
// in light. Each mood changes the shape and the colour of that light.
//
// Everything is plain scene-graph items moved by Animators, so it stays smooth
// when the GUI thread is busy, and every loop stops when `alive` is false
// (0 % CPU while hidden). Outfits share the same s × s coordinates (Outfit.qml).
Item {
    id: mascot

    property real size: 32
    property color color: "#D97757"
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

    readonly property string face: dizzyTimer.running ? "dizzy" : annoyed && mood !== "happy" ? "annoyed" : mood
    property int pokes: 0
    property bool annoyed: false

    // The light the face is drawn with.
    readonly property color light: face === "working" ? "#8FE3FF"
                                 : face === "alert" ? "#FFC85C"
                                 : face === "sad" ? "#8FB0FF"
                                 : "#EEF8FF"
    readonly property color bodyColor: face === "sad" ? Qt.darker(color, 1.35) : color

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

    // ------------------------------------------------------------ ground shadow
    // Only where Lumo is big enough to stand on something.
    Rectangle {
        visible: mascot.size >= 34
        width: mascot.size * 0.62
        height: Math.max(3, mascot.size * 0.08)
        radius: height / 2
        x: (mascot.width - width) / 2
        y: mascot.height - height * 0.35
        color: "black"
        opacity: 0.32
        // smaller while Lumo is up in the air
        scale: 1 + hop.y / mascot.size
    }

    // ------------------------------------------------------------ body
    Item {
        id: holder
        anchors.fill: parent
        transformOrigin: Item.Bottom

        // aura: Lumo glows in its own colour, brighter when something happens
        Item {
            id: aura
            z: -2
            anchors.fill: parent
            readonly property bool lively: mascot.face === "working" || mascot.face === "alert" || mascot.face === "happy"
            // soft rings of light, fading out: no hard edge
            Repeater {
                model: [[1.42, 0.035], [1.28, 0.05], [1.16, 0.07]]
                Rectangle {
                    required property var modelData
                    anchors.centerIn: parent
                    width: mascot.size * modelData[0]; height: width
                    radius: width / 2
                    color: mascot.bodyColor
                    opacity: modelData[1]
                }
            }
            SequentialAnimation on scale {
                running: mascot.alive && aura.lively
                loops: Animation.Infinite
                onRunningChanged: if (!running) aura.scale = 1
                NumberAnimation { to: 1.08; duration: mascot.face === "alert" ? 420 : 900; easing.type: Easing.InOutSine }
                NumberAnimation { to: 1.0; duration: mascot.face === "alert" ? 420 : 900; easing.type: Easing.InOutSine }
            }
        }

        // what is worn behind the body: a mane, ears
        Outfit {
            z: -1
            s: mascot.size
            name: mascot.outfit
            back: true
            visible: name.length > 0
        }

        // arms: little nubs on both sides
        Repeater {
            model: 2
            Rectangle {
                required property int index
                z: -0.5
                x: (index === 0 ? 0.0 : 0.86) * mascot.size
                y: 0.5 * mascot.size
                width: 0.14 * mascot.size; height: 0.14 * mascot.size
                radius: width * 0.3
                color: Qt.darker(mascot.bodyColor, 1.08)
                antialiasing: true
            }
        }
        // four stubby legs, Clawd-style
        Repeater {
            model: [0.2, 0.34, 0.57, 0.71]
            Rectangle {
                required property var modelData
                z: -0.5
                x: modelData * mascot.size
                y: 0.8 * mascot.size
                width: 0.09 * mascot.size; height: 0.17 * mascot.size
                radius: width * 0.35
                color: Qt.darker(mascot.bodyColor, 1.18)
                antialiasing: true
            }
        }

        Rectangle {
            id: body
            x: mascot.size * 0.06
            y: mascot.size * 0.04
            width: mascot.size * 0.88
            height: mascot.size * 0.82
            radius: width * 0.26
            antialiasing: true
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.lighter(mascot.bodyColor, 1.14) }
                GradientStop { position: 0.55; color: mascot.bodyColor }
                GradientStop { position: 1; color: Qt.darker(mascot.bodyColor, 1.22) }
            }
            border.width: Math.max(1, mascot.size * 0.022)
            border.color: Qt.rgba(1, 1, 1, 0.22)

            // gloss
            Rectangle {
                x: mascot.size * 0.16; y: mascot.size * 0.05
                width: mascot.size * 0.28; height: mascot.size * 0.085
                radius: height / 2
                rotation: -12
                color: "white"
                opacity: 0.32
                antialiasing: true
            }
        }

        // ------------------------------------------------------------ face screen
        Rectangle {
            id: visor
            x: mascot.size * 0.15
            y: mascot.size * 0.26
            width: mascot.size * 0.70
            height: mascot.size * 0.42
            radius: height * 0.46
            antialiasing: true
            gradient: Gradient {
                GradientStop { position: 0; color: mascot.face === "sad" ? "#1A1D2B" : "#1D1D27" }
                GradientStop { position: 1; color: "#0B0B10" }
            }
            border.width: Math.max(1, mascot.size * 0.02)
            border.color: Qt.rgba(0, 0, 0, 0.35)

            // glass reflection along the top
            Rectangle {
                x: parent.width * 0.16; y: parent.height * 0.1
                width: parent.width * 0.5; height: Math.max(1, parent.height * 0.08)
                radius: height / 2
                color: "white"
                opacity: 0.08
            }

            // blush, when happy
            Repeater {
                model: 2
                Rectangle {
                    required property int index
                    width: mascot.size * 0.085; height: mascot.size * 0.045
                    radius: height / 2
                    x: index === 0 ? visor.width * 0.08 : visor.width * 0.92 - width
                    y: visor.height * 0.66
                    color: "#FF8FA3"
                    opacity: mascot.face === "happy" ? 0.6 : 0
                    Behavior on opacity { OpacityAnimator { duration: 200 } }
                }
            }
        }

        // ------------------------------------------------------------ eyes
        Item {
            id: eyes
            anchors.fill: parent

            // Gaze offset in -1..1, from lookAt or the wander timer.
            property real gx: 0
            property real gy: 0
            Behavior on gx { SmoothedAnimation { velocity: 6 } }
            Behavior on gy { SmoothedAnimation { velocity: 6 } }

            function aim() {
                if (mascot.face === "working") return
                if (mascot.lookAt) {
                    const dx = mascot.lookAt.x - mascot.width / 2, dy = mascot.lookAt.y - mascot.height * 0.45
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
                        eyes.gx = eyes.gx > 0 ? -0.8 : 0.8
                        eyes.gy = 0.3
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
                    readonly property real cx: mascot.size * (index === 0 ? 0.355 : 0.645)
                    readonly property real cy: mascot.size * 0.455
                    readonly property real w: mascot.size * 0.115
                    readonly property real h: mascot.size * 0.19
                    x: cx - w / 2 + eyes.gx * mascot.size * 0.045
                    y: cy - h / 2 + eyes.gy * mascot.size * 0.035
                    width: w
                    height: h

                    // the soft light around the eye
                    Rectangle {
                        anchors.centerIn: parent
                        width: eye.w * 2.1; height: eye.h * 1.5
                        radius: width / 2
                        color: mascot.light
                        opacity: mascot.face === "happy" || mascot.face === "dizzy" ? 0 : 0.16
                    }

                    // open eye: a capsule of light
                    Rectangle {
                        id: lens
                        anchors.centerIn: parent
                        visible: mascot.face !== "happy" && mascot.face !== "dizzy"
                        width: mascot.face === "sad" ? eye.w * 1.3 : eye.w
                        height: mascot.face === "annoyed" ? eye.h * 0.26
                              : mascot.face === "sad" ? eye.h * 0.42
                              : mascot.face === "working" ? eye.h * 0.78
                              : eye.h
                        radius: Math.min(width, height) * (mascot.face === "sad" || mascot.face === "annoyed" ? 0.5 : 0.34)
                        rotation: mascot.face === "sad" ? (eye.index === 0 ? -16 : 16) : 0
                        scale: mascot.face === "alert" ? 1.18 : 1
                        color: mascot.light
                        antialiasing: true
                        Behavior on height { NumberAnimation { duration: 140; easing.type: Easing.OutQuad } }
                        Behavior on width { NumberAnimation { duration: 140 } }
                        Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutBack } }
                        Behavior on color { ColorAnimation { duration: 200 } }
                    }

                    // happy eye: ∩
                    Shape {
                        anchors.centerIn: parent
                        width: eye.w * 1.5; height: eye.h * 0.7
                        visible: mascot.face === "happy"
                        preferredRendererType: Shape.CurveRenderer
                        ShapePath {
                            fillColor: "transparent"
                            strokeColor: mascot.light
                            strokeWidth: Math.max(1.4, mascot.size * 0.045)
                            capStyle: ShapePath.RoundCap
                            startX: 0; startY: eye.h * 0.62
                            PathQuad { x: eye.w * 1.5; y: eye.h * 0.62; controlX: eye.w * 0.75; controlY: -eye.h * 0.3 }
                        }
                    }

                    // dizzy eye: a spinning ×
                    Item {
                        anchors.centerIn: parent
                        width: eye.w * 1.6; height: width
                        visible: mascot.face === "dizzy"
                        Repeater {
                            model: 2
                            Rectangle {
                                required property int index
                                anchors.centerIn: parent
                                width: parent.width; height: Math.max(1.4, mascot.size * 0.04)
                                radius: height / 2
                                rotation: index === 0 ? 45 : -45
                                color: mascot.light
                            }
                        }
                        RotationAnimator on rotation {
                            running: mascot.alive && mascot.face === "dizzy"
                            from: eye.index === 0 ? 0 : 360; to: eye.index === 0 ? 360 : 0
                            duration: 700; loops: Animation.Infinite
                        }
                    }
                }
            }

            // blink: squash the eyes vertically, on the render thread
            transform: Scale {
                id: blinkScale
                origin.y: mascot.size * 0.455
                yScale: 1
            }
            SequentialAnimation {
                id: blink
                NumberAnimation { target: blinkScale; property: "yScale"; to: 0.1; duration: 70 }
                NumberAnimation { target: blinkScale; property: "yScale"; to: 1; duration: 110 }
            }
            Timer {
                running: (mascot.alive || mascot.blinks) && mascot.face !== "happy" && mascot.face !== "dizzy"
                repeat: true
                interval: 2600 + Math.random() * 2600
                onTriggered: { interval = 2200 + Math.random() * 3200; blink.restart() }
            }
        }

        // ------------------------------------------------------------ mouth
        Item {
            id: mouth
            readonly property real cx: mascot.size * 0.5
            readonly property real cy: mascot.size * 0.585
            x: cx - width / 2
            y: cy - height / 2
            width: mascot.size * 0.18
            height: mascot.size * 0.08

            // a short line of light
            Rectangle {
                anchors.centerIn: parent
                visible: mascot.face !== "happy" && mascot.face !== "sad" && mascot.face !== "alert" && mascot.face !== "dizzy"
                width: mascot.face === "working" ? mascot.size * 0.06 : mascot.size * 0.09
                height: Math.max(1.2, mascot.size * 0.028)
                radius: height / 2
                color: mascot.light
                opacity: 0.85
                Behavior on width { NumberAnimation { duration: 160 } }
            }
            // surprised / dizzy: a small ring
            Rectangle {
                anchors.centerIn: parent
                visible: mascot.face === "alert" || mascot.face === "dizzy"
                width: mascot.size * 0.075; height: width
                radius: width / 2
                color: "transparent"
                border.width: Math.max(1.2, mascot.size * 0.024)
                border.color: mascot.light
            }
            // smile or frown
            Shape {
                anchors.fill: parent
                visible: mascot.face === "happy" || mascot.face === "sad"
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: mascot.light
                    strokeWidth: Math.max(1.3, mascot.size * 0.032)
                    capStyle: ShapePath.RoundCap
                    readonly property bool smile: mascot.face === "happy"
                    startX: mouth.width * (smile ? 0.1 : 0.28)
                    startY: mouth.height * (smile ? 0.25 : 0.8)
                    PathQuad {
                        x: mouth.width * (mascot.face === "happy" ? 0.9 : 0.72)
                        y: mouth.height * (mascot.face === "happy" ? 0.25 : 0.8)
                        controlX: mouth.width * 0.5
                        controlY: mouth.height * (mascot.face === "happy" ? 1.15 : 0.05)
                    }
                }
            }
        }

        // what is worn on top
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
        gradient: Gradient {
            GradientStop { position: 0; color: "#FFD978" }
            GradientStop { position: 1; color: "#F2A93B" }
        }
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
