import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window

// Lumo's settings: a sidebar of pages, each page a stack of grouped cards.
Window {
    id: settings
    width: 880
    height: 660
    minimumWidth: 700
    minimumHeight: 480
    title: qsTr("Settings")
    color: bg
    property int page: 0
    signal previewFinished()
    signal previewNeedsYou()
    signal previewGreeting()

    // ------------------------------------------------------------ palette
    readonly property color bg: Ui.islandLow
    readonly property color sidebar: Ui.island
    readonly property color groupBg: Ui.card
    readonly property color groupLine: Ui.line
    readonly property color track: "#2E2E36"
    readonly property color accentSoft: Qt.rgba(Ui.accent.r, Ui.accent.g, Ui.accent.b, 0.14)
    readonly property int contentMax: 680

    // 24×24 stroke icons, one per page.
    readonly property var pages: [
        { title: qsTr("General"), subtitle: qsTr("How Lumo looks, sounds and behaves."),
          icon: "M4 8h9 M17 8h3 M4 16h3 M11 16h9 M13 8a2 2 0 1 0 4 0a2 2 0 1 0 -4 0 M7 16a2 2 0 1 0 4 0a2 2 0 1 0 -4 0" },
        { title: qsTr("Dictation"), subtitle: qsTr("Hold a key, speak, let go — offline, on this computer."),
          icon: "M12 4a3 3 0 0 0 -3 3v5a3 3 0 0 0 6 0v-5a3 3 0 0 0 -3 -3z M6 11a6 6 0 0 0 12 0 M12 17v3 M9 20h6" },
        { title: qsTr("Claude Code"), subtitle: qsTr("Follow your sessions and answer them from the island."),
          icon: "M5 7l5 5l-5 5 M12 17h7" },
        { title: qsTr("Chat"), subtitle: qsTr("Talk to Claude on your subscription, or to a local model."),
          icon: "M6 5h12a2 2 0 0 1 2 2v8a2 2 0 0 1 -2 2h-7l-4 3v-3h-1a2 2 0 0 1 -2 -2v-8a2 2 0 0 1 2 -2z" },
        { title: qsTr("About"), subtitle: qsTr("A tiny friend for your AI coding agents."),
          icon: "M3 12a9 9 0 1 0 18 0a9 9 0 1 0 -18 0 M12 11v5 M12 8v0.01" }
    ]

    onPageChanged: {
        scroller.contentY = 0
        pageFade.restart()
    }

    // ------------------------------------------------------------ search
    // The sidebar's field looks through every row of every page: its title,
    // its description, its group and its page, ignoring case and accents.
    property string query: ""
    function plain(s) {
        let t = String(s || "")
        try { t = t.normalize("NFD").replace(/[\u0300-\u036f]/g, "") } catch (e) {}
        return t.toLowerCase()
    }
    function collectRows(item, out, page, group) {
        // A QML list isn't iterable everywhere: index it.
        for (let i = 0; i < item.children.length; i++) {
            const c = item.children[i]
            const g = c.isGroup === true ? c.title : group
            if (c.isSettingRow === true)
                out.push({ page: page, group: g, title: c.title, description: c.description, item: c })
            collectRows(c, out, page, g)
        }
    }
    readonly property var results: {
        const words = plain(query).split(/\s+/).filter(w => w.length > 0)
        if (words.length === 0) return []
        const rows = []
        for (let i = 0; i < pagesBox.children.length; i++) {
            const p = pagesBox.children[i]
            if (p.isPage === true) collectRows(p, rows, p.index, "")
        }
        return rows.filter(r => {
            const hay = plain([r.title, r.description, r.group, settings.pages[r.page].title].join(" "))
            return words.every(w => hay.indexOf(w) >= 0)
        })
    }
    // Opens the row's page, brings the row into view and makes it glow.
    function goTo(r) {
        searchField.text = ""
        settings.page = r.page
        revealTimer.target = r.item
        revealTimer.restart()
    }
    Timer {
        id: revealTimer
        property var target: null
        interval: 60
        onTriggered: {
            if (!target) return
            const y = target.mapToItem(pagesBox, 0, 0).y + pagesBox.y
            scroller.contentY = Math.max(0, Math.min(y - 80, scroller.contentHeight - scroller.height))
            target.flash()
        }
    }
    Shortcut { sequence: StandardKey.Find; onActivated: { searchField.forceActiveFocus(); searchField.selectAll() } }

    // ------------------------------------------------------------ building blocks

    component Toggle: Item {
        id: t
        property bool on: false
        signal toggled(bool on)
        implicitWidth: 42
        implicitHeight: 24
        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: t.on ? Ui.accent : toggleHover.hovered ? "#3A3A44" : settings.track
            Behavior on color { ColorAnimation { duration: 150 } }
        }
        Rectangle {
            width: 18; height: 18; radius: 9; y: 3
            x: t.on ? t.width - width - 3 : 3
            color: "white"
            scale: toggleTap.pressed ? 0.9 : 1
            Behavior on x { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
            Behavior on scale { NumberAnimation { duration: 90 } }
        }
        HoverHandler { id: toggleHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { id: toggleTap; onTapped: t.toggled(!t.on) }
    }

    component LumoSlider: Slider {
        id: s
        implicitWidth: 180
        implicitHeight: 24
        background: Rectangle {
            x: s.leftPadding
            y: s.topPadding + s.availableHeight / 2 - height / 2
            width: s.availableWidth
            height: 4
            radius: 2
            color: settings.track
            Rectangle { width: s.visualPosition * parent.width; height: parent.height; radius: 2; color: Ui.accent }
        }
        handle: Rectangle {
            x: s.leftPadding + s.visualPosition * (s.availableWidth - width)
            y: s.topPadding + s.availableHeight / 2 - height / 2
            width: 16; height: 16; radius: 8
            color: "white"
            border.width: s.pressed ? 4 : 0
            border.color: Qt.rgba(Ui.accent.r, Ui.accent.g, Ui.accent.b, 0.45)
            Behavior on border.width { NumberAnimation { duration: 90 } }
        }
        HoverHandler { cursorShape: Qt.PointingHandCursor }
    }

    component LumoCombo: ComboBox {
        id: c
        implicitWidth: 240
        implicitHeight: 32
        background: Rectangle {
            radius: 8
            color: c.hovered ? Ui.cardHover : Ui.card
            border.color: c.activeFocus || c.popup.visible ? Ui.accent : settings.groupLine
        }
        contentItem: Text {
            leftPadding: 12
            rightPadding: 30
            text: c.displayText
            color: Ui.text
            font.pixelSize: Ui.body
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        indicator: Icon {
            x: c.width - width - 10
            y: (c.height - height) / 2
            size: 14
            path: "M6 9l6 6l6 -6"
        }
        // Wrapped explicitly: an inline component can't rely on implicit Component conversion.
        delegate: Component {
            ItemDelegate {
                id: d
                required property int index
                required property var modelData
                width: c.width - 8
                height: 30
                contentItem: Text {
                    text: d.modelData
                    color: Ui.text
                    font.pixelSize: Ui.body
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                background: Rectangle {
                    radius: 6
                    color: c.currentIndex === d.index ? settings.accentSoft : d.hovered ? Ui.cardHover : "transparent"
                }
            }
        }
        popup: Popup {
            y: c.height + 4
            width: c.width
            implicitHeight: Math.min(contentItem.implicitHeight + 8, 260)
            padding: 4
            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: c.popup.visible ? c.delegateModel : null
                currentIndex: c.highlightedIndex
            }
            background: Rectangle { radius: 10; color: "#1B1B20"; border.color: settings.groupLine }
        }
    }

    // PillButton, outlined so it reads on the cards.
    component ActionButton: PillButton {
        id: btn
        implicitHeight: 30
        border.width: checked || primary ? (checked ? 1 : 0) : 1
        border.color: checked ? tint : btnHover.hovered ? "#45454F" : "#34343C"
        color: checked ? Qt.rgba(tint.r, tint.g, tint.b, 0.22)
                       : primary ? tint
                       : btnHover.hovered ? "#26262D" : "#1E1E24"
        HoverHandler { id: btnHover }
    }

    // A coloured dot and a word, in a soft pill: "Connected", "Not installed"…
    component StatusPill: Rectangle {
        id: pill
        property string text
        property color tint: Ui.good
        implicitWidth: pillRow.implicitWidth + 20
        implicitHeight: 24
        radius: 12
        color: Qt.rgba(tint.r, tint.g, tint.b, 0.14)
        Row {
            id: pillRow
            anchors.centerIn: parent
            spacing: 6
            Rectangle { width: 7; height: 7; radius: 3.5; color: pill.tint; anchors.verticalCenter: parent.verticalCenter }
            Text { text: pill.text; color: pill.tint; font.pixelSize: Ui.small; font.weight: Font.DemiBold }
        }
    }

    // A page: big title, subtitle, then its groups.
    component Page: ColumnLayout {
        id: pg
        readonly property bool isPage: true
        property int index
        default property alias content: pageBody.data
        visible: settings.page === index
        width: parent ? parent.width : 0
        spacing: 0
        Text {
            text: settings.pages[pg.index].title
            color: Ui.text
            font.pixelSize: 26
            font.weight: Font.Bold
        }
        Text {
            text: settings.pages[pg.index].subtitle
            color: Ui.dim
            font.pixelSize: Ui.body
            Layout.topMargin: 4
            Layout.bottomMargin: 26
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        ColumnLayout {
            id: pageBody
            Layout.fillWidth: true
            spacing: 26
        }
    }

    // A titled card of rows, with an optional note under it.
    component Group: ColumnLayout {
        id: g
        readonly property bool isGroup: true
        property string title
        property string footer
        default property alias rows: groupBody.data
        Layout.fillWidth: true
        spacing: 8
        Text {
            visible: g.title.length > 0
            text: g.title.toUpperCase()
            color: Ui.dim
            font.pixelSize: 11
            font.weight: Font.DemiBold
            font.letterSpacing: 0.9
            Layout.leftMargin: 4
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: groupBody.implicitHeight
            radius: 12
            color: settings.groupBg
            border.color: settings.groupLine
            ColumnLayout {
                id: groupBody
                width: parent.width
                spacing: 0
            }
        }
        Text {
            visible: g.footer.length > 0
            text: g.footer
            color: Ui.faint
            font.pixelSize: Ui.small
            wrapMode: Text.Wrap
            lineHeight: 1.15
            Layout.fillWidth: true
            Layout.leftMargin: 4
            Layout.rightMargin: 4
        }
    }

    // One setting: what it is on the left, its control on the right.
    component SettingRow: Item {
        id: row
        readonly property bool isSettingRow: true
        property string title
        property string description
        property color titleColor: Ui.text
        default property alias controls: rowControls.data
        // A search result was opened: glow for a moment.
        function flash() { flashAnim.restart() }
        Layout.fillWidth: true
        implicitHeight: Math.max(rowText.implicitHeight, rowControls.implicitHeight) + 26
        Rectangle {
            id: flashRect
            anchors.fill: parent
            anchors.margins: 3
            radius: 10
            color: Ui.accent
            opacity: 0
        }
        SequentialAnimation {
            id: flashAnim
            NumberAnimation { target: flashRect; property: "opacity"; to: 0.22; duration: 180 }
            PauseAnimation { duration: 700 }
            NumberAnimation { target: flashRect; property: "opacity"; to: 0; duration: 700 }
        }
        Rectangle { visible: row.y > 0; x: 16; width: parent.width - 32; height: 1; color: settings.groupLine }
        ColumnLayout {
            id: rowText
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.right: rowControls.left
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3
            Text {
                text: row.title
                color: row.titleColor
                font.pixelSize: Ui.body + 1
                font.weight: Font.Medium
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Text {
                visible: row.description.length > 0
                text: row.description
                color: Ui.dim
                font.pixelSize: Ui.small + 1
                wrapMode: Text.Wrap
                lineHeight: 1.15
                Layout.fillWidth: true
            }
        }
        RowLayout {
            id: rowControls
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
        }
    }

    // Free-form content inside a group, with the same padding and divider as a row.
    component Pane: Item {
        id: pane
        default property alias content: paneBody.data
        Layout.fillWidth: true
        implicitHeight: paneBody.implicitHeight + 32
        Rectangle { visible: pane.y > 0; x: 16; width: parent.width - 32; height: 1; color: settings.groupLine }
        ColumnLayout {
            id: paneBody
            x: 16; y: 16
            width: parent.width - 32
            spacing: 12
        }
    }

    component Note: Text {
        color: Ui.dim
        font.pixelSize: Ui.small + 1
        wrapMode: Text.Wrap
        lineHeight: 1.15
        Layout.fillWidth: true
    }

    // ------------------------------------------------------------ layout

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---------------------------------------------------- sidebar
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 224
            color: settings.sidebar
            Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: settings.groupLine }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                anchors.topMargin: 20
                spacing: 2

                RowLayout {
                    spacing: 12
                    Layout.leftMargin: 4
                    Layout.bottomMargin: 22
                    Mascot {
                        size: 40
                        alive: settings.visible
                        mood: hub.mood
                        outfit: Ui.outfitFor(shell.outfit, music.playing && shell.showMusic)
                    }
                    ColumnLayout {
                        spacing: 0
                        Text { text: "Lumo"; color: Ui.text; font.pixelSize: 18; font.weight: Font.Bold }
                        Text { text: qsTr("Settings"); color: Ui.dim; font.pixelSize: Ui.small + 1 }
                    }
                }

                TextField {
                    id: searchField
                    Layout.fillWidth: true
                    Layout.bottomMargin: 10
                    placeholderText: qsTr("Search settings")
                    placeholderTextColor: Ui.faint
                    color: Ui.text
                    font.pixelSize: Ui.body
                    leftPadding: 34
                    onTextChanged: settings.query = text
                    Keys.onEscapePressed: text = ""
                    background: Rectangle {
                        implicitHeight: 34
                        radius: 9
                        color: Ui.card
                        border.color: searchField.activeFocus ? Ui.accent : settings.groupLine
                        Icon { x: 11; anchors.verticalCenter: parent.verticalCenter; path: Ui.icons.search; tint: Ui.dim; size: 15 }
                    }
                }

                Repeater {
                    model: settings.pages
                    Rectangle {
                        id: nav
                        required property var modelData
                        required property int index
                        readonly property bool current: settings.page === index
                        Layout.fillWidth: true
                        implicitHeight: 38
                        radius: 9
                        color: current ? settings.accentSoft : navHover.hovered ? Ui.card : "transparent"
                        Behavior on color { ColorAnimation { duration: 120 } }
                        Rectangle {
                            visible: nav.current
                            width: 3; height: 18; radius: 1.5
                            anchors.verticalCenter: parent.verticalCenter
                            color: Ui.accent
                        }
                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            x: 14
                            spacing: 12
                            Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                path: nav.modelData.icon
                                tint: nav.current ? Ui.accent : Ui.dim
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: nav.modelData.title
                                color: nav.current ? Ui.text : "#C8C8D0"
                                font.pixelSize: Ui.body + 1
                                font.weight: nav.current ? Font.DemiBold : Font.Normal
                            }
                        }
                        HoverHandler { id: navHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: settings.page = nav.index }
                    }
                }

                Item { Layout.fillHeight: true }

                // Is Claude Code reporting in? The one thing worth seeing from every page.
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: footerCol.implicitHeight + 24
                    radius: 10
                    color: footerHover.hovered ? Ui.card : "transparent"
                    border.color: settings.groupLine
                    ColumnLayout {
                        id: footerCol
                        x: 12; y: 12
                        width: parent.width - 24
                        spacing: 6
                        RowLayout {
                            spacing: 8
                            Rectangle { width: 8; height: 8; radius: 4; color: installer.installed ? Ui.good : Ui.warn }
                            Text {
                                text: installer.installed ? qsTr("Claude Code connected") : qsTr("Claude Code not connected")
                                color: Ui.text
                                font.pixelSize: Ui.small + 1
                                font.weight: Font.Medium
                            }
                        }
                        Text {
                            text: qsTr("Version %1").arg(Qt.application.version || "0.1")
                            color: Ui.faint
                            font.pixelSize: Ui.small
                        }
                    }
                    HoverHandler { id: footerHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: settings.page = 2 }
                }
            }
        }

        // ---------------------------------------------------- search results
        Flickable {
            visible: settings.query.trim().length > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            contentWidth: width
            contentHeight: resultsCol.implicitHeight + 80
            ColumnLayout {
                id: resultsCol
                width: Math.min(settings.contentMax, parent.width - 80)
                x: (parent.width - width) / 2
                y: 40
                spacing: 10
                Text {
                    text: qsTr("Search")
                    color: Ui.text
                    font.pixelSize: 26
                    font.weight: Font.Bold
                }
                Text {
                    Layout.bottomMargin: 16
                    text: settings.results.length === 0 ? qsTr("Nothing matches “%1”.").arg(settings.query.trim())
                        : settings.results.length === 1 ? qsTr("1 setting matches “%1”.").arg(settings.query.trim())
                        : qsTr("%1 settings match “%2”.").arg(settings.results.length).arg(settings.query.trim())
                    color: Ui.dim
                    font.pixelSize: Ui.body
                }
                Repeater {
                    model: settings.results
                    Rectangle {
                        id: hit
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: hitCol.implicitHeight + 24
                        radius: 12
                        color: hitHover.hovered ? Ui.cardHover : settings.groupBg
                        border.color: hitHover.hovered ? Ui.accent : settings.groupLine
                        ColumnLayout {
                            id: hitCol
                            x: 16; y: 12
                            width: parent.width - 32
                            spacing: 3
                            Text {
                                text: settings.pages[hit.modelData.page].title + (hit.modelData.group ? "  ›  " + hit.modelData.group : "")
                                color: Ui.accent
                                font.pixelSize: Ui.small
                                font.weight: Font.DemiBold
                            }
                            Text {
                                Layout.fillWidth: true
                                text: hit.modelData.title
                                color: Ui.text
                                font.pixelSize: Ui.body + 1
                                font.weight: Font.Medium
                                wrapMode: Text.Wrap
                            }
                            Text {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: hit.modelData.description
                                color: Ui.dim
                                font.pixelSize: Ui.small + 1
                                wrapMode: Text.Wrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }
                        }
                        HoverHandler { id: hitHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: settings.goTo(hit.modelData) }
                    }
                }
            }
        }

        // ---------------------------------------------------- pages
        Flickable {
            id: scroller
            visible: settings.query.trim().length === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            contentWidth: width
            contentHeight: (pagesBox.children[settings.page] ? pagesBox.children[settings.page].implicitHeight : 0) + 80
            ScrollBar.vertical: ScrollBar {
                id: vbar
                policy: scroller.contentHeight > scroller.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                contentItem: Rectangle {
                    implicitWidth: 6
                    radius: 3
                    color: vbar.pressed ? "#55555F" : "#3A3A44"
                    opacity: vbar.active ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: 200 } }
                }
            }

            Item {
                id: pagesBox
                width: Math.min(settings.contentMax, scroller.width - 80)
                x: (scroller.width - width) / 2
                y: 40
                opacity: 1
                NumberAnimation on opacity { id: pageFade; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic; running: false }

                // ------------------------------------------------ General
                Page {
                    index: 0

                    Group {
                        title: qsTr("Island")
                        SettingRow {
                            title: qsTr("Start Lumo when you log in")
                            Toggle { on: shell.autostart; onToggled: (v) => shell.autostart = v }
                        }
                        SettingRow {
                            title: qsTr("Say hello when Lumo starts")
                            description: qsTr("The island opens, Lumo pops up and waves, then it gets out of your way.")
                            ActionButton { text: qsTr("Play"); onClicked: settings.previewGreeting() }
                            Toggle { on: shell.greeting; onToggled: (v) => shell.greeting = v }
                        }
                        SettingRow {
                            title: qsTr("Always show the island")
                            description: qsTr("Keep it on top of everything. The eye button in the island hides it for a while.")
                            Toggle { on: shell.alwaysShow; onToggled: (v) => shell.alwaysShow = v }
                        }
                        SettingRow {
                            title: qsTr("Screen")
                            description: qsTr("Where the island sits. If that screen is unplugged, the primary one takes it until it's back.")
                            LumoCombo {
                                readonly property var choices: [{ name: "", label: qsTr("Primary screen") }].concat(shell.screens)
                                model: choices.map(c => c.label)
                                currentIndex: Math.max(0, choices.findIndex(c => c.name === shell.screen))
                                onActivated: (i) => shell.screen = choices[i].name
                                Layout.maximumWidth: 300
                            }
                        }
                        SettingRow {
                            title: qsTr("Show Claude usage")
                            description: qsTr("Your 5-hour and weekly limits, read from your Claude account every 5 minutes.")
                            Toggle { on: shell.showUsage; onToggled: (v) => shell.showUsage = v }
                        }
                    }

                    Group {
                        title: qsTr("Music")
                        SettingRow {
                            title: qsTr("Show what's playing")
                            description: qsTr("Spotify, your browser, or any MPRIS player.")
                            Toggle { on: shell.showMusic; onToggled: (v) => shell.showMusic = v }
                        }
                        SettingRow {
                            title: qsTr("Lumo dances while music plays")
                            Toggle { on: shell.dance; onToggled: (v) => shell.dance = v }
                        }
                    }

                    Group {
                        title: qsTr("Sounds")
                        footer: qsTr("Replace a sound by dropping a WAV named hello, greet, finish, needs, error, poke, dizzy, pop, send or reply in ~/.config/lumo/sounds/.")
                        SettingRow {
                            title: qsTr("Play sounds")
                            Toggle { on: !sounds.muted; onToggled: (v) => sounds.muted = !v }
                        }
                        SettingRow {
                            title: qsTr("Volume")
                            LumoSlider { from: 0; to: 1; value: sounds.volume; onMoved: sounds.volume = value }
                            ActionButton { text: qsTr("Test"); onClicked: sounds.play("hello") }
                        }
                        SettingRow {
                            title: qsTr("Agent alerts")
                            description: qsTr("What you hear when Claude finishes, and when it needs you.")
                            ActionButton { text: qsTr("Finished"); onClicked: settings.previewFinished() }
                            ActionButton { text: qsTr("Needs you"); onClicked: settings.previewNeedsYou() }
                        }
                    }

                    Group {
                        title: qsTr("Wardrobe")
                        footer: qsTr("Auto puts Lumo's headphones on while music plays. You can also right-click Lumo in the island.")
                        Pane {
                            Wardrobe {
                                Layout.fillWidth: true
                                current: shell.outfit
                                onPicked: (outfit) => shell.outfit = outfit
                            }
                        }
                    }

                    Group {
                        title: qsTr("Shortcuts")
                        footer: qsTr("Change them in System Settings → Shortcuts → Lumo.")
                        SettingRow {
                            title: qsTr("Open the chat")
                            Keycap { text: "Meta" }
                            Keycap { text: "Alt" }
                            Keycap { text: "N" }
                        }
                        SettingRow {
                            title: qsTr("Show or hide the island")
                            Keycap { text: "Meta" }
                            Keycap { text: "Alt" }
                            Keycap { text: "I" }
                        }
                        SettingRow {
                            title: qsTr("Dictate")
                            description: dictation.keyMode === "evdev" ? qsTr("Hold, speak, let go. Tap twice for hands-free.") : qsTr("Set on the Dictation page.")
                            Keycap { text: dictation.keyLabel || qsTr("None") }
                        }
                    }

                    Note {
                        visible: !shell.layerShell
                        color: Ui.warn
                        text: qsTr("Not running on Wayland: the island uses a plain always-on-top window.")
                    }
                }

                // ------------------------------------------------ Dictation
                Page {
                    index: 1

                    Group {
                        title: qsTr("Dictation")
                        footer: dictation.keyMode !== "evdev"
                                ? qsTr("Wayland lets no app see a lone Ctrl. To hold left Ctrl alone, add yourself to the input group (sudo usermod -aG input $USER), log out and in: Lumo switches by itself. Note that any app you run can then read the keyboard.")
                                : ""
                        SettingRow {
                            title: qsTr("Dictate anywhere")
                            description: qsTr("Hold a key, speak, let go: your words are pasted where you type.")
                            Toggle { on: dictation.enabled; onToggled: (v) => dictation.enabled = v }
                        }
                        SettingRow {
                            title: qsTr("Key")
                            description: dictation.keyMode === "evdev" ? qsTr("Hold left Ctrl alone. Two quick taps for hands-free. Esc cancels.")
                                       : dictation.keyMode === "portal" ? qsTr("Hold it to talk. Tap it twice for hands-free, press again to finish.")
                                       : dictation.keyProblem || qsTr("Waiting for the desktop…")
                            Keycap { text: dictation.keyLabel || qsTr("No key yet") }
                            ActionButton { visible: dictation.keyMode === "portal"; text: qsTr("Change…"); onClicked: dictation.configureKey() }
                        }
                        SettingRow {
                            title: qsTr("Microphone")
                            LumoCombo {
                                id: micBox
                                property var mics: [{ id: "", name: qsTr("System default") }].concat(dictation.microphones())
                                model: mics.map(m => m.name)
                                currentIndex: Math.max(0, mics.findIndex(m => m.id === dictation.microphoneId))
                                onActivated: (i) => dictation.microphoneId = mics[i].id
                                onPressedChanged: if (pressed) mics = [{ id: "", name: qsTr("System default") }].concat(dictation.microphones())
                            }
                        }
                        SettingRow {
                            title: qsTr("Mute the computer while I talk")
                            description: qsTr("So only your voice is heard. The sound comes back as soon as you stop.")
                            Toggle { on: dictation.muteOutput; onToggled: (v) => dictation.muteOutput = v }
                        }
                    }

                    Group {
                        title: qsTr("Speech recognition")
                        footer: dictation.speech.modelPresent
                                ? qsTr("Stored in %1. Nothing you say leaves this computer.").arg(dictation.speech.modelDir)
                                : qsTr("Nothing you say leaves this computer.")
                        SettingRow {
                            title: qsTr("Parakeet v3")
                            description: dictation.speech.modelPresent
                                         ? qsTr("Offline, 25 languages.")
                                         : qsTr("Not installed yet — about 640 MB.")
                            StatusPill {
                                visible: dictation.speech.modelPresent
                                text: dictation.speech.loaded ? qsTr("In memory") : qsTr("Ready")
                                tint: Ui.good
                            }
                            ActionButton {
                                visible: !dictation.speech.modelPresent && !dictation.speech.downloading
                                text: qsTr("Download"); primary: true; tint: Ui.accent
                                onClicked: dictation.speech.download()
                            }
                            Text {
                                visible: dictation.speech.downloading
                                text: Math.round(dictation.speech.downloadProgress * 100) + " %"
                                color: Ui.dim
                                font.pixelSize: Ui.body
                            }
                        }
                        Pane {
                            visible: dictation.speech.downloading || dictation.speech.downloadError.length > 0
                            Rectangle {
                                visible: dictation.speech.downloading
                                Layout.fillWidth: true; implicitHeight: 6; radius: 3; color: settings.track
                                Rectangle { width: parent.width * dictation.speech.downloadProgress; height: parent.height; radius: 3; color: Ui.accent }
                            }
                            Note { visible: dictation.speech.downloadError.length > 0; color: Ui.bad; text: dictation.speech.downloadError }
                        }
                        SettingRow {
                            title: qsTr("Keep it in memory")
                            description: qsTr("Instant first dictation, for about 1 GB of RAM. Otherwise it loads while you speak and leaves after 5 idle minutes.")
                            Toggle { on: dictation.speech.keepLoaded; onToggled: (v) => dictation.speech.keepLoaded = v }
                        }
                    }

                    Group {
                        title: qsTr("Your words")
                        Pane {
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                Repeater {
                                    model: [
                                        { v: dictation.wordsToday, l: qsTr("today") },
                                        { v: dictation.totalWords, l: qsTr("in all") },
                                        { v: dictation.wordsPerMinute, l: qsTr("words / min") }
                                    ]
                                    Rectangle {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        implicitHeight: 74
                                        radius: 10
                                        color: settings.bg
                                        border.color: settings.groupLine
                                        Column {
                                            anchors.centerIn: parent
                                            spacing: 2
                                            Text {
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                text: Number(modelData.v).toLocaleString(Qt.locale(), "f", 0)
                                                color: Ui.text
                                                font.pixelSize: 24
                                                font.weight: Font.Bold
                                            }
                                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.l; color: Ui.dim; font.pixelSize: Ui.small }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Group {
                        title: qsTr("Recent")
                        SettingRow {
                            visible: dictation.history.length === 0
                            title: qsTr("Nothing dictated yet")
                            description: qsTr("Your last dictations show up here, ready to copy again.")
                        }
                        Repeater {
                            model: dictation.history.slice(0, 20)
                            Item {
                                id: hist
                                required property var modelData
                                required property int index
                                Layout.fillWidth: true
                                implicitHeight: histText.implicitHeight + histMeta.implicitHeight + 30
                                Rectangle { visible: hist.index > 0; x: 16; width: parent.width - 32; height: 1; color: settings.groupLine }
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    radius: 8
                                    color: histHover.hovered ? Ui.cardHover : "transparent"
                                }
                                HoverHandler { id: histHover }
                                Text {
                                    id: histText
                                    x: 16; y: 12
                                    width: parent.width - 32 - (histHover.hovered ? histActions.width + 12 : 0)
                                    text: hist.modelData.text
                                    color: Ui.text
                                    font.pixelSize: Ui.body
                                    wrapMode: Text.Wrap
                                }
                                Text {
                                    id: histMeta
                                    x: 16
                                    anchors.top: histText.bottom
                                    anchors.topMargin: 4
                                    text: new Date(hist.modelData.at).toLocaleString(Qt.locale(), "d MMM HH:mm") + (hist.modelData.app ? "  ·  " + hist.modelData.app : "")
                                    color: Ui.faint
                                    font.pixelSize: Ui.small
                                }
                                Row {
                                    id: histActions
                                    anchors.right: parent.right
                                    anchors.rightMargin: 14
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 6
                                    visible: histHover.hovered
                                    ActionButton { implicitHeight: 26; text: qsTr("Instinct"); onClicked: shell.sendToInstinct(hist.modelData.text) }
                                    ActionButton { implicitHeight: 26; text: qsTr("Copy"); onClicked: dictation.copy(hist.modelData.text) }
                                }
                            }
                        }
                        SettingRow {
                            visible: dictation.history.length > 0
                            title: qsTr("Clear history")
                            titleColor: Ui.dim
                            ActionButton { text: qsTr("Clear"); onClicked: dictation.clearHistory() }
                        }
                    }
                }

                // ------------------------------------------------ Claude Code
                Page {
                    index: 2

                    Group {
                        title: qsTr("Connection")
                        footer: qsTr("Lumo adds its hook (%1) to ~/.claude/settings.json so every session, tool, permission request and question reaches the island. If Lumo isn't running, the hook exits at once and Claude Code carries on as usual.").arg(installer.hookPath)
                        SettingRow {
                            title: installer.installed ? qsTr("Claude Code reports to Lumo") : qsTr("Not connected yet")
                            description: installer.installed ? qsTr("New sessions show up in the island.")
                                                             : qsTr("Connect once; you'll see every change before it's written.")
                            StatusPill {
                                text: installer.installed ? qsTr("Connected") : qsTr("Off")
                                tint: installer.installed ? Ui.good : Ui.warn
                            }
                        }
                        SettingRow {
                            visible: installer.coucouHooks
                            title: qsTr("Replace Coucou's hooks")
                            description: qsTr("Both would ask you for every permission.")
                            Toggle { id: replaceCoucou; on: true; onToggled: (v) => on = v }
                        }
                        SettingRow {
                            title: qsTr("Show plan usage")
                            description: qsTr("Your 5-hour and weekly limits, through Claude Code's status line. Your own status line keeps working.")
                            Toggle { id: statusLine; on: true; onToggled: (v) => on = v }
                        }
                        SettingRow {
                            title: qsTr("Show my cloud sessions")
                            description: qsTr("Your claude.ai/code sessions, listed every minute with your Claude login. Approvals still happen on claude.ai.")
                            Toggle { on: cloud.enabled; onToggled: (v) => cloud.enabled = v }
                        }
                        Pane {
                            RowLayout {
                                spacing: 8
                                ActionButton {
                                    text: installer.installed ? qsTr("Update connection…") : qsTr("Connect…")
                                    primary: true; tint: Ui.accent
                                    onClicked: installer.prepare(true, replaceCoucou.visible && replaceCoucou.on, statusLine.on)
                                }
                                ActionButton { visible: installer.installed; text: qsTr("Disconnect…"); onClicked: installer.prepare(false, false, false) }
                            }
                        }
                    }

                    Group {
                        visible: installer.message.length > 0 || installer.preview.length > 0
                        title: qsTr("Review the change")
                        Pane {
                            Note { visible: installer.message.length > 0; color: Ui.text; text: installer.message }
                            DiffView {
                                visible: installer.preview.length > 0
                                Layout.fillWidth: true
                                maxHeight: 280
                                lines: installer.preview
                            }
                            RowLayout {
                                visible: installer.preview.length > 0
                                spacing: 8
                                ActionButton { text: qsTr("Cancel"); onClicked: installer.cancel() }
                                ActionButton { text: qsTr("Apply"); primary: true; tint: Ui.good; onClicked: installer.apply() }
                            }
                        }
                    }
                }

                // ------------------------------------------------ Chat
                Page {
                    index: 3

                    Group {
                        title: qsTr("Assistant")
                        footer: qsTr("Claude runs on your Claude Pro or Max subscription, through the `claude` CLI you are signed in to — no API key, nothing billed per token. Ollama talks to a local server on this computer.")
                        SettingRow {
                            title: qsTr("Provider")
                            Repeater {
                                model: [{ id: "claude", n: qsTr("Claude") }, { id: "notion", n: qsTr("Notion") }, { id: "ollama", n: qsTr("Ollama") }]
                                ActionButton {
                                    required property var modelData
                                    text: modelData.n
                                    checked: chat.provider === modelData.id
                                    tint: Ui.accent
                                    onClicked: chat.provider = modelData.id
                                }
                            }
                        }
                        SettingRow {
                            title: qsTr("Model")
                            description: chat.provider === "ollama" && chat.models.length === 0 ? qsTr("No local model found. Is Ollama running?") : ""
                            LumoCombo {
                                model: chat.models
                                currentIndex: chat.models.indexOf(chat.model)
                                onActivated: (i) => chat.model = chat.models[i]
                            }
                        }
                        SettingRow {
                            visible: chat.provider === "notion"
                            title: qsTr("Let Claude edit Notion")
                            description: qsTr("Off: read-only. On: Claude may create and change pages, and says what it will do first.")
                            Toggle { on: chat.notionEdits; onToggled: (v) => chat.notionEdits = v }
                        }
                    }

                    Group {
                        title: qsTr("Instinct")
                        footer: qsTr("Instinct and Claude have no direct connection, so Lumo relays through you. “Send to Instinct” (under chat answers, finished sessions and dictations) copies the text and opens your WhatsApp chat with Instinct with it typed in — you review it and press Send. In the Notion chat, “From Instinct” finds the pages Instinct labelled “(Made by Instinct)”, read as information only.")
                        SettingRow {
                            title: qsTr("WhatsApp number")
                            TextField {
                                id: instinctField
                                implicitWidth: 220
                                implicitHeight: 32
                                text: shell.instinctNumber ? "+" + shell.instinctNumber : ""
                                placeholderText: qsTr("+212 6…")
                                placeholderTextColor: Ui.faint
                                color: Ui.text
                                font.pixelSize: Ui.body
                                leftPadding: 12
                                background: Rectangle { radius: 8; color: Ui.card; border.color: instinctField.activeFocus ? Ui.accent : settings.groupLine }
                                onAccepted: shell.instinctNumber = text
                            }
                            ActionButton { text: qsTr("Save"); primary: true; tint: Ui.accent; onClicked: shell.instinctNumber = instinctField.text }
                        }
                    }
                }

                // ------------------------------------------------ About
                Page {
                    index: 4

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: hero.implicitHeight + 56
                        radius: 16
                        gradient: Gradient {
                            GradientStop { position: 0; color: Qt.rgba(Ui.accent.r, Ui.accent.g, Ui.accent.b, 0.20) }
                            GradientStop { position: 1; color: settings.groupBg }
                        }
                        border.color: settings.groupLine
                        ColumnLayout {
                            id: hero
                            anchors.centerIn: parent
                            spacing: 6
                            // a lit stage: Lumo's light fading out behind it
                            Item {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.bottomMargin: 6
                                implicitWidth: 200; implicitHeight: 140
                                Repeater {
                                    model: [[200, 0.022], [176, 0.026], [152, 0.03], [128, 0.036], [106, 0.042]]
                                    Rectangle {
                                        required property var modelData
                                        anchors.centerIn: parent
                                        anchors.verticalCenterOffset: -6
                                        width: modelData[0]; height: width; radius: width / 2
                                        color: Ui.accent; opacity: modelData[1]
                                    }
                                }
                                Mascot {
                                    anchors.centerIn: parent
                                    size: 88
                                    alive: settings.visible && settings.page === 4
                                    mood: hub.mood
                                    outfit: Ui.outfitFor(shell.outfit, music.playing && shell.showMusic)
                                }
                            }
                            Text { Layout.alignment: Qt.AlignHCenter; text: "Lumo"; color: Ui.text; font.pixelSize: 30; font.weight: Font.Bold }
                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: qsTr("Version %1").arg(Qt.application.version || "0.1")
                                color: Ui.dim
                                font.pixelSize: Ui.body
                            }
                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.topMargin: 6
                                Layout.maximumWidth: 420
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.Wrap
                                text: qsTr("A native Qt Quick companion for your AI coding agents, made for KDE Plasma on Wayland.")
                                color: "#C8C8D0"
                                font.pixelSize: Ui.body + 1
                            }
                        }
                    }

                    Group {
                        title: qsTr("Privacy")
                        SettingRow {
                            title: qsTr("No telemetry, no account")
                            description: qsTr("Lumo only talks to what you use: Claude Code, Anthropic's usage endpoint with your Claude login, your local Ollama and your Notion connector. Nothing is sent or approved without your click.")
                        }
                        SettingRow {
                            title: qsTr("Hook socket")
                            description: "$XDG_RUNTIME_DIR/lumo.sock"
                        }
                    }

                    Group {
                        title: qsTr("Project")
                        SettingRow {
                            title: qsTr("Source code")
                            description: qsTr("MIT licence. Issues and pull requests welcome.")
                            ActionButton { text: qsTr("Open on GitHub"); onClicked: Qt.openUrlExternally("https://github.com/MoBENLAMINE/Lumo") }
                        }
                        SettingRow {
                            title: qsTr("Quit Lumo")
                            description: qsTr("The island, dictation and the hook's answers stop until you start it again.")
                            ActionButton { text: qsTr("Quit"); textColor: Ui.bad; onClicked: shell.quit() }
                        }
                    }
                }
            }
        }
    }
}
