import QtQuick
import QtQuick.Layouts
import QtQuick.Window

// A whole screen for Lumo's pages, as tiles: a big clock, Today, Music,
// System, Controls and Tools, in the order you pick, on the screen you pick.
// Esc or the × closes it; everything in it stops when it closes.
Window {
    id: dash
    title: qsTr("Lumo — Dashboard")
    color: Ui.islandLow
    flags: Qt.Window | Qt.FramelessWindowHint
    visible: false

    property Timers timers
    signal fullscreenRequested()

    function open() {
        const name = prefs.dashboardScreen
        const target = Qt.application.screens.find(s => s.name === name)
        if (target) dash.screen = target
        dash.showFullScreen()
        dash.requestActivate()
    }

    readonly property real zoom: Math.max(1, Math.min(width / 1500, height / 900))
    readonly property var tiles: prefs.dashboardTiles
    // three columns, tiles going round them
    readonly property int columns: width / zoom > 1300 ? 3 : 2

    property date now: new Date()
    Timer { running: dash.visible; interval: 1000; repeat: true; triggeredOnStart: true; onTriggered: dash.now = new Date() }

    Item {
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: dash.close()

        Item {
            width: parent.width / dash.zoom
            height: parent.height / dash.zoom
            scale: dash.zoom
            transformOrigin: Item.TopLeft

            Flickable {
                anchors.fill: parent
                anchors.margins: 32
                contentHeight: columnsRow.implicitHeight
                clip: true
                RowLayout {
                    id: columnsRow
                    width: parent.width
                    spacing: 24
                    Repeater {
                        model: dash.columns
                        ColumnLayout {
                            id: col
                            required property int index
                            Layout.preferredWidth: (columnsRow.width - (dash.columns - 1) * 24) / dash.columns
                            Layout.alignment: Qt.AlignTop
                            spacing: 24
                            Repeater {
                                model: dash.tiles.filter((t, i) => i % dash.columns === col.index)
                                Rectangle {
                                    id: tile
                                    required property string modelData
                                    Layout.fillWidth: true
                                    implicitHeight: loader.item ? loader.item.implicitHeight + 32 : 0
                                    radius: 22
                                    color: Ui.island
                                    border.color: Ui.line
                                    Loader {
                                        id: loader
                                        x: 16; y: 16
                                        width: tile.width - 32
                                        active: dash.visible
                                        sourceComponent: tile.modelData === "clock" ? clockTile
                                                       : tile.modelData === "today" ? todayTile
                                                       : tile.modelData === "media" ? mediaTile
                                                       : tile.modelData === "system" ? systemTile
                                                       : tile.modelData === "controls" ? controlsTile
                                                       : tile.modelData === "tools" ? toolsTile : null
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        IconButton {
            anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 20
            size: 36; iconSize: 16
            icon: Ui.icons.close
            tip: qsTr("Close (Esc)")
            onClicked: dash.close()
        }
    }

    Component {
        id: clockTile
        Column {
            spacing: 4
            Text {
                text: dash.now.toLocaleTimeString(Qt.locale(), Ui.timeFormat())
                color: Ui.text; font.pixelSize: 96; font.weight: Font.Bold
            }
            Text {
                text: dash.now.toLocaleDateString(Qt.locale(), "dddd d MMMM yyyy")
                font.capitalization: Font.Capitalize
                color: Ui.accent; font.pixelSize: 22; font.weight: Font.DemiBold
            }
            Row {
                visible: weather.ready
                spacing: 10
                topPadding: 12
                Icon { path: Ui.icons[weather.icon(weather.code, weather.day)]; size: 28; tint: Ui.text; anchors.verticalCenter: parent.verticalCenter }
                Text { text: weather.temperature + "°  " + weather.summary; color: Ui.dim; font.pixelSize: 18; anchors.verticalCenter: parent.verticalCenter }
            }
            Component.onCompleted: weather.want()
        }
    }
    Component { id: todayTile; TodayPage { live: true } }
    Component { id: mediaTile; MediaPage { live: true; onFullscreenRequested: dash.fullscreenRequested() } }
    Component { id: systemTile; SystemPage { live: true } }
    Component { id: controlsTile; ControlsPage { live: true } }
    Component { id: toolsTile; ToolsPage { live: true; timers: dash.timers } }
}
