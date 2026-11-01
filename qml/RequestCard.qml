import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// A permission request (Allow / Deny / Always) or an AskUserQuestion.
Rectangle {
    id: card
    required property string requestId
    required property string agentColor
    required property string project
    required property string kind
    required property string tool
    required property string title
    required property string detail
    required property var questions
    required property bool canAlways

    // question text -> label (single) or [labels] (multi)
    property var picks: ({})
    property bool live: true
    readonly property bool complete: {
        if (kind !== "question") return false
        for (let q of questions) {
            const p = picks[q.question]
            if (p === undefined || (Array.isArray(p) && p.length === 0)) return false
        }
        return questions.length > 0
    }

    function pick(q, label) {
        const next = Object.assign({}, picks)
        if (q.multiSelect) {
            const cur = (next[q.question] || []).slice()
            const i = cur.indexOf(label)
            if (i >= 0) cur.splice(i, 1); else cur.push(label)
            next[q.question] = cur
        } else {
            next[q.question] = label
        }
        picks = next
        // A single one-choice question answers itself.
        if (questions.length === 1 && !q.multiSelect) hub.answer(requestId, picks)
    }
    function picked(q, label) {
        const p = picks[q.question]
        return Array.isArray(p) ? p.indexOf(label) >= 0 : p === label
    }

    width: ListView.view ? ListView.view.width : 400
    implicitHeight: col.implicitHeight + 24
    radius: 14
    color: Ui.card
    border.width: 1
    border.color: Qt.rgba(Ui.warn.r, Ui.warn.g, Ui.warn.b, 0.55)

    // gentle glow pulse while waiting
    SequentialAnimation on border.color {
        running: card.live
        loops: Animation.Infinite
        ColorAnimation { to: Qt.rgba(Ui.warn.r, Ui.warn.g, Ui.warn.b, 0.95); duration: 900; easing.type: Easing.InOutSine }
        ColorAnimation { to: Qt.rgba(Ui.warn.r, Ui.warn.g, Ui.warn.b, 0.35); duration: 900; easing.type: Easing.InOutSine }
    }

    ColumnLayout {
        id: col
        x: 14; y: 12
        width: parent.width - 28
        spacing: 10

        RowLayout {
            spacing: 8
            Rectangle { width: 8; height: 8; radius: 4; color: card.agentColor }
            Text { text: card.project; color: Ui.dim; font.pixelSize: Ui.small; visible: card.project.length > 0 }
            Item { Layout.fillWidth: true }
            Text { text: card.tool; color: Ui.faint; font.pixelSize: Ui.small; font.family: Ui.mono }
        }

        Text {
            text: card.title
            color: Ui.text
            font.pixelSize: Ui.title
            font.weight: Font.DemiBold
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            visible: card.kind !== "question"
        }

        // ---- permission detail
        Rectangle {
            visible: card.kind !== "question" && card.kind !== "plan" && card.detail.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(150, detailText.implicitHeight + 16)
            radius: 8
            color: "#0F0F12"
            border.color: Ui.line
            clip: true
            Flickable {
                anchors.fill: parent
                anchors.margins: 8
                contentHeight: detailText.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                Text {
                    id: detailText
                    width: parent.width
                    text: card.detail
                    color: Ui.text
                    font.family: Ui.mono
                    font.pixelSize: 12
                    wrapMode: Text.WrapAnywhere
                    textFormat: Text.PlainText
                }
            }
        }

        RowLayout {
            visible: card.kind === "permission"
            Layout.fillWidth: true
            spacing: 8
            PillButton { text: qsTr("Terminal"); textColor: Ui.dim; onClicked: hub.replyInTerminal(card.requestId) }
            Item { Layout.fillWidth: true }
            PillButton { text: qsTr("Deny"); tint: Ui.bad; textColor: Ui.bad; onClicked: hub.deny(card.requestId) }
            PillButton { text: qsTr("Always"); visible: card.canAlways; tint: Ui.good; textColor: Ui.good; onClicked: hub.always(card.requestId) }
            PillButton { text: qsTr("Allow"); primary: true; tint: Ui.good; onClicked: hub.allow(card.requestId) }
        }
        // ---- a plan to approve
        Rectangle {
            visible: card.kind === "plan"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(300, planText.implicitHeight + 20)
            radius: 10
            color: "#101014"
            border.color: Ui.line
            clip: true
            Flickable {
                anchors.fill: parent
                anchors.margins: 10
                contentHeight: planText.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                TextEdit {
                    id: planText
                    width: parent.width
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.MarkdownText
                    text: card.kind === "plan" ? card.detail : ""
                    color: Ui.text
                    font.pixelSize: Ui.small
                }
            }
        }
        Rectangle {
            visible: card.kind === "plan"
            Layout.fillWidth: true
            implicitHeight: 34
            radius: 17
            color: "#121216"
            border.color: feedback.activeFocus ? Ui.accent : Ui.line
            TextField {
                id: feedback
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                placeholderText: qsTr("What should change? (optional, sent with “Keep planning”)")
                placeholderTextColor: Ui.faint
                color: Ui.text
                font.pixelSize: Ui.small
                background: null
                // The island only takes the keyboard when you type in it.
                TapHandler { onTapped: { shell.setKeyboard(true); feedback.forceActiveFocus() } }
                Keys.onReturnPressed: hub.keepPlanning(card.requestId, feedback.text)
            }
        }
        RowLayout {
            visible: card.kind === "plan"
            Layout.fillWidth: true
            spacing: 8
            PillButton { text: qsTr("Terminal"); textColor: Ui.dim; onClicked: hub.replyInTerminal(card.requestId) }
            Item { Layout.fillWidth: true }
            PillButton { text: qsTr("Keep planning"); tint: Ui.warn; textColor: Ui.warn; onClicked: hub.keepPlanning(card.requestId, feedback.text) }
            PillButton { text: qsTr("Approve + auto-edit"); visible: card.canAlways; tint: Ui.good; textColor: Ui.good; onClicked: hub.always(card.requestId) }
            PillButton { text: qsTr("Approve"); primary: true; tint: Ui.good; onClicked: hub.allow(card.requestId) }
        }

        RowLayout {
            visible: card.kind === "notice"
            Layout.fillWidth: true
            Text { text: qsTr("Answer it in the terminal."); color: Ui.dim; font.pixelSize: Ui.small; Layout.fillWidth: true }
            PillButton { text: qsTr("OK"); onClicked: hub.replyInTerminal(card.requestId) }
        }

        // ---- questions
        Repeater {
            model: card.kind === "question" ? card.questions : []
            delegate: ColumnLayout {
                id: qcol
                required property var modelData
                Layout.fillWidth: true
                spacing: 6
                Text {
                    text: (qcol.modelData.header ? qcol.modelData.header + " · " : "") + qcol.modelData.question
                    color: Ui.text
                    font.pixelSize: Ui.body
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                Text {
                    visible: qcol.modelData.multiSelect
                    text: qsTr("Pick one or more")
                    color: Ui.faint
                    font.pixelSize: 10
                }
                Repeater {
                    model: qcol.modelData.options
                    delegate: Rectangle {
                        id: opt
                        required property var modelData
                        readonly property bool on: { card.picks; return card.picked(qcol.modelData, opt.modelData.label) }
                        Layout.fillWidth: true
                        implicitHeight: optCol.implicitHeight + 14
                        radius: 9
                        color: on ? Qt.rgba(Ui.accent.r, Ui.accent.g, Ui.accent.b, 0.22) : optHover.hovered ? Ui.cardHover : "#121216"
                        border.width: 1
                        border.color: on ? Ui.accent : Ui.line
                        scale: optTap.pressed ? 0.98 : 1
                        Behavior on scale { NumberAnimation { duration: 80 } }
                        Behavior on color { ColorAnimation { duration: 120 } }
                        HoverHandler { id: optHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { id: optTap; onTapped: card.pick(qcol.modelData, opt.modelData.label) }
                        Column {
                            id: optCol
                            x: 10; y: 7
                            width: parent.width - 20
                            spacing: 2
                            Text { text: opt.modelData.label; color: Ui.text; font.pixelSize: Ui.body; width: parent.width; wrapMode: Text.Wrap }
                            Text {
                                visible: !!opt.modelData.description
                                text: opt.modelData.description || ""
                                color: Ui.dim; font.pixelSize: Ui.small; width: parent.width; wrapMode: Text.Wrap
                            }
                        }
                    }
                }
            }
        }
        RowLayout {
            visible: card.kind === "question"
            Layout.fillWidth: true
            PillButton { text: qsTr("Reply in terminal"); textColor: Ui.dim; onClicked: hub.replyInTerminal(card.requestId) }
            Item { Layout.fillWidth: true }
            PillButton {
                text: qsTr("Send")
                primary: true
                tint: Ui.accent
                enabledLook: card.complete
                visible: card.questions.length > 1 || (card.questions.length === 1 && card.questions[0].multiSelect)
                onClicked: hub.answer(card.requestId, card.picks)
            }
        }
    }
}
