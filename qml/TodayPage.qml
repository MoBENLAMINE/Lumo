import QtQuick
import QtQuick.Layouts

// Today: the date and the weather where you are, the next hours and the next
// days. The weather is fetched when this page opens, at most every 30 min.
ColumnLayout {
    id: page
    property bool live: false
    spacing: 10

    onLiveChanged: if (live) { weather.want(); calendar.want() }
    Component.onCompleted: if (live) { weather.want(); calendar.want() }
    property date picked: new Date()

    property date now: new Date()
    Timer { running: page.live; interval: 30000; repeat: true; onTriggered: page.now = new Date() }
    onVisibleChanged: now = new Date()

    function weatherIcon(code, day) { return Ui.icons[weather.icon(code, day)] }
    function degrees(v) { return v + "°" }

    // date and the weather now
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 112
        radius: 16
        color: Ui.card
        RowLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 16
            Column {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: page.now.toLocaleDateString(Qt.locale(), "dddd")
                    color: Ui.accent; font.pixelSize: Ui.body; font.weight: Font.DemiBold
                    font.capitalization: Font.Capitalize
                }
                Text {
                    text: page.now.toLocaleDateString(Qt.locale(), "d MMMM")
                    color: Ui.text; font.pixelSize: 26; font.weight: Font.DemiBold
                }
                Text {
                    text: page.now.toLocaleTimeString(Qt.locale(), Ui.timeFormat())
                    color: Ui.dim; font.pixelSize: Ui.body
                }
            }
            Rectangle { implicitWidth: 1; Layout.fillHeight: true; color: Ui.line; visible: weather.enabled }
            Item {
                visible: weather.enabled
                Layout.preferredWidth: 210
                Layout.fillHeight: true
                Row {
                    visible: weather.ready
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 12
                    Icon { path: page.weatherIcon(weather.code, weather.day); size: 44; stroke: 1.5; tint: Ui.text; anchors.verticalCenter: parent.verticalCenter }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        Text { text: page.degrees(weather.temperature); color: Ui.text; font.pixelSize: 30; font.weight: Font.DemiBold }
                        Text { text: weather.summary; color: Ui.dim; font.pixelSize: Ui.small }
                        Text {
                            text: qsTr("H %1  L %2").arg(page.degrees(weather.high)).arg(page.degrees(weather.low)) + "  ·  " + weather.city.split(",")[0]
                            color: Ui.faint; font.pixelSize: Ui.small
                        }
                    }
                }
                Text {
                    visible: !weather.ready
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.Wrap
                    text: weather.status || qsTr("Looking at the sky…")
                    color: Ui.dim; font.pixelSize: Ui.small
                }
            }
        }
    }

    // the next hours
    Rectangle {
        Layout.fillWidth: true
        visible: weather.enabled && weather.ready
        implicitHeight: 92
        radius: 16
        color: Ui.card
        Row {
            anchors.centerIn: parent
            spacing: 0
            Repeater {
                model: weather.hours.slice(0, 8)
                Column {
                    required property var modelData
                    width: (page.width - 24) / 8
                    spacing: 6
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.time; color: Ui.faint; font.pixelSize: 10 }
                    Icon { anchors.horizontalCenter: parent.horizontalCenter; path: page.weatherIcon(modelData.code, true); size: 20; tint: Ui.text }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: page.degrees(modelData.temperature); color: Ui.text; font.pixelSize: Ui.body }
                }
            }
        }
    }

    // the next days
    Rectangle {
        Layout.fillWidth: true
        visible: weather.enabled && weather.ready
        implicitHeight: daysCol.implicitHeight + 20
        radius: 16
        color: Ui.card
        Column {
            id: daysCol
            x: 14; y: 10
            width: parent.width - 28
            spacing: 4
            Repeater {
                model: weather.days.slice(1, 6)
                RowLayout {
                    required property var modelData
                    width: daysCol.width
                    spacing: 12
                    Text {
                        text: modelData.date.toLocaleDateString(Qt.locale(), "ddd")
                        color: Ui.dim; font.pixelSize: Ui.body
                        font.capitalization: Font.Capitalize
                        Layout.preferredWidth: 50
                    }
                    Icon { path: page.weatherIcon(modelData.code, true); size: 18; tint: Ui.text }
                    Text { text: weather.describe(modelData.code); color: Ui.dim; font.pixelSize: Ui.small; Layout.fillWidth: true; elide: Text.ElideRight }
                    Text { text: page.degrees(modelData.low); color: Ui.faint; font.pixelSize: Ui.body; Layout.preferredWidth: 34; horizontalAlignment: Text.AlignRight }
                    Text { text: page.degrees(modelData.high); color: Ui.text; font.pixelSize: Ui.body; Layout.preferredWidth: 34; horizontalAlignment: Text.AlignRight }
                }
            }
        }
    }

    // ---- the month: holidays and your events
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: monthCol.implicitHeight + 24
        radius: 16
        color: Ui.card
        ColumnLayout {
            id: monthCol
            x: 14; y: 12
            width: parent.width - 28
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: calendar.month.toLocaleDateString(Qt.locale(), "MMMM yyyy")
                    font.capitalization: Font.Capitalize
                    color: Ui.text; font.pixelSize: Ui.title; font.weight: Font.DemiBold
                }
                IconButton { size: 26; iconSize: 14; icon: "M15 6l-6 6l6 6"; onClicked: calendar.shiftMonth(-1) }
                IconButton { size: 26; iconSize: 14; icon: Ui.icons.chevronRight; onClicked: calendar.shiftMonth(1) }
            }
            Grid {
                id: grid
                Layout.fillWidth: true
                columns: 7
                readonly property real cell: width / 7
                readonly property int firstDay: Qt.locale().firstDayOfWeek === 0 ? 7 : Qt.locale().firstDayOfWeek   // 1 = Monday
                readonly property int lead: (calendar.month.getDay() === 0 ? 7 : calendar.month.getDay()) - firstDay
                // weekday initials
                Repeater {
                    model: 7
                    Text {
                        required property int index
                        width: grid.cell; height: 20
                        horizontalAlignment: Text.AlignHCenter
                        text: Qt.locale().dayName(((grid.firstDay - 1 + index) % 7 + 1) % 7, Locale.ShortFormat).replace(".", "").slice(0, 2)
                        color: Ui.faint; font.pixelSize: Ui.small
                    }
                }
                Repeater {
                    model: 42
                    Item {
                        id: cell
                        required property int index
                        readonly property date day: new Date(calendar.month.getFullYear(), calendar.month.getMonth(), 1 + index - ((grid.lead + 7) % 7))
                        readonly property bool inMonth: day.getMonth() === calendar.month.getMonth()
                        readonly property var events: inMonth ? calendar.events.filter(e => e.date.getDate() === day.getDate()) : []
                        readonly property bool today: day.toDateString() === new Date().toDateString()
                        readonly property bool picked: day.toDateString() === page.picked.toDateString()
                        visible: index < 35 || ((grid.lead + 7) % 7) + new Date(calendar.month.getFullYear(), calendar.month.getMonth() + 1, 0).getDate() > 35
                        width: grid.cell; height: 34
                        Rectangle {
                            anchors.centerIn: parent
                            width: 30; height: 30; radius: 15
                            color: cell.today ? Ui.accent : cell.picked ? Ui.line : cellHover.hovered && cell.inMonth ? Ui.cardHover : "transparent"
                            border.width: cell.events.some(e => e.holiday) && !cell.today ? 1 : 0
                            border.color: Ui.warn
                        }
                        Text {
                            anchors.centerIn: parent
                            text: cell.day.getDate()
                            color: cell.today ? "#0A0A0C" : cell.inMonth ? Ui.text : Ui.faint
                            opacity: cell.inMonth ? 1 : 0.5
                            font.pixelSize: Ui.small
                            font.weight: cell.today ? Font.Bold : Font.Normal
                        }
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom; anchors.bottomMargin: 1
                            spacing: 2
                            Repeater {
                                model: Math.min(3, cell.events.filter(e => !e.holiday).length)
                                Rectangle { width: 4; height: 4; radius: 2; color: Ui.info }
                            }
                        }
                        HoverHandler { id: cellHover; cursorShape: cell.inMonth ? Qt.PointingHandCursor : Qt.ArrowCursor }
                        TapHandler { enabled: cell.inMonth; onTapped: page.picked = cell.day }
                    }
                }
            }
            // the picked day
            Text {
                text: page.picked.toLocaleDateString(Qt.locale(), "dddd d MMMM")
                font.capitalization: Font.Capitalize
                color: Ui.dim; font.pixelSize: Ui.small; font.weight: Font.DemiBold
            }
            Repeater {
                model: calendar.events.filter(e => e.date.toDateString() === page.picked.toDateString())
                RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 10
                    Rectangle { implicitWidth: 3; implicitHeight: 18; radius: 1.5; color: modelData.holiday ? Ui.warn : Ui.info }
                    Text { text: modelData.allDay ? qsTr("All day") : modelData.time; color: Ui.dim; font.pixelSize: Ui.small; Layout.preferredWidth: 64 }
                    Text { text: modelData.title; color: Ui.text; font.pixelSize: Ui.body; elide: Text.ElideRight; Layout.fillWidth: true }
                }
            }
            Text {
                visible: calendar.events.filter(e => e.date.toDateString() === page.picked.toDateString()).length === 0
                text: calendar.links.length ? qsTr("Nothing that day.") : qsTr("Nothing that day. Add your calendar's private link in Settings → Widgets → Calendar.")
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Ui.faint; font.pixelSize: Ui.small
            }
            Text {
                visible: calendar.status.length > 0
                text: calendar.status
                color: Ui.warn; font.pixelSize: Ui.small
            }
        }
    }
}
