import QtQuick
import QtQuick.Layouts

// One agent session. Folded: what it's doing now and a one-line summary.
// Unfolded: what you asked, Claude's answer, the files it changed (click for
// the diff) and a timeline of its steps.
Rectangle {
    id: card
    required property string sessionId
    required property string agentName
    required property string agentColor
    required property string project
    required property string status
    required property string activity
    required property string prompt
    required property int added
    required property int removed
    required property int tools
    required property var startedAt
    required property var updatedAt
    required property string lastMessage
    required property var steps
    required property var diffs
    required property string url

    property bool open: false
    // Set by the island: false while the panel is hidden, so nothing ticks.
    property bool live: true
    property string openFile: ""
    property bool allSteps: false

    readonly property bool busy: status === "working" || status === "waiting"

    // One row per file: edits to the same file add up; the newest diff is kept.
    readonly property var files: {
        const byPath = {}, order = []
        for (const d of diffs) {
            const key = d.path || d.file
            if (!byPath[key]) { byPath[key] = { file: d.file, path: key, added: 0, removed: 0, lines: d.lines, edits: 0 }; order.push(key) }
            byPath[key].added += d.added
            byPath[key].removed += d.removed
            byPath[key].edits += 1
        }
        return order.map(k => byPath[k])
    }
    readonly property var shownSteps: allSteps ? steps : steps.slice(0, 6)

    function duration() {
        const s = Math.max(0, Math.round(((busy ? Date.now() : updatedAt) - startedAt) / 1000))
        return s < 60 ? qsTr("%1 s").arg(s) : s < 3600 ? qsTr("%1 min").arg(Math.round(s / 60)) : qsTr("%1 h %2").arg(Math.floor(s / 3600)).arg(Math.round(s % 3600 / 60))
    }
    function tag(icon, kind) {
        if (kind === "prompt") return { t: qsTr("You"), c: Ui.accent }
        if (kind === "done") return { t: qsTr("Done"), c: Ui.good }
        const m = {
            Read: [qsTr("Read"), "#7AA2F7"], Edit: [qsTr("Edit"), "#F5A65B"], MultiEdit: [qsTr("Edit"), "#F5A65B"],
            Write: [qsTr("Write"), "#F5A65B"], NotebookEdit: [qsTr("Edit"), "#F5A65B"], Bash: [qsTr("Run"), "#B48CFF"],
            Grep: [qsTr("Search"), "#4FD1C5"], Glob: [qsTr("Search"), "#4FD1C5"], WebFetch: [qsTr("Web"), "#7DD3FC"],
            WebSearch: [qsTr("Web"), "#7DD3FC"], Task: [qsTr("Agent"), "#FF8FB1"], Agent: [qsTr("Agent"), "#FF8FB1"],
            TodoWrite: [qsTr("Plan"), "#C9CDD4"]
        }[icon] || [icon && icon.startsWith("mcp__") ? qsTr("Tool") : (icon || qsTr("Step")), "#9A9AA5"]
        return { t: m[0], c: kind === "fail" ? Ui.bad : m[1] }
    }

    width: ListView.view ? ListView.view.width : 400
    implicitHeight: col.implicitHeight + 24
    radius: 14
    color: hover.hovered && !open ? Ui.cardHover : Ui.card
    border.width: 1
    border.color: status === "waiting" ? Qt.rgba(Ui.warn.r, Ui.warn.g, Ui.warn.b, 0.5) : open ? Ui.line : "transparent"
    Behavior on color { ColorAnimation { duration: 120 } }
    Behavior on implicitHeight { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
    clip: true

    HoverHandler { id: hover }

    // Re-read the clock now and then while it runs, for the duration.
    property int tick: 0
    Timer { running: card.live && card.busy; interval: 10000; repeat: true; onTriggered: card.tick++ }

    ColumnLayout {
        id: col
        x: 14; y: 12
        width: parent.width - 28
        spacing: 10

        // ------------------------------------------------ header (click to fold)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            TapHandler { onTapped: card.open = !card.open }

            RowLayout {
                spacing: 8
                Layout.fillWidth: true

                Mascot {
                    size: 24
                    color: card.agentColor
                    mood: card.status === "working" ? "working" : card.status === "error" ? "sad" : card.status === "waiting" ? "alert" : "idle"
                    alive: card.live && card.busy
                    badge: card.status === "waiting"
                }
                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true
                    Text {
                        text: card.project || card.agentName
                        color: Ui.text
                        font.pixelSize: Ui.body
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: card.agentName
                        color: Ui.faint
                        font.pixelSize: 10
                    }
                }
                Rectangle {
                    implicitWidth: statusText.implicitWidth + 16
                    implicitHeight: 20
                    radius: 10
                    color: Qt.rgba(Ui.statusColor(card.status).r, Ui.statusColor(card.status).g, Ui.statusColor(card.status).b, 0.16)
                    Text {
                        id: statusText
                        anchors.centerIn: parent
                        color: Ui.statusColor(card.status)
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        text: ({ working: qsTr("Working"), waiting: qsTr("Needs you"), done: qsTr("Done"), error: qsTr("Error"), idle: qsTr("Waiting for you") })[card.status] || card.status
                    }
                }
                Rectangle { // bring the session's terminal or editor forward
                    implicitWidth: 26; implicitHeight: 26; radius: 13
                    color: jumpHover.hovered ? Ui.line : "transparent"
                    Text { anchors.centerIn: parent; text: "↗"; color: jumpHover.hovered ? Ui.text : Ui.dim; font.pixelSize: 14 }
                    HoverHandler { id: jumpHover; cursorShape: Qt.PointingHandCursor }
                    // A cloud session has no process here: its card opens it on claude.ai.
                    TapHandler {
                        onTapped: {
                            if (card.url.length) Qt.openUrlExternally(card.url)
                            else activeWindow.focusProcess(hub.sessionPids(card.sessionId))
                        }
                    }
                }
                Rectangle {
                    implicitWidth: 26; implicitHeight: 26; radius: 13
                    color: closeHover.hovered ? Ui.line : "transparent"
                    Text { anchors.centerIn: parent; text: "✕"; color: closeHover.hovered ? Ui.text : Ui.faint; font.pixelSize: 11 }
                    HoverHandler { id: closeHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: hub.dismissSession(card.sessionId) }
                }
            }

            // now
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Rectangle {
                    width: 7; height: 7; radius: 3.5
                    color: Ui.statusColor(card.status)
                    SequentialAnimation on opacity {
                        running: card.live && card.status === "working"
                        loops: Animation.Infinite
                        OpacityAnimator { from: 1; to: 0.25; duration: 700; easing.type: Easing.InOutSine }
                        OpacityAnimator { from: 0.25; to: 1; duration: 700; easing.type: Easing.InOutSine }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: card.status === "done" ? qsTr("Finished") : card.status === "idle" ? qsTr("Waiting for your next message") : card.activity
                    color: card.busy ? Ui.text : Ui.dim
                    font.pixelSize: Ui.body
                    elide: Text.ElideRight
                }
                Text {
                    text: card.open ? "▴" : "▾"
                    color: Ui.faint
                    font.pixelSize: 11
                }
            }

            // summary
            Text {
                Layout.fillWidth: true
                textFormat: Text.StyledText
                color: Ui.faint
                font.pixelSize: Ui.small
                elide: Text.ElideRight
                text: {
                    card.tick
                    const parts = [card.tools === 1 ? qsTr("1 step") : qsTr("%1 steps").arg(card.tools)]
                    if (card.files.length) parts.push(card.files.length === 1 ? qsTr("1 file") : qsTr("%1 files").arg(card.files.length))
                    if (card.added + card.removed) parts.push("<font color='#7EE2A8'>+" + card.added + "</font> <font color='#FF9A9A'>−" + card.removed + "</font>")
                    parts.push(card.duration())
                    return parts.join("  ·  ")
                }
            }
        }

        // ------------------------------------------------ unfolded
        // what you asked
        Rectangle {
            visible: card.open && card.prompt.length > 0
            Layout.fillWidth: true
            implicitHeight: askCol.implicitHeight + 16
            radius: 10
            color: Qt.rgba(Ui.accent.r, Ui.accent.g, Ui.accent.b, 0.10)
            ColumnLayout {
                id: askCol
                x: 10; y: 8
                width: parent.width - 20
                spacing: 3
                Text { text: qsTr("You asked"); color: Ui.accent; font.pixelSize: 10; font.weight: Font.DemiBold }
                Text {
                    Layout.fillWidth: true
                    text: card.prompt
                    color: Ui.text
                    font.pixelSize: Ui.small
                    wrapMode: Text.Wrap
                    maximumLineCount: 5
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }
            }
        }

        // Claude's answer
        Rectangle {
            visible: card.open && card.lastMessage.length > 0
            Layout.fillWidth: true
            implicitHeight: ansCol.implicitHeight + 16
            radius: 10
            color: "#121216"
            ColumnLayout {
                id: ansCol
                x: 10; y: 8
                width: parent.width - 20
                spacing: 3
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("%1 answered").arg(card.agentName); color: Ui.good; font.pixelSize: 10; font.weight: Font.DemiBold; Layout.fillWidth: true }
                    InstinctButton {
                        payload: (card.project ? card.project + " — " : "") + qsTr("I asked: ") + card.prompt + "\n\n" + card.lastMessage
                                 + (card.files.length ? "\n\n" + qsTr("Files changed: ") + card.files.map(f => f.file).join(", ") : "")
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: card.lastMessage
                    color: Ui.text
                    font.pixelSize: Ui.small
                    wrapMode: Text.Wrap
                    maximumLineCount: 10
                    elide: Text.ElideRight
                    textFormat: Text.MarkdownText
                }
            }
        }

        // files changed
        ColumnLayout {
            visible: card.open && card.files.length > 0
            Layout.fillWidth: true
            spacing: 4
            Text { text: qsTr("Files changed"); color: Ui.dim; font.pixelSize: 10; font.weight: Font.DemiBold }
            Repeater {
                model: card.open ? card.files : []
                delegate: ColumnLayout {
                    id: fileRow
                    required property var modelData
                    readonly property bool shown: card.openFile === modelData.path
                    Layout.fillWidth: true
                    spacing: 4
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 28
                        radius: 7
                        color: fileHover.hovered || fileRow.shown ? Ui.cardHover : "#121216"
                        HoverHandler { id: fileHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: card.openFile = fileRow.shown ? "" : fileRow.modelData.path }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8
                            Text { text: fileRow.shown ? "▾" : "▸"; color: Ui.faint; font.pixelSize: 10 }
                            Text {
                                Layout.fillWidth: true
                                text: fileRow.modelData.file
                                color: Ui.text
                                font.pixelSize: Ui.small
                                font.family: Ui.mono
                                elide: Text.ElideMiddle
                            }
                            Text {
                                visible: fileRow.modelData.edits > 1
                                text: qsTr("%1 edits").arg(fileRow.modelData.edits)
                                color: Ui.faint
                                font.pixelSize: 10
                            }
                            Text {
                                textFormat: Text.StyledText
                                text: "<font color='#7EE2A8'>+" + fileRow.modelData.added + "</font>  <font color='#FF9A9A'>−" + fileRow.modelData.removed + "</font>"
                                font.pixelSize: Ui.small
                                font.family: Ui.mono
                            }
                        }
                    }
                    DiffView {
                        visible: fileRow.shown
                        Layout.fillWidth: true
                        lines: fileRow.shown ? fileRow.modelData.lines : []
                    }
                }
            }
        }

        // steps
        ColumnLayout {
            visible: card.open && card.steps.length > 0
            Layout.fillWidth: true
            spacing: 2
            RowLayout {
                Layout.fillWidth: true
                Text { text: qsTr("Steps"); color: Ui.dim; font.pixelSize: 10; font.weight: Font.DemiBold; Layout.fillWidth: true }
                Text {
                    visible: card.steps.length > 6
                    text: card.allSteps ? qsTr("Show fewer") : qsTr("Show all %1").arg(card.steps.length)
                    color: moreHover.hovered ? Ui.text : Ui.info
                    font.pixelSize: 10
                    HoverHandler { id: moreHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: card.allSteps = !card.allSteps }
                }
            }
            Repeater {
                model: card.open ? card.shownSteps : []
                delegate: RowLayout {
                    id: stepRow
                    required property var modelData
                    required property int index
                    readonly property var t: card.tag(modelData.icon, modelData.kind)
                    Layout.fillWidth: true
                    spacing: 8
                    // timeline rail
                    Item {
                        implicitWidth: 10
                        implicitHeight: 24
                        Rectangle { x: 4; width: 2; height: parent.height; color: Ui.line; visible: stepRow.index < card.shownSteps.length - 1; y: 12 }
                        Rectangle { x: 1; y: 8; width: 8; height: 8; radius: 4; color: stepRow.t.c }
                    }
                    Rectangle {
                        implicitWidth: Math.max(44, tagText.implicitWidth + 12)
                        implicitHeight: 18
                        radius: 5
                        color: Qt.rgba(Qt.color(stepRow.t.c).r, Qt.color(stepRow.t.c).g, Qt.color(stepRow.t.c).b, 0.16)
                        Text { id: tagText; anchors.centerIn: parent; text: stepRow.t.t; color: stepRow.t.c; font.pixelSize: 10; font.weight: Font.DemiBold }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: stepRow.modelData.text
                        color: stepRow.modelData.kind === "prompt" ? Ui.text : Ui.dim
                        font.pixelSize: Ui.small
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }
                    Text { text: Ui.ago(stepRow.modelData.at); color: Ui.faint; font.pixelSize: 10 }
                }
            }
        }
    }
}
