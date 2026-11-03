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

    // Music is playing: dance on its beat (Beat.cpp), a different move every
    // two bars, as big as the beat is hard.
    property bool dancing: false
    property string move: "bob"
    property bool pumping: false        // the arm is up, pumping to the beat
    // The pose the next beat moves to.
    property real danceAngle: 0
    property real danceX: 0
    property real danceY: 0
    property real danceSquash: 0
    property real danceArm: 0
    property int beatMs: 500
    readonly property var danceMoves: ["bob", "sway", "step", "hop", "arms", "sway", "step", "bob"]

    Connections {
        target: mascot.dancing && mascot.alive && typeof beat !== "undefined" ? beat : null
        function onBeat(index, strength, period) { mascot.danceBeat(index, strength, period) }
    }
    onDancingChanged: if (!dancing) settle()

    function danceBeat(index, strength, period) {
        if (spin.running || mascot.face === "dizzy")
            return
        // A new move every two bars; soft music gets the gentle ones.
        if (index % 8 === 0) {
            const gentle = strength < 0.4
            let next = move
            for (let tries = 0; tries < 6 && next === move; ++tries) {
                next = danceMoves[Math.floor(Math.random() * danceMoves.length)]
                if (gentle && (next === "hop" || next === "arms")) next = move
            }
            move = next
            // Now and then, a twirl to start the phrase.
            if (index > 0 && index % 16 === 0 && strength > 0.7 && Math.random() < 0.35) {
                spin.restart()
                return
            }
        }
        const a = Math.max(0.35, Math.min(1, strength))
        const side = index % 2 ? 1 : -1
        let angle = 0, x = 0, y = 0, squashBy = 0.1 * a, arm = 0
        switch (move) {
        case "sway": angle = side * 9 * a; squashBy = 0.05 * a; break
        case "step": x = side * 0.07 * size * a; angle = side * 3 * a; squashBy = 0.08 * a; break
        case "hop": y = -size * (strength >= 0.45 ? 0.1 + 0.12 * a : 0.05); squashBy = 0.12 * a; break
        case "arms": arm = side > 0 ? -80 : -20; angle = side * 4 * a; squashBy = 0.07 * a; break
        default: squashBy = 0.13 * a   // bob: a nod of the whole body
        }
        pumping = move === "arms"
        beatMs = Math.max(250, Math.min(900, period))
        danceAngle = angle; danceX = x; danceY = y; danceSquash = squashBy; danceArm = arm
        danceAnim.restart()
    }
    function settle() {
        pumping = false
        danceAngle = 0; danceX = 0; danceY = 0; danceSquash = 0; danceArm = 0
        beatMs = 500
        danceAnim.restart()
    }

    ParallelAnimation {
        id: danceAnim
        NumberAnimation { target: swayTurn; property: "angle"; to: mascot.danceAngle; duration: mascot.beatMs * 0.45; easing.type: Easing.OutBack }
        NumberAnimation { target: groove; property: "x"; to: mascot.danceX; duration: mascot.beatMs * 0.4; easing.type: Easing.OutQuad }
        SequentialAnimation {
            NumberAnimation { target: groove; property: "y"; to: mascot.danceY; duration: mascot.beatMs * 0.35; easing.type: Easing.OutQuad }
            NumberAnimation { target: groove; property: "y"; to: 0; duration: mascot.beatMs * 0.5; easing.type: Easing.OutBounce }
        }
        SequentialAnimation {
            ParallelAnimation {
                NumberAnimation { target: beatScale; property: "yScale"; to: 1 - mascot.danceSquash; duration: mascot.beatMs * 0.18; easing.type: Easing.OutQuad }
                NumberAnimation { target: beatScale; property: "xScale"; to: 1 + mascot.danceSquash * 0.6; duration: mascot.beatMs * 0.18; easing.type: Easing.OutQuad }
            }
            ParallelAnimation {
                NumberAnimation { target: beatScale; property: "yScale"; to: 1; duration: mascot.beatMs * 0.7; easing.type: Easing.OutBack }
                NumberAnimation { target: beatScale; property: "xScale"; to: 1; duration: mascot.beatMs * 0.7; easing.type: Easing.OutBack }
            }
        }
        NumberAnimation { target: waveArm; property: "rotation"; to: mascot.danceArm; duration: mascot.beatMs * 0.4; easing.type: Easing.OutBack }
    }

    // A "!" badge on the corner: something needs you.
    property bool badge: false

    signal poked()

    // Claude finished: a jump with a full spin, then a wink.
    function celebrate() { spin.restart(); winkLater.restart() }
    Timer { id: winkLater; interval: 760; onTriggered: mascot.wink() }

    // A passing expression over the mood: "wink", "love", "yawn".
    property string expression: ""
    function express(name, ms) {
        expression = name
        expressionTimer.interval = ms
        expressionTimer.restart()
    }
    Timer { id: expressionTimer; onTriggered: mascot.expression = "" }
    function wink() { express("wink", 900); winked() }
    signal winked()

    // Rest the pointer on Lumo for a moment and it loves it (when `pettable`).
    property bool pettable: false
    signal petted()
    // Poked too often: it's dizzy for a few seconds.
    signal gotDizzy()
    // Hello: the right arm rises and waves three times.
    function wave() { waveAnim.restart() }
    property bool waving: false

    width: size
    height: size

    // idle | working | thinking | alert | happy | sad | waiting | sleeping,
    // and the passing ones: dizzy, annoyed, wink, love, yawn
    readonly property string face: dizzyTimer.running ? "dizzy" : expression !== "" ? expression
                                 : annoyed && mood !== "happy" ? "annoyed" : mood
    property int pokes: 0
    property bool annoyed: false

    // The light the face is drawn with.
    readonly property color light: face === "working" ? "#8FE3FF"
                                 : face === "thinking" ? "#C4B8FF"
                                 : face === "alert" ? "#FFC85C"
                                 : face === "sad" ? "#8FB0FF"
                                 : face === "love" ? "#FF7AA8"
                                 : face === "sleeping" ? "#9AA9C2"
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
            gotDizzy()
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

        // arms: little nubs on both sides (the right one rises to wave)
        Repeater {
            model: 2
            Rectangle {
                required property int index
                visible: !(index === 1 && (mascot.waving || mascot.pumping))
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

        // the waving arm, pivoting at the body's right edge
        Rectangle {
            id: waveArm
            z: 1
            visible: mascot.waving || mascot.pumping
            x: 0.9 * mascot.size
            y: 0.5 * mascot.size
            width: 0.28 * mascot.size; height: 0.15 * mascot.size
            radius: height * 0.42
            transformOrigin: Item.Left
            rotation: 0
            color: Qt.darker(mascot.bodyColor, 1.08)
            border.width: Math.max(1, mascot.size * 0.015)
            border.color: Qt.rgba(1, 1, 1, 0.18)
            antialiasing: true
        }
        SequentialAnimation {
            id: waveAnim
            PropertyAction { target: mascot; property: "waving"; value: true }
            NumberAnimation { target: waveArm; property: "rotation"; from: 0; to: -55; duration: 220; easing.type: Easing.OutBack }
            SequentialAnimation {
                loops: 3
                NumberAnimation { target: waveArm; property: "rotation"; to: -100; duration: 170; easing.type: Easing.InOutSine }
                NumberAnimation { target: waveArm; property: "rotation"; to: -45; duration: 170; easing.type: Easing.InOutSine }
            }
            NumberAnimation { target: waveArm; property: "rotation"; to: 0; duration: 220; easing.type: Easing.InQuad }
            PropertyAction { target: mascot; property: "waving"; value: false }
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
                running: mascot.alive && (!mascot.lookAt || mascot.face === "working" || mascot.face === "thinking")
                repeat: true
                interval: mascot.face === "working" ? 650 : 1800 + Math.random() * 1800
                onTriggered: {
                    if (mascot.face === "thinking") {
                        eyes.gx = 0.6; eyes.gy = -0.7
                    } else if (mascot.face === "working") {
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
                    // What this eye is drawn as.
                    readonly property string kind: {
                        const f = mascot.face
                        if (f === "wink") return index === 1 ? "arc" : "open"
                        if (f === "happy") return "arc"
                        if (f === "sleeping") return "closed"
                        if (f === "love") return "heart"
                        if (f === "dizzy") return "spiral"
                        if (f === "annoyed") return index === 0 ? "chevronRight" : "chevronLeft"
                        return "open"
                    }
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
                        opacity: eye.kind === "open" ? 0.16 : eye.kind === "heart" ? 0.22 : 0
                    }

                    // open eye: a capsule of light
                    Rectangle {
                        id: lens
                        anchors.centerIn: parent
                        visible: eye.kind === "open"
                        width: mascot.face === "sad" ? eye.w * 1.3 : eye.w
                        height: mascot.face === "yawn" ? eye.h * 0.3
                              : mascot.face === "sad" ? eye.h * 0.42
                              : mascot.face === "working" ? eye.h * 0.78
                              : eye.h
                        radius: Math.min(width, height) * (mascot.face === "sad" || mascot.face === "yawn" ? 0.5 : 0.34)
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
                        visible: eye.kind === "arc"
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

                    // asleep: ‿
                    Shape {
                        anchors.centerIn: parent
                        width: eye.w * 1.5; height: eye.h * 0.5
                        visible: eye.kind === "closed"
                        preferredRendererType: Shape.CurveRenderer
                        ShapePath {
                            fillColor: "transparent"
                            strokeColor: mascot.light
                            strokeWidth: Math.max(1.3, mascot.size * 0.04)
                            capStyle: ShapePath.RoundCap
                            startX: 0; startY: eye.h * 0.1
                            PathQuad { x: eye.w * 1.5; y: eye.h * 0.1; controlX: eye.w * 0.75; controlY: eye.h * 0.55 }
                        }
                    }

                    // annoyed: > <
                    Shape {
                        anchors.centerIn: parent
                        width: eye.w * 1.3; height: eye.h * 0.9
                        visible: eye.kind === "chevronRight" || eye.kind === "chevronLeft"
                        preferredRendererType: Shape.CurveRenderer
                        // ">" points right, "<" points left. (Named plainly: inside a
                        // path, "right" would be the item's anchor line.)
                        ShapePath {
                            fillColor: "transparent"
                            strokeColor: mascot.light
                            strokeWidth: Math.max(1.4, mascot.size * 0.045)
                            capStyle: ShapePath.RoundCap
                            joinStyle: ShapePath.RoundJoin
                            startX: eye.w * 1.3 * (eye.kind === "chevronRight" ? 0.15 : 0.85); startY: eye.h * 0.9 * 0.15
                            PathLine { x: eye.w * 1.3 * (eye.kind === "chevronRight" ? 0.85 : 0.15); y: eye.h * 0.9 * 0.5 }
                            PathLine { x: eye.w * 1.3 * (eye.kind === "chevronRight" ? 0.15 : 0.85); y: eye.h * 0.9 * 0.85 }
                        }
                    }

                    // in love: a heart of light, beating
                    Shape {
                        id: heart
                        readonly property real base: eye.w * 1.7 / 24
                        anchors.centerIn: parent
                        width: 24; height: 24
                        visible: eye.kind === "heart"
                        scale: base
                        preferredRendererType: Shape.CurveRenderer
                        ShapePath {
                            fillColor: mascot.light
                            strokeColor: "transparent"
                            PathSvg { path: "M12 21l-1.5 -1.4c-5.2 -4.7 -8.5 -7.7 -8.5 -11.4c0 -3 2.4 -5.2 5.4 -5.2c1.7 0 3.4 0.8 4.6 2.1c1.2 -1.3 2.9 -2.1 4.6 -2.1c3 0 5.4 2.2 5.4 5.2c0 3.7 -3.3 6.7 -8.5 11.4z" }
                        }
                        SequentialAnimation on scale {
                            running: mascot.face === "love" && mascot.alive
                            loops: Animation.Infinite
                            NumberAnimation { to: heart.base * 1.15; duration: 260; easing.type: Easing.OutQuad }
                            NumberAnimation { to: heart.base; duration: 340; easing.type: Easing.InQuad }
                        }
                    }

                    // dizzy eye: a spinning spiral
                    Item {
                        anchors.centerIn: parent
                        width: eye.w * 1.7; height: width
                        visible: eye.kind === "spiral"
                        Shape {
                            anchors.fill: parent
                            preferredRendererType: Shape.CurveRenderer
                            ShapePath {
                                fillColor: "transparent"
                                strokeColor: mascot.light
                                strokeWidth: Math.max(1.2, mascot.size * 0.03)
                                capStyle: ShapePath.RoundCap
                                PathPolyline {
                                    path: {
                                        const pts = [], c = eye.w * 0.85, turns = 2.2
                                        for (let i = 0; i <= 44; i++) {
                                            const t = i / 44, a = t * turns * 2 * Math.PI, r = t * c * 0.95
                                            pts.push(Qt.point(c + r * Math.cos(a), c + r * Math.sin(a)))
                                        }
                                        return pts
                                    }
                                }
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
                running: (mascot.alive || mascot.blinks) && ["happy", "dizzy", "sleeping", "love", "wink", "annoyed"].indexOf(mascot.face) < 0
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
                visible: ["happy", "sad", "alert", "dizzy", "love", "wink", "yawn", "thinking"].indexOf(mascot.face) < 0
                width: mascot.face === "working" || mascot.face === "sleeping" ? mascot.size * 0.06 : mascot.size * 0.09
                height: Math.max(1.2, mascot.size * 0.028)
                radius: height / 2
                color: mascot.light
                opacity: 0.85
                Behavior on width { NumberAnimation { duration: 160 } }
            }
            // surprised / dizzy: a small ring; a yawn: a big one
            Rectangle {
                anchors.centerIn: parent
                visible: mascot.face === "alert" || mascot.face === "dizzy" || mascot.face === "yawn"
                width: mascot.size * (mascot.face === "yawn" ? 0.12 : 0.075); height: width * (mascot.face === "yawn" ? 1.15 : 1)
                radius: width / 2
                color: "transparent"
                border.width: Math.max(1.2, mascot.size * 0.024)
                border.color: mascot.light
            }
            // thinking: three dots, one after the other
            Row {
                anchors.centerIn: parent
                visible: mascot.face === "thinking"
                spacing: mascot.size * 0.025
                Repeater {
                    model: 3
                    Rectangle {
                        id: dot
                        required property int index
                        width: Math.max(1.6, mascot.size * 0.03); height: width; radius: width / 2
                        color: mascot.light
                        opacity: 0.35
                        SequentialAnimation on opacity {
                            running: mascot.alive && mascot.face === "thinking"
                            loops: Animation.Infinite
                            PauseAnimation { duration: dot.index * 180 }
                            NumberAnimation { to: 1; duration: 220 }
                            NumberAnimation { to: 0.35; duration: 380 }
                            PauseAnimation { duration: (2 - dot.index) * 180 }
                        }
                    }
                }
            }
            // smile or frown
            Shape {
                anchors.fill: parent
                visible: ["happy", "sad", "love", "wink"].indexOf(mascot.face) >= 0
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: mascot.light
                    strokeWidth: Math.max(1.3, mascot.size * 0.032)
                    capStyle: ShapePath.RoundCap
                    readonly property bool smile: mascot.face !== "sad"
                    startX: mouth.width * (smile ? 0.1 : 0.28)
                    startY: mouth.height * (smile ? 0.25 : 0.8)
                    PathQuad {
                        x: mouth.width * (mascot.face !== "sad" ? 0.9 : 0.72)
                        y: mouth.height * (mascot.face !== "sad" ? 0.25 : 0.8)
                        controlX: mouth.width * 0.5
                        controlY: mouth.height * (mascot.face !== "sad" ? 1.15 : 0.05)
                    }
                }
            }
        }

        // asleep: a few z's drift up, then stay (no endless loop while resting)
        Item {
            id: zz
            z: 6
            visible: mascot.face === "sleeping"
            x: mascot.size * 0.78; y: -mascot.size * 0.1
            Repeater {
                model: 3
                Text {
                    required property int index
                    x: index * mascot.size * 0.12
                    y: -index * mascot.size * 0.16
                    text: "z"
                    color: "#C9D6EE"
                    font.pixelSize: Math.max(7, mascot.size * (0.16 + index * 0.05))
                    font.weight: Font.Bold
                    opacity: 0.85 - index * 0.2
                }
            }
            SequentialAnimation on y {
                running: zz.visible
                loops: 4
                NumberAnimation { from: -mascot.size * 0.02; to: -mascot.size * 0.14; duration: 1400; easing.type: Easing.InOutSine }
                NumberAnimation { to: -mascot.size * 0.02; duration: 1400; easing.type: Easing.InOutSine }
            }
        }

        // in love: two hearts float up and fade
        Repeater {
            model: 2
            Text {
                id: floatHeart
                required property int index
                z: 6
                text: "♥"
                color: "#FF7AA8"
                font.pixelSize: Math.max(8, mascot.size * 0.22)
                x: mascot.size * (index === 0 ? 0.12 : 0.74)
                y: 0
                opacity: 0
                Connections { target: mascot; function onPetted() { floatUp.restart() } }
                ParallelAnimation {
                    id: floatUp
                    NumberAnimation { target: floatHeart; property: "y"; from: mascot.size * 0.15; to: -mascot.size * 0.35; duration: 1200 + floatHeart.index * 200; easing.type: Easing.OutQuad }
                    SequentialAnimation {
                        NumberAnimation { target: floatHeart; property: "opacity"; to: 0.95; duration: 150 }
                        PauseAnimation { duration: 600 }
                        NumberAnimation { target: floatHeart; property: "opacity"; to: 0; duration: 500 }
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
        Scale { id: squashScale; origin.x: mascot.width / 2; origin.y: mascot.height; xScale: 1; yScale: 1 },
        // dancing: a step, a lean from the feet, a nod
        Translate { id: groove; x: 0; y: 0 },
        Rotation { id: swayTurn; origin.x: mascot.width / 2; origin.y: mascot.height; angle: 0 },
        Scale { id: beatScale; origin.x: mascot.width / 2; origin.y: mascot.height; xScale: 1; yScale: 1 }
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

    // petting: rest the pointer on Lumo for a moment
    HoverHandler { id: petHover; enabled: mascot.pettable }
    Timer {
        running: petHover.hovered && mascot.pettable && !dizzyTimer.running && mascot.expression === ""
        interval: 1400
        onTriggered: { mascot.express("love", 2000); mascot.petted() }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: (mouse) => mouse.button === Qt.RightButton ? mascot.wardrobeRequested() : mascot.poke()
    }
}
