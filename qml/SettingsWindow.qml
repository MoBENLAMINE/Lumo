import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: settings
    width: 720
    height: 560
    minimumWidth: 600
    minimumHeight: 440
    title: qsTr("Lumo Settings")
    color: "#111114"
    property int page: 0
    signal previewFinished()
    signal previewNeedsYou()

    readonly property var pages: [qsTr("General"), qsTr("Dictation"), qsTr("Claude Code"), qsTr("Chat"), qsTr("About")]

    component Section: Text {
        color: Ui.text
        font.pixelSize: Ui.title
        font.weight: Font.DemiBold
        Layout.topMargin: 6
    }
    component Hint: Text {
        color: Ui.dim
        font.pixelSize: Ui.small
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    component Toggle: Rectangle {
        id: t
        property bool on: false
        signal toggled(bool on)
        implicitWidth: 40; implicitHeight: 22; radius: 11
        color: on ? Ui.accent : Ui.line
        Behavior on color { ColorAnimation { duration: 150 } }
        Rectangle {
            width: 18; height: 18; radius: 9; y: 2
            x: t.on ? t.width - width - 2 : 2
            color: "white"
            Behavior on x { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        }
        TapHandler { onTapped: t.toggled(!t.on) }
    }
    component Row2: RowLayout {
        property alias label: lbl.text
        Layout.fillWidth: true
        spacing: 12
        Text { id: lbl; color: Ui.text; font.pixelSize: Ui.body; Layout.fillWidth: true; wrapMode: Text.Wrap }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // sidebar
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 180
            color: "#0C0C0F"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 4
                RowLayout {
                    spacing: 10
                    Layout.bottomMargin: 14
                    Mascot { size: 34; alive: settings.visible; mood: hub.mood; outfit: shell.outfit === "auto" ? Ui.seasonal(new Date(), false) : shell.outfit }
                    Text { text: "Lumo"; color: Ui.text; font.pixelSize: 18; font.weight: Font.Bold }
                }
                Repeater {
                    model: settings.pages
                    Rectangle {
                        required property string modelData
                        required property int index
                        Layout.fillWidth: true
                        implicitHeight: 34
                        radius: 8
                        color: settings.page === index ? Ui.line : navHover.hovered ? Ui.card : "transparent"
                        Behavior on color { ColorAnimation { duration: 120 } }
                        Text { x: 12; anchors.verticalCenter: parent.verticalCenter; text: modelData; color: Ui.text; font.pixelSize: Ui.body }
                        HoverHandler { id: navHover }
                        TapHandler { onTapped: settings.page = index }
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth

            StackLayout {
                width: parent.width
                currentIndex: settings.page

                // ---------------------------------------------------- General
                ColumnLayout {
                    Layout.margins: 24
                    spacing: 14
                    Section { text: qsTr("General") }
                    Row2 {
                        label: qsTr("Start Lumo when you log in")
                        Toggle { on: shell.autostart; onToggled: (v) => shell.autostart = v }
                    }
                    Row2 {
                        label: qsTr("Always show the island, on top of everything (or use the eye button in the island)")
                        Toggle { on: shell.alwaysShow; onToggled: (v) => shell.alwaysShow = v }
                    }
                    Row2 {
                        label: qsTr("Show Claude usage (5-hour and weekly limits, read from your Claude account, updated every 5 minutes)")
                        Toggle { on: shell.showUsage; onToggled: (v) => shell.showUsage = v }
                    }
                    Row2 {
                        label: qsTr("Show what's playing (Spotify, browser, any player)")
                        Toggle { on: shell.showMusic; onToggled: (v) => shell.showMusic = v }
                    }
                    Row2 {
                        label: qsTr("Lumo dances while music plays")
                        Toggle { on: shell.dance; onToggled: (v) => shell.dance = v }
                    }
                    Row2 {
                        label: qsTr("Sounds")
                        Toggle { on: !sounds.muted; onToggled: (v) => sounds.muted = !v }
                    }
                    Row2 {
                        label: qsTr("Volume")
                        Slider {
                            from: 0; to: 1; value: sounds.volume
                            Layout.preferredWidth: 200
                            onMoved: sounds.volume = value
                        }
                        PillButton { text: qsTr("Test"); onClicked: sounds.play("hello") }
                    }
                    Row2 {
                        label: qsTr("When Claude finishes, and when it needs you")
                        PillButton { text: qsTr("Preview “finished”"); onClicked: settings.previewFinished() }
                        PillButton { text: qsTr("Preview “needs you”"); onClicked: settings.previewNeedsYou() }
                    }
                    Hint { text: qsTr("Replace a sound by dropping a WAV named hello, finish, needs, error, poke, dizzy, pop, send or reply in ~/.config/lumo/sounds/.") }
                    Section { text: qsTr("Wardrobe") }
                    Hint { text: qsTr("Seasonal dresses Lumo by itself: a witch hat at Halloween, a Santa hat in December, a party hat at New Year, a bow on Valentine's Day, and headphones while music plays. You can also right-click Lumo in the island.") }
                    Wardrobe {
                        Layout.fillWidth: true
                        current: shell.outfit
                        onPicked: (outfit) => shell.outfit = outfit
                    }
                    Section { text: qsTr("Shortcuts") }
                    Hint { text: qsTr("Meta+Alt+N opens the chat, Meta+Alt+I shows or hides the island. Change them in System Settings → Shortcuts → Lumo.") }
                    Section { text: qsTr("The island") }
                    Hint { text: qsTr("Hover the pill to open it, drop a file on it to ask about it. Poke Lumo — but not too much.") }
                    Hint { visible: !shell.layerShell; color: Ui.warn; text: qsTr("Not running on Wayland: the island uses a plain always-on-top window.") }
                }

                // ---------------------------------------------------- Dictation
                ColumnLayout {
                    Layout.margins: 24
                    spacing: 14
                    Section { text: qsTr("Dictation") }
                    Row2 {
                        label: qsTr("Hold a key, speak, let go: your words are pasted where you type")
                        Toggle { on: dictation.enabled; onToggled: (v) => dictation.enabled = v }
                    }

                    // the key
                    RowLayout {
                        spacing: 10
                        Rectangle {
                            implicitWidth: keyText.implicitWidth + 20; implicitHeight: 30; radius: 8
                            color: Ui.card; border.color: Ui.line
                            Text { id: keyText; anchors.centerIn: parent; color: Ui.text; font.pixelSize: Ui.body; font.weight: Font.DemiBold
                                   text: dictation.keyLabel || qsTr("no key yet") }
                        }
                        Text {
                            color: Ui.dim; font.pixelSize: Ui.small; Layout.fillWidth: true; wrapMode: Text.Wrap
                            text: dictation.keyMode === "evdev" ? qsTr("Hold left Ctrl alone. Two quick taps for hands-free. Esc cancels.")
                                : dictation.keyMode === "portal" ? qsTr("Hold it to talk. Tap it twice for hands-free, press again to finish.")
                                : dictation.keyProblem || qsTr("Waiting for the desktop…")
                        }
                        PillButton { visible: dictation.keyMode === "portal"; text: qsTr("Change keys…"); onClicked: dictation.configureKey() }
                    }
                    Hint {
                        visible: dictation.keyMode !== "evdev"
                        text: qsTr("Wayland lets no app see a lone Ctrl. To hold left Ctrl alone, add yourself to the input group (sudo usermod -aG input $USER), log out and in: Lumo switches by itself. Note that any app you run can then read the keyboard.")
                    }

                    Row2 {
                        label: qsTr("Mute the computer's sound while I dictate (so only my voice is heard)")
                        Toggle { on: dictation.muteOutput; onToggled: (v) => dictation.muteOutput = v }
                    }
                    Row2 {
                        label: qsTr("Microphone")
                        ComboBox {
                            id: micBox
                            Layout.preferredWidth: 280
                            property var mics: [{ id: "", name: qsTr("System default") }].concat(dictation.microphones())
                            model: mics.map(m => m.name)
                            currentIndex: Math.max(0, mics.findIndex(m => m.id === dictation.microphoneId))
                            onActivated: (i) => dictation.microphoneId = mics[i].id
                            onPressedChanged: if (pressed) mics = [{ id: "", name: qsTr("System default") }].concat(dictation.microphones())
                        }
                    }

                    // the model
                    Section { text: qsTr("Speech recognition") }
                    RowLayout {
                        spacing: 8
                        Rectangle { width: 10; height: 10; radius: 5; color: dictation.speech.modelPresent ? Ui.good : Ui.warn }
                        Text {
                            Layout.fillWidth: true; wrapMode: Text.Wrap; color: Ui.text; font.pixelSize: Ui.body
                            text: dictation.speech.modelPresent
                                  ? qsTr("Parakeet v3 — offline, 25 languages%1").arg(dictation.speech.loaded ? qsTr(" · in memory") : "")
                                  : qsTr("Parakeet isn't installed yet (about 640 MB).")
                        }
                        PillButton {
                            visible: !dictation.speech.modelPresent && !dictation.speech.downloading
                            text: qsTr("Download"); primary: true; tint: Ui.accent
                            onClicked: dictation.speech.download()
                        }
                    }
                    Hint { visible: dictation.speech.modelPresent; text: dictation.speech.modelDir; font.family: Ui.mono }
                    Rectangle {
                        visible: dictation.speech.downloading
                        Layout.fillWidth: true; implicitHeight: 6; radius: 3; color: Ui.line
                        Rectangle { width: parent.width * dictation.speech.downloadProgress; height: parent.height; radius: 3; color: Ui.accent }
                    }
                    Hint { visible: dictation.speech.downloadError.length > 0; color: Ui.bad; text: dictation.speech.downloadError }
                    Row2 {
                        label: qsTr("Keep it in memory (instant first dictation, ~1 GB of RAM)")
                        Toggle { on: dictation.speech.keepLoaded; onToggled: (v) => dictation.speech.keepLoaded = v }
                    }
                    Hint { text: qsTr("Otherwise it loads while you speak and leaves memory after 5 idle minutes. Nothing you say leaves this computer.") }

                    // stats
                    Section { text: qsTr("Your words") }
                    RowLayout {
                        spacing: 10
                        Repeater {
                            model: [
                                { v: dictation.wordsToday, l: qsTr("today") },
                                { v: dictation.totalWords, l: qsTr("in all") },
                                { v: dictation.wordsPerMinute, l: qsTr("words / min") }
                            ]
                            Rectangle {
                                required property var modelData
                                implicitWidth: 120; implicitHeight: 62; radius: 10; color: Ui.card
                                Column {
                                    anchors.centerIn: parent
                                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.v; color: Ui.text; font.pixelSize: 20; font.weight: Font.Bold }
                                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.l; color: Ui.dim; font.pixelSize: Ui.small }
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Section { text: qsTr("Recent"); Layout.fillWidth: true }
                        PillButton { visible: dictation.history.length > 0; text: qsTr("Clear"); onClicked: dictation.clearHistory() }
                    }
                    Hint { visible: dictation.history.length === 0; text: qsTr("Nothing dictated yet.") }
                    Repeater {
                        model: dictation.history.slice(0, 20)
                        Rectangle {
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: histText.implicitHeight + 34
                            radius: 10
                            color: histHover.hovered ? Ui.cardHover : Ui.card
                            HoverHandler { id: histHover }
                            Text {
                                id: histText
                                x: 12; y: 8; width: parent.width - 80
                                text: modelData.text; color: Ui.text; font.pixelSize: Ui.body; wrapMode: Text.Wrap
                            }
                            Text {
                                x: 12; anchors.bottom: parent.bottom; anchors.bottomMargin: 6
                                text: new Date(modelData.at).toLocaleString(Qt.locale(), "d MMM HH:mm") + (modelData.app ? " · " + modelData.app : "")
                                color: Ui.faint; font.pixelSize: 10
                            }
                            Row {
                                anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter
                                spacing: 6
                                visible: histHover.hovered
                                PillButton { implicitHeight: 24; text: qsTr("Instinct"); onClicked: shell.sendToInstinct(modelData.text) }
                                PillButton { implicitHeight: 24; text: qsTr("Copy"); onClicked: dictation.copy(modelData.text) }
                            }
                        }
                    }
                }

                // ---------------------------------------------------- Claude Code
                ColumnLayout {
                    Layout.margins: 24
                    spacing: 14
                    Section { text: qsTr("Claude Code") }
                    RowLayout {
                        spacing: 8
                        Rectangle { width: 10; height: 10; radius: 5; color: installer.installed ? Ui.good : Ui.warn }
                        Text {
                            color: Ui.text; font.pixelSize: Ui.body
                            text: installer.installed ? qsTr("Connected: Claude Code reports to Lumo.") : qsTr("Not connected yet.")
                        }
                    }
                    Hint {
                        text: qsTr("Lumo adds its hook (%1) to ~/.claude/settings.json so every session, tool, permission request and question reaches the island. "
                                 + "If Lumo isn't running the hook exits at once and Claude Code carries on as usual.").arg(installer.hookPath)
                    }
                    CheckBox {
                        id: replaceCoucou
                        visible: installer.coucouHooks
                        checked: true
                        text: qsTr("Replace Coucou's hooks (both would ask you for every permission)")
                        contentItem: Text { leftPadding: 28; text: replaceCoucou.text; color: Ui.text; font.pixelSize: Ui.body; wrapMode: Text.Wrap }
                    }
                    CheckBox {
                        id: statusLine
                        checked: true
                        text: qsTr("Show plan usage (5-hour and weekly limits) — your current status line keeps working")
                        contentItem: Text { leftPadding: 28; text: statusLine.text; color: Ui.text; font.pixelSize: Ui.body; wrapMode: Text.Wrap }
                    }
                    RowLayout {
                        spacing: 8
                        PillButton { text: installer.installed ? qsTr("Update connection…") : qsTr("Connect…"); primary: true; tint: Ui.accent
                            onClicked: installer.prepare(true, replaceCoucou.visible && replaceCoucou.checked, statusLine.checked) }
                        PillButton { visible: installer.installed; text: qsTr("Disconnect…"); onClicked: installer.prepare(false, false, false) }
                    }
                    Hint { text: installer.message; visible: installer.message.length > 0; color: Ui.text }
                    Row2 {
                        label: qsTr("Show my cloud sessions from claude.ai/code. Lumo asks Anthropic for the list every minute, with your Claude login; approvals still happen on claude.ai.")
                        Toggle { on: cloud.enabled; onToggled: (v) => cloud.enabled = v }
                    }
                    DiffView {
                        visible: installer.preview.length > 0
                        Layout.fillWidth: true
                        maxHeight: 240
                        lines: installer.preview
                    }
                    RowLayout {
                        visible: installer.preview.length > 0
                        spacing: 8
                        PillButton { text: qsTr("Cancel"); onClicked: installer.cancel() }
                        PillButton { text: qsTr("Apply"); primary: true; tint: Ui.good; onClicked: installer.apply() }
                    }
                }

                // ---------------------------------------------------- Chat
                ColumnLayout {
                    Layout.margins: 24
                    spacing: 14
                    Section { text: qsTr("Chat") }
                    Hint { text: qsTr("Claude chat runs on your Claude Pro subscription, through the `claude` CLI you are signed in to — no API key, nothing billed per token. Ollama talks to a local server on this computer.") }
                    Row2 {
                        label: qsTr("Provider")
                        Repeater {
                            model: [{ id: "claude", n: "Claude (Pro)" }, { id: "ollama", n: "Ollama" }]
                            PillButton {
                                required property var modelData
                                text: modelData.n
                                checked: chat.provider === modelData.id
                                tint: Ui.accent
                                onClicked: chat.provider = modelData.id
                            }
                        }
                    }
                    Row2 {
                        label: qsTr("Model")
                        ComboBox {
                            Layout.preferredWidth: 220
                            model: chat.models
                            currentIndex: chat.models.indexOf(chat.model)
                            onActivated: (i) => chat.model = chat.models[i]
                        }
                    }
                    Section { text: qsTr("Instinct") }
                    Hint {
                        text: qsTr("Instinct and Claude have no direct connection, so Lumo relays through you, as Instinct's own brief describes. "
                                 + "“Send to Instinct” (under chat answers, finished sessions and dictations) copies the text and opens your WhatsApp chat with Instinct with it typed in — you review it and press Send. "
                                 + "In the Notion chat, “From Instinct” finds the pages Instinct labelled “(Made by Instinct)”, read as information only.")
                    }
                    RowLayout {
                        spacing: 8
                        Layout.fillWidth: true
                        TextField {
                            id: instinctField
                            Layout.fillWidth: true
                            text: shell.instinctNumber ? "+" + shell.instinctNumber : ""
                            placeholderText: qsTr("Instinct's WhatsApp number, e.g. +212 6…")
                            color: Ui.text
                            background: Rectangle { radius: 8; color: Ui.card; border.color: instinctField.activeFocus ? Ui.accent : Ui.line }
                        }
                        PillButton { text: qsTr("Save"); primary: true; tint: Ui.accent; onClicked: shell.instinctNumber = instinctField.text }
                    }
                }

                // ---------------------------------------------------- About
                ColumnLayout {
                    Layout.margins: 24
                    spacing: 14
                    Section { text: qsTr("About Lumo") }
                    Hint { text: qsTr("Lumo 0.1 — a native Qt Quick companion for your AI coding agents, made for KDE Plasma on Wayland. No telemetry; it only talks to the services you pick.") }
                    Hint { text: qsTr("Hook socket: %1").arg("$XDG_RUNTIME_DIR/lumo.sock") }
                    PillButton { text: qsTr("Quit Lumo"); onClicked: shell.quit() }
                }
            }
        }
    }
}
