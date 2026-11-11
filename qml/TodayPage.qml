import QtQuick
import QtQuick.Layouts

// Today: the date and the weather where you are, the next hours and the next
// days. The weather is fetched when this page opens, at most every 30 min.
ColumnLayout {
    id: page
    property bool live: false
    spacing: 10

    onLiveChanged: if (live) weather.want()

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
}
