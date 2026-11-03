import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window

// The island: a black notch-like shape at the top of the screen.
//   rest    — nothing going on: the island at its usual size, Lumo dozing; always there
//   hidden  — only when you hid it (the eye button): retracted into the screen
//             edge; a thin strip at the top still wakes it
//   compact — a pill with Lumo and what the agents are doing
//   open    — sessions, approvals, questions and chat
Window {
    id: win
    width: 760
    // Room for a tall island: it may grow to nearly the screen's height.
    height: Math.max(680, Math.min(Screen.height - 20, 1100))
    readonly property real maxOpenH: Math.max(Ui.maxOpenHeight, height - 40)
    visible: false
    color: "transparent"
    title: "Lumo"

    // ------------------------------------------------------------ state
    property bool peek: false
    property bool hoverOpen: false
    property bool pinned: false
    property string tab: "agents"

    readonly property bool hasRequests: hub.requests.count > 0
    // What the island shows: the agents' mood, or a preview from Settings.
    property string previewMood: ""
    readonly property string mood: previewMood || hub.mood
    // A session stopped and waits for your next prompt: show it for a while.
    property bool callingYou: false
    readonly property bool activeMood: win.mood === "working" || win.mood === "alert" || win.mood === "happy" || win.mood === "sad"
    readonly property bool dictating: dictation.state !== "idle"
    readonly property bool resting: shell.alwaysShow
    readonly property bool musicShown: shell.showMusic && music.available && (music.playing || hub.sessions.count === 0)
    readonly property bool dancing: shell.dance && shell.showMusic && music.playing
    // What Lumo wears: your pick, or the season's (headphones while music plays).
    readonly property string outfit: Ui.outfitFor(shell.outfit, music.playing && shell.showMusic)
    property bool showWardrobe: false
    function openWardrobe() { showWardrobe = true; openIsland("agents", true) }
    readonly property string mode: dictating ? "dictation"
                                 : (hoverOpen || pinned || hasRequests) ? "open"
                                 : greeting ? "greeting"
                                 : (activeMood || callingYou || (peek && !resting)) ? "compact"
                                 : resting ? "rest" : "hidden"
    // The window only takes the pointer inside the island (its input region),
    // so "hovered anywhere in the window" is "on the island" — buttons, cards
    // and the chat field included. An open popup (the model menu) counts too.
    readonly property bool pointerIn: rootHover.hovered || dropArea.containsDrag || chatPane.menuOpen

    function openIsland(which, pin) {
        if (which) tab = which
        pinned = pin
        hoverOpen = true
        if (tab === "chat") focusChat()
    }
    function closeIsland() {
        showWardrobe = false
        pinned = false
        hoverOpen = false
        peek = false
        shell.setKeyboard(false)
    }
    function focusChat() {
        shell.setKeyboard(true)
        Qt.callLater(() => chatPane.input.forceActiveFocus())
    }

    onPointerInChanged: {
        if (pointerIn) {
            closeTimer.stop()
            peek = true
            if (mode !== "open") openTimer.restart()
        } else {
            openTimer.stop()
            closeTimer.restart()
        }
    }
    onModeChanged: {
        updateMask()
        if (mode === "open") { sounds.play("pop"); planUsage.refresh(60000) }
        if (mode !== "open") shell.setKeyboard(false)
    }
    onTabChanged: if (tab === "chat" && mode === "open") focusChat()
    // After the shell has set up the surface, so the pill takes the pointer at once.
    Component.onCompleted: { Qt.callLater(updateMask); greetAtStart.start() }

    // ------------------------------------------------------------ greeting
    // Lumo says hello when it starts: the island opens, light bursts out of it,
    // Lumo pops up, smiles and waves, then the island goes back to what it was.
    // A request or a dictation comes first; a click skips it.
    property bool greeting: false
    property int greetBurst: 0
    function greet() { greetFx.restart() }
    function endGreeting() { greetFx.stop(); sounds.stop("greet"); helloMascot.mood = "idle"; greeting = false }
    Timer { id: greetAtStart; interval: 600; onTriggered: if (shell.greeting) win.greet() }
    SequentialAnimation {
        id: greetFx
        // The "greet" sound follows the whole greeting: change a step here, change Sounds.cpp too.
        ScriptAction { script: { helloMascot.scale = 0; helloMascot.opacity = 0; helloMascot.mood = "idle"; win.greeting = true; sounds.play("greet") } }
        // let the island open first
        PauseAnimation { duration: 280 }
        ScriptAction { script: win.greetBurst++ }
        ParallelAnimation {
            NumberAnimation { target: helloMascot; property: "scale"; from: 0.2; to: 1; duration: 520; easing.type: Easing.OutBack; easing.overshoot: 2 }
            NumberAnimation { target: helloMascot; property: "opacity"; from: 0; to: 1; duration: 180 }
        }
        PauseAnimation { duration: 150 }
        ScriptAction { script: helloMascot.mood = "happy" }
        PauseAnimation { duration: 850 }
        ScriptAction { script: { helloMascot.mood = "idle"; helloMascot.wave() } }
        PauseAnimation { duration: 1500 }
        ScriptAction { script: win.greeting = false }
    }

    Timer { id: openTimer; interval: 260; onTriggered: win.hoverOpen = true }
    Timer {
        id: closeTimer
        interval: 600
        onTriggered: {
            // Typing in the chat keeps it open; so does an explicit pin.
            if (win.pinned || (win.tab === "chat" && chatPane.input.activeFocus && chatPane.input.text.length > 0)) return
            win.hoverOpen = false
            win.peek = false
        }
    }

    Connections {
        target: shell
        function onToggleRequested() { win.mode === "open" ? win.closeIsland() : win.openIsland("agents", true) }
        function onChatRequested() { win.openIsland("chat", true) }
        function onSettingsRequested() { settings.show(); settings.raise(); settings.requestActivate() }
        function onResurfaced() { win.updateMask() }
    }
    Connections {
        target: hub
        function onSessionFinished() { win.finished() }
        function onAttention() { win.needsYou() }
        function onWaitingForYou() { win.callingYou = true; callTimer.restart(); win.needsYou() }
        function onPromptSubmitted() { win.callingYou = false }
    }

    // ------------------------------------------------------------ reactions
    // Claude finished: green ripple, sparkles, Lumo spins. Claude needs you:
    // the island shakes, amber ripples, Lumo wears a "!".
    property real shakeX: 0
    property int burst: 0
    Timer { id: callTimer; interval: 8000; onTriggered: win.callingYou = false }
    Timer { id: previewTimer; interval: 4500; onTriggered: win.previewMood = "" }
    // Let the island reach its new size first, so the effects start from Lumo.
    Timer { id: finishLater; interval: 260; onTriggered: { finishFx.restart(); win.burst++; miniMascot.celebrate(); mascot.celebrate() } }
    Timer { id: needsLater; interval: 120; onTriggered: needsFx.restart() }
    function finished() { finishLater.restart() }
    function needsYou() { needsLater.restart() }
    function previewFinished() {
        previewMood = "happy"; previewTimer.restart()
        sounds.play("finish"); finished()
    }
    function previewNeedsYou() {
        callingYou = true; callTimer.restart()
        sounds.play("needs"); needsYou()
    }

    SequentialAnimation {
        id: finishFx
        ScriptAction { script: ripple.tint = Ui.good }
        ParallelAnimation {
            NumberAnimation { target: ripple; property: "scale"; from: 1; to: 1.22; duration: 760; easing.type: Easing.OutCubic }
            NumberAnimation { target: ripple; property: "opacity"; from: 0.95; to: 0; duration: 760; easing.type: Easing.InQuad }
        }
        ParallelAnimation {
            NumberAnimation { target: ripple; property: "scale"; from: 1; to: 1.22; duration: 760; easing.type: Easing.OutCubic }
            NumberAnimation { target: ripple; property: "opacity"; from: 0.6; to: 0; duration: 760; easing.type: Easing.InQuad }
        }
    }
    SequentialAnimation {
        id: needsFx
        ScriptAction { script: ripple.tint = Ui.warn }
        SequentialAnimation {
            NumberAnimation { target: win; property: "shakeX"; to: -8; duration: 45; easing.type: Easing.OutQuad }
            NumberAnimation { target: win; property: "shakeX"; to: 8; duration: 80; easing.type: Easing.InOutQuad }
            NumberAnimation { target: win; property: "shakeX"; to: -6; duration: 70; easing.type: Easing.InOutQuad }
            NumberAnimation { target: win; property: "shakeX"; to: 5; duration: 65; easing.type: Easing.InOutQuad }
            NumberAnimation { target: win; property: "shakeX"; to: -2; duration: 55; easing.type: Easing.InOutQuad }
            NumberAnimation { target: win; property: "shakeX"; to: 0; duration: 45; easing.type: Easing.OutQuad }
        }
        SequentialAnimation {
            loops: 3
            ParallelAnimation {
                NumberAnimation { target: ripple; property: "scale"; from: 1; to: 1.16; duration: 900; easing.type: Easing.OutCubic }
                NumberAnimation { target: ripple; property: "opacity"; from: 0.9; to: 0; duration: 900; easing.type: Easing.InQuad }
            }
            PauseAnimation { duration: 250 }
        }
    }

    // ------------------------------------------------------------ geometry
    readonly property real targetW: mode === "open" ? Ui.openWidth
                                   : mode === "greeting" ? 340
                                   : mode === "compact" ? Math.max(232, Math.min(470, compactRow.implicitWidth + 30))
                                   : mode === "dictation" ? Math.max(200, Math.min(520, dictationBar.implicitWidth + 32))
                                   : mode === "rest" ? Math.max(232, Math.min(470, restRow.implicitWidth + 30))
                                   : 160
    readonly property real targetH: mode === "open" ? Math.min(win.maxOpenH, openCol.implicitHeight + 28)
                                   : mode === "greeting" ? 148
                                   : mode === "compact" || mode === "dictation" ? Ui.compactHeight
                                   : mode === "rest" ? Ui.compactHeight : 0
    property real bodyW: targetW
    property real bodyH: targetH
    Behavior on bodyW { SpringAnimation { spring: 3.4; damping: 0.3; epsilon: 0.3 } }
    Behavior on bodyH { SpringAnimation { spring: 3.0; damping: 0.32; epsilon: 0.3 } }
    onBodyWChanged: updateMask()
    onBodyHChanged: updateMask()

    function updateMask() {
        if (mode === "hidden" && bodyH < 2) {
            shell.setInputRect(Qt.rect(width / 2 - 150, 0, 300, 4))
            return
        }
        const w = Math.max(bodyW, targetW) + 4, h = Math.max(bodyH, targetH) + 4
        shell.setInputRect(Qt.rect((width - w) / 2, 0, w, h))
    }

    // ------------------------------------------------------------ scene
    Item {
        id: root
        anchors.fill: parent

        HoverHandler { id: rootHover }

        DropArea {
            id: dropArea
            anchors.fill: parent
            onEntered: (drag) => { drag.accept(Qt.CopyAction); win.openIsland("chat", true) }
            onDropped: (drop) => {
                for (const u of drop.urls) chat.attach(u)
                drop.accept(Qt.CopyAction)
                sounds.play("pop")
                mascot.poke()
            }
        }

        // The ripple behind the shape (tinted by the reaction playing).
        Rectangle {
            id: ripple
            property color tint: Ui.good
            x: body.x; y: body.y
            width: body.width; height: body.height
            radius: body.radius
            color: "transparent"
            border.width: 2.5
            border.color: tint
            opacity: 0
            visible: opacity > 0
            transformOrigin: Item.Top
            transform: Translate { x: win.shakeX }
        }

        // The black shape. Its top corners sit above the screen edge so only
        // the bottom ones show, like a notch.
        Rectangle {
            id: body
            readonly property real r: win.mode === "open" || win.mode === "greeting" ? 28 : 19
            x: (win.width - win.bodyW) / 2
            y: -r
            width: win.bodyW
            height: Math.max(0, win.bodyH) + r
            radius: r
            gradient: Gradient {
                GradientStop { position: 0; color: Ui.island }
                GradientStop { position: 1; color: Ui.islandLow }
            }
            opacity: win.bodyH > 1 ? 1 : 0
            antialiasing: true
            Behavior on radius { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }

            border.width: 1
            border.color: win.hasRequests || win.callingYou ? Qt.rgba(Ui.warn.r, Ui.warn.g, Ui.warn.b, 0.45)
                        : win.mood === "happy" ? Qt.rgba(Ui.good.r, Ui.good.g, Ui.good.b, 0.45)
                        : Qt.rgba(hub.headlineColor.r, hub.headlineColor.g, hub.headlineColor.b, 0.22)
            Behavior on border.color { ColorAnimation { duration: 300 } }
            transform: Translate { x: win.shakeX }

            TapHandler {
                enabled: win.mode === "compact" || win.mode === "rest"
                onTapped: win.openIsland(win.tab, false)
            }
        }

        // Everything inside the shape, clipped to it.
        Item {
            id: clipper
            x: body.x
            y: 0
            width: body.width
            height: Math.max(0, win.bodyH)
            clip: true
            transform: Translate { x: win.shakeX }

            // ---------------- greeting
            Item {
                id: greetLayer
                anchors.fill: parent
                opacity: win.mode === "greeting" ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { OpacityAnimator { duration: win.mode === "greeting" ? 200 : 260 } }

                // a field of light bursting out of Lumo, then drifting away
                Repeater {
                    model: 30
                    Rectangle {
                        id: star
                        required property int index
                        readonly property real angle: index * 2.399963 // the golden angle: an even spread
                        readonly property real reach: 46 + (index * 37 % 120)
                        readonly property real cx: greetLayer.width / 2
                        readonly property real cy: greetLayer.height / 2 + 6
                        width: index % 5 === 0 ? 4 : 2.5
                        height: width
                        radius: width / 2
                        color: index % 3 === 0 ? Ui.accent : index % 3 === 1 ? Ui.glow : "white"
                        x: cx; y: cy
                        opacity: 0
                        Connections { target: win; function onGreetBurstChanged() { starFly.restart() } }
                        SequentialAnimation {
                            id: starFly
                            PauseAnimation { duration: (star.index % 6) * 40 }
                            ParallelAnimation {
                                NumberAnimation { target: star; property: "x"; from: star.cx; to: star.cx + Math.cos(star.angle) * star.reach * 1.5; duration: 1700; easing.type: Easing.OutCubic }
                                NumberAnimation { target: star; property: "y"; from: star.cy; to: star.cy + Math.sin(star.angle) * star.reach * 0.55; duration: 1700; easing.type: Easing.OutCubic }
                                SequentialAnimation {
                                    NumberAnimation { target: star; property: "opacity"; to: 0.95; duration: 120 }
                                    PauseAnimation { duration: 500 }
                                    NumberAnimation { target: star; property: "opacity"; to: 0; duration: 1080; easing.type: Easing.InQuad }
                                }
                            }
                        }
                    }
                }

                Mascot {
                    id: helloMascot
                    size: 64
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 8
                    color: Ui.accent
                    outfit: win.outfit
                    alive: win.mode === "greeting"
                    scale: 0
                    opacity: 0
                }
                TapHandler { onTapped: win.endGreeting() }
            }

            // ---------------- compact
            Item {
                anchors.fill: parent
                opacity: win.mode === "compact" ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { OpacityAnimator { duration: win.mode === "compact" ? 220 : 90 } }

                Row {
                    id: compactRow
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 1
                    spacing: 9

                    Mascot {
                        id: miniMascot
                        size: 24
                        anchors.verticalCenter: parent.verticalCenter
                        color: hub.headlineColor
                        mood: win.mood === "idle" ? (win.callingYou ? "waiting" : "idle") : win.mood
                        badge: win.mood === "alert" || win.callingYou
                        outfit: win.outfit
                        onWardrobeRequested: win.openWardrobe()
                        dancing: win.dancing && win.mode === "compact" && win.mood === "idle"
                        alive: win.mode === "compact"
                        onPoked: sounds.play(pokes === 0 ? "dizzy" : "poke")
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: hub.headlineAgent || "Lumo"
                        color: Ui.text
                        font.pixelSize: Ui.body
                        font.weight: Font.DemiBold
                        width: Math.min(implicitWidth, 130)
                        elide: Text.ElideRight
                    }
                    Text {
                        id: headlineText
                        anchors.verticalCenter: parent.verticalCenter
                        text: win.mood === "happy" ? qsTr("Done")
                            : win.callingYou && win.mood !== "alert" ? qsTr("Waiting for you")
                            : win.mood === "alert" && !hub.headline ? qsTr("Needs you")
                            : hub.headline || (hub.sessions.count ? (hub.sessions.count === 1 ? qsTr("1 session") : qsTr("%1 sessions").arg(hub.sessions.count)) : qsTr("Hi!"))
                        color: win.mood === "happy" ? Ui.good : win.mood === "alert" || win.callingYou ? Ui.warn : Ui.dim
                        Behavior on color { ColorAnimation { duration: 250 } }
                        font.pixelSize: Ui.body
                        width: Math.min(implicitWidth, 230)
                        elide: Text.ElideRight
                        // fade in when the activity changes
                        onTextChanged: headlineFade.restart()
                        OpacityAnimator { id: headlineFade; target: headlineText; from: 0.25; to: 1; duration: 200 }
                    }
                    // working dots
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 3
                        visible: win.mood === "working"
                        Repeater {
                            model: 3
                            Rectangle {
                                required property int index
                                width: 4; height: 4; radius: 2; color: hub.headlineColor
                                SequentialAnimation on opacity {
                                    running: win.mode === "compact" && win.mood === "working"
                                    loops: Animation.Infinite
                                    PauseAnimation { duration: index * 140 }
                                    OpacityAnimator { from: 0.2; to: 1; duration: 280 }
                                    OpacityAnimator { from: 1; to: 0.2; duration: 280 }
                                    PauseAnimation { duration: (2 - index) * 140 }
                                }
                            }
                        }
                    }
                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: win.mood === "happy"
                        path: Ui.icons.check
                        tint: Ui.good
                        size: 16
                        stroke: 2.4
                    }
                }
            }

            // ---------------- rest
            Item {
                anchors.fill: parent
                opacity: win.mode === "rest" ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { OpacityAnimator { duration: win.mode === "rest" ? 240 : 80 } }
                Row {
                    id: restRow
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 1
                    spacing: 9
                    Mascot {
                        size: 24
                        anchors.verticalCenter: parent.verticalCenter
                        color: Ui.accent
                        mood: win.mood === "waiting" ? "waiting" : "idle"
                        alive: false
                        blinks: win.mode === "rest"
                        outfit: win.outfit
                        onWardrobeRequested: win.openWardrobe()
                        dancing: win.dancing && win.mode === "rest"
                        badge: win.mood === "waiting"
                    }
                    // music playing: cover and track instead of the name
                    Rectangle {
                        visible: win.musicShown
                        anchors.verticalCenter: parent.verticalCenter
                        width: 20; height: 20; radius: 5
                        color: Ui.line
                        clip: true
                        Image { anchors.fill: parent; source: win.musicShown ? music.artUrl : ""; fillMode: Image.PreserveAspectCrop; asynchronous: true; sourceSize: Qt.size(40, 40) }
                    }
                    Text {
                        visible: win.musicShown
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, 260)
                        elide: Text.ElideRight
                        textFormat: Text.StyledText
                        text: "<b>" + music.title.replace(/</g, "&lt;") + "</b>" + (music.artist ? "  <font color='#9A9AA5'>" + music.artist.replace(/</g, "&lt;") + "</font>" : "")
                        color: Ui.text
                        font.pixelSize: Ui.body
                    }
                    Text {
                        visible: !win.musicShown
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Lumo"
                        color: Ui.text
                        font.pixelSize: Ui.body
                        font.weight: Font.DemiBold
                    }
                    Text {
                        visible: !win.musicShown
                        anchors.verticalCenter: parent.verticalCenter
                        text: win.mood === "waiting" ? qsTr("Waiting for you")
                            : hub.sessions.count ? (hub.sessions.count === 1 ? qsTr("1 session") : qsTr("%1 sessions").arg(hub.sessions.count))
                            : qsTr("All quiet")
                        color: win.mood === "waiting" ? Ui.warn : Ui.dim
                        font.pixelSize: Ui.body
                    }
                    Text {
                        visible: hub.hasUsage && shell.showUsage
                        anchors.verticalCenter: parent.verticalCenter
                        text: Math.round(hub.fiveHour) + "%"
                        color: hub.fiveHour >= 90 ? Ui.bad : hub.fiveHour >= 70 ? Ui.warn : Ui.faint
                        font.pixelSize: Ui.small
                        font.weight: Font.Medium
                    }
                }
            }

            // ---------------- dictation
            Item {
                anchors.fill: parent
                opacity: win.mode === "dictation" ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { OpacityAnimator { duration: win.mode === "dictation" ? 160 : 90 } }
                DictationBar {
                    id: dictationBar
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 1
                    live: win.mode === "dictation"
                }
            }

            // ---------------- open
            Item {
                id: openLayer
                anchors.fill: parent
                opacity: win.mode === "open" ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { OpacityAnimator { duration: win.mode === "open" ? 240 : 80 } }

                ColumnLayout {
                    id: openCol
                    width: Ui.openWidth - 32
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 12
                    spacing: 12

                    // header (above what follows, so its tooltips aren't covered)
                    RowLayout {
                        z: 5
                        Layout.fillWidth: true
                        spacing: 10

                        Mascot {
                            id: mascot
                            size: 40
                            color: hub.headlineColor
                            mood: win.mood === "idle" && win.tab === "chat" && chat.busy ? "working" : win.mood
                            badge: hub.requests.count > 0
                            outfit: wardrobePanel.trying !== "" && win.showWardrobe
                                    ? Ui.outfitFor(wardrobePanel.trying, true) : win.outfit
                            onWardrobeRequested: win.showWardrobe = !win.showWardrobe
                            dancing: win.dancing && win.mode === "open"
                            alive: win.mode === "open"
                            lookAt: rootHover.hovered ? mascot.mapFromItem(root, rootHover.point.position) : null
                            onPoked: sounds.play(pokes === 0 ? "dizzy" : "poke")
                        }
                        ColumnLayout {
                            spacing: 0
                            Layout.fillWidth: true
                            Text {
                                text: hub.requests.count ? qsTr("Lumo needs you") : hub.workingCount ? (hub.workingCount === 1 ? qsTr("1 agent working") : qsTr("%1 agents working").arg(hub.workingCount)) : qsTr("All quiet")
                                color: hub.requests.count ? Ui.warn : Ui.text
                                Behavior on color { ColorAnimation { duration: 200 } }
                                font.pixelSize: Ui.title
                                font.weight: Font.DemiBold
                            }
                            Text {
                                text: hub.headline || qsTr("Your agents show up here.")
                                color: Ui.dim
                                font.pixelSize: Ui.small
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        // tabs
                        Rectangle {
                            implicitWidth: tabs.implicitWidth + 6
                            implicitHeight: 30
                            radius: 15
                            color: Ui.card
                            Rectangle { // sliding highlight
                                id: tabHighlight
                                property Item target: null
                                y: 3; height: 24; radius: 12
                                color: Ui.line
                                x: target ? target.x + 3 : 3
                                width: target ? target.width : 0
                                Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                                Behavior on width { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
                            }
                            Row {
                                id: tabs
                                x: 3
                                anchors.verticalCenter: parent.verticalCenter
                                Repeater {
                                    model: [{ id: "agents", label: qsTr("Agents") }, { id: "chat", label: qsTr("Chat") }]
                                    Item {
                                        id: tabItem
                                        required property var modelData
                                        readonly property bool current: win.tab === modelData.id
                                        readonly property int count: modelData.id === "agents" ? hub.sessions.count : 0
                                        width: tabRow.implicitWidth + 24
                                        height: 24
                                        onCurrentChanged: if (current) tabHighlight.target = tabItem
                                        Component.onCompleted: if (current) tabHighlight.target = tabItem
                                        Row {
                                            id: tabRow
                                            anchors.centerIn: parent
                                            spacing: 6
                                            Text {
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: tabItem.modelData.label
                                                color: tabItem.current || tabHover.hovered ? Ui.text : Ui.dim
                                                font.pixelSize: Ui.body
                                                font.weight: tabItem.current ? Font.DemiBold : Font.Normal
                                            }
                                            Rectangle {
                                                visible: tabItem.count > 0
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: Math.max(18, countText.implicitWidth + 10)
                                                height: 16
                                                radius: 8
                                                color: tabItem.current ? Qt.rgba(Ui.accent.r, Ui.accent.g, Ui.accent.b, 0.25) : Ui.line
                                                Text {
                                                    id: countText
                                                    anchors.centerIn: parent
                                                    text: tabItem.count
                                                    color: tabItem.current ? Ui.accent : Ui.dim
                                                    font.pixelSize: 10
                                                    font.weight: Font.DemiBold
                                                }
                                            }
                                        }
                                        HoverHandler { id: tabHover; cursorShape: Qt.PointingHandCursor }
                                        TapHandler { onTapped: win.tab = tabItem.modelData.id }
                                    }
                                }
                            }
                        }

                        Row {
                            spacing: 6
                            visible: hub.hasUsage && shell.showUsage
                            Layout.bottomMargin: 8
                            UsageRing { size: 28; label: "5h"; percent: hub.fiveHour; resetsAt: hub.fiveHourReset }
                            UsageRing { size: 28; label: "7d"; percent: hub.sevenDay; resetsAt: hub.sevenDayReset }
                        }

                        Row {
                            spacing: 6
                            IconButton {
                                icon: shell.alwaysShow ? Ui.icons.eye : Ui.icons.eyeOff
                                tip: shell.alwaysShow ? qsTr("Hide when idle") : qsTr("Always show")
                                onClicked: shell.alwaysShow = !shell.alwaysShow
                            }
                            IconButton {
                                visible: dictation.enabled
                                icon: Ui.icons.mic
                                tip: qsTr("Dictate")
                                onClicked: { win.closeIsland(); dictation.toggle() }
                            }
                            IconButton {
                                icon: sounds.muted ? Ui.icons.volumeOff : Ui.icons.volume
                                tip: sounds.muted ? qsTr("Sounds off") : qsTr("Sounds on")
                                active: sounds.muted
                                activeTint: Ui.faint
                                onClicked: sounds.muted = !sounds.muted
                            }
                            IconButton {
                                icon: Ui.icons.gear
                                tip: qsTr("Settings")
                                onClicked: shell.settingsRequested()
                            }
                        }
                    }

                    // ---- agents: everything scrolls together once it's taller than the screen allows
                    Flickable {
                        id: agentsScroll
                        visible: win.tab === "agents"
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(agentsCol.implicitHeight, win.maxOpenH - 110)
                        contentHeight: agentsCol.implicitHeight
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: agentsScroll.contentHeight > agentsScroll.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

                    ColumnLayout {
                        id: agentsCol
                        width: agentsScroll.width
                        spacing: 8

                            // the wardrobe (right-click Lumo)
                            Rectangle {
                                visible: win.showWardrobe
                                Layout.fillWidth: true
                                implicitHeight: wardCol.implicitHeight + 24
                                radius: 14
                                color: Ui.card
                                Column {
                                    id: wardCol
                                    x: 12; y: 12
                                    width: parent.width - 24
                                    spacing: 10
                                    Item {
                                        width: parent.width; height: 20
                                        Text { text: qsTr("Wardrobe — rest on an outfit to try it on"); color: Ui.text; font.pixelSize: Ui.body; font.weight: Font.DemiBold }
                                        IconButton {
                                            anchors.right: parent.right
                                            anchors.verticalCenter: parent.verticalCenter
                                            size: 24; iconSize: 13; filled: false
                                            icon: Ui.icons.close
                                            onClicked: win.showWardrobe = false
                                        }
                                    }
                                    Wardrobe {
                                        id: wardrobePanel
                                        width: parent.width
                                        current: shell.outfit
                                        onPicked: (outfit) => { shell.outfit = outfit; sounds.play("pop") }
                                    }
                                }
                            }

                            MusicCard {
                                visible: shell.showMusic && music.available
                                Layout.fillWidth: true
                                live: win.mode === "open" && win.tab === "agents"
                            }

                            // after the rename: the hooks still point at the old path
                            Rectangle {
                                visible: installer.needsUpdate
                                Layout.fillWidth: true
                                implicitHeight: updRow.implicitHeight + 20
                                radius: 12
                                color: Ui.card
                                border.color: Qt.rgba(Ui.accent.r, Ui.accent.g, Ui.accent.b, 0.5)
                                RowLayout {
                                    id: updRow
                                    x: 12; y: 10; width: parent.width - 24
                                    Text {
                                        Layout.fillWidth: true
                                        wrapMode: Text.Wrap
                                        color: Ui.text
                                        font.pixelSize: Ui.small
                                        text: qsTr("Nook is now Lumo. Update the Claude Code connection to the new hook path (you'll see the change first).")
                                    }
                                    PillButton {
                                        text: qsTr("Review…")
                                        primary: true
                                        tint: Ui.accent
                                        onClicked: { installer.prepare(true, true, true); settings.page = 2; shell.settingsRequested() }
                                    }
                                }
                            }

                            ListView {
                                id: requestList
                                Layout.fillWidth: true
                                Layout.preferredHeight: contentHeight
                                interactive: false
                                spacing: 8
                                model: hub.requests
                                delegate: RequestCard { live: win.mode === "open" }
                                add: Transition {
                                    ParallelAnimation {
                                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200 }
                                        NumberAnimation { property: "scale"; from: 0.92; to: 1; duration: 260; easing.type: Easing.OutBack }
                                    }
                                }
                                remove: Transition {
                                    ParallelAnimation {
                                        NumberAnimation { property: "opacity"; to: 0; duration: 160 }
                                        NumberAnimation { property: "scale"; to: 0.94; duration: 160 }
                                    }
                                }
                                displaced: Transition { NumberAnimation { property: "y"; duration: 220; easing.type: Easing.OutCubic } }
                            }

                            ListView {
                                id: sessionList
                                Layout.fillWidth: true
                                Layout.preferredHeight: contentHeight
                                interactive: false
                                spacing: 8
                                model: hub.sessions
                                delegate: SessionCard { live: win.mode === "open" && win.tab === "agents" }
                                add: Transition {
                                    ParallelAnimation {
                                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 220 }
                                        NumberAnimation { property: "x"; from: -20; to: 0; duration: 260; easing.type: Easing.OutCubic }
                                    }
                                }
                                remove: Transition { NumberAnimation { property: "opacity"; to: 0; duration: 160 } }
                                displaced: Transition { NumberAnimation { properties: "x,y"; duration: 240; easing.type: Easing.OutCubic } }
                                move: Transition { NumberAnimation { properties: "y"; duration: 240; easing.type: Easing.OutCubic } }
                            }

                            // empty state
                            ColumnLayout {
                                visible: hub.sessions.count === 0 && hub.requests.count === 0
                                Layout.fillWidth: true
                                Layout.topMargin: 6
                                Layout.bottomMargin: 6
                                spacing: 10
                                Text {
                                    Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.Wrap
                                    color: Ui.dim
                                    font.pixelSize: Ui.body
                                    text: installer.installed ? qsTr("No agent is running. Start Claude Code in a terminal and Lumo will follow it here.")
                                                              : qsTr("Connect Claude Code to see its sessions, approve its requests and answer its questions from here.")
                                }
                                PillButton {
                                    visible: !installer.installed
                                    Layout.alignment: Qt.AlignHCenter
                                    text: qsTr("Connect Claude Code…")
                                    primary: true
                                    tint: Ui.accent
                                    onClicked: { settings.page = 2; shell.settingsRequested() }
                                }
                            }
                    }
                    }

                    // ---- chat
                    ChatPane {
                        id: chatPane
                        live: win.mode === "open" && win.tab === "chat"
                        visible: win.tab === "chat"
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                    }
                }
            }
        }

        // Sparkles bursting out of Lumo when Claude finishes.
        Item {
            id: sparkles
            z: 50
            width: 1; height: 1
            Connections {
                target: win
                function onBurstChanged() {
                    const m = win.mode === "open" ? mascot : miniMascot
                    const c = m.mapToItem(root, m.width / 2, m.height / 2)
                    sparkles.x = c.x; sparkles.y = c.y
                }
            }
            Repeater {
                model: 14
                Rectangle {
                    id: dot
                    required property int index
                    readonly property real angle: (index / 14) * 2 * Math.PI + 0.3
                    readonly property real reach: 26 + (index % 3) * 9
                    width: index % 3 === 0 ? 6.5 : 4.5
                    height: width
                    radius: width / 2
                    color: ["#3DD68C", "#FFD866", "#D97757", "#8FE3FF"][index % 4]
                    opacity: 0
                    Connections { target: win; function onBurstChanged() { fly.restart() } }
                    ParallelAnimation {
                        id: fly
                        NumberAnimation { target: dot; property: "x"; from: -dot.width / 2; to: Math.cos(dot.angle) * dot.reach * 1.6 - dot.width / 2; duration: 760; easing.type: Easing.OutCubic }
                        NumberAnimation { target: dot; property: "y"; from: -dot.height / 2; to: Math.sin(dot.angle) * dot.reach - dot.height / 2; duration: 760; easing.type: Easing.OutCubic }
                        NumberAnimation { target: dot; property: "scale"; from: 1.4; to: 0.3; duration: 760 }
                        SequentialAnimation {
                            NumberAnimation { target: dot; property: "opacity"; to: 1; duration: 60 }
                            NumberAnimation { target: dot; property: "opacity"; to: 0; duration: 700; easing.type: Easing.InQuad }
                        }
                    }
                }
            }
        }

        Keys.onEscapePressed: win.closeIsland()
    }

    SettingsWindow {
        id: settings
        transientParent: null
        onPreviewFinished: win.previewFinished()
        onPreviewNeedsYou: win.previewNeedsYou()
        onPreviewGreeting: win.greet()
    }
}
