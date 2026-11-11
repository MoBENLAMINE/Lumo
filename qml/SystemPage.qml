import QtQuick
import QtQuick.Layouts

// CPU, GPU and memory with their last 40 seconds, network speed, the busiest
// programs, and the addresses. Sampled every second only while it shows.
ColumnLayout {
    id: page
    property bool live: false
    spacing: 10

    onLiveChanged: systemStats.live = live
    Component.onDestruction: systemStats.live = false

    component Gauge: Rectangle {
        id: g
        property string icon
        property string label
        property real value: 0      // 0..100, < 0 unknown
        property string detail
        property var history: []
        property color tint: Ui.accent
        Layout.fillWidth: true
        implicitHeight: 92
        radius: 14
        color: Ui.card
        clip: true
        Sparkline {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 40
            values: g.history
            max: 100
            tint: g.tint
            visible: g.history.length > 1
        }
        Row {
            x: 12; y: 10
            spacing: 6
            Icon { path: g.icon; size: 14; tint: Ui.dim; anchors.verticalCenter: parent.verticalCenter }
            Text { text: g.label; color: Ui.dim; font.pixelSize: Ui.small; anchors.verticalCenter: parent.verticalCenter }
        }
        Text {
            x: 12; y: 28
            text: g.value < 0 ? "—" : Math.round(g.value) + "%"
            color: Ui.text
            font.pixelSize: 22
            font.weight: Font.DemiBold
        }
        Text {
            anchors.right: parent.right; anchors.rightMargin: 12; y: 12
            text: g.detail
            color: Ui.faint
            font.pixelSize: Ui.small
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        Gauge {
            icon: Ui.icons.cpu
            label: qsTr("CPU")
            value: systemStats.cpu
            history: systemStats.cpuHistory
        }
        Gauge {
            icon: Ui.icons.system
            label: systemStats.gpuName ? qsTr("GPU · %1").arg(systemStats.gpuName) : qsTr("GPU")
            value: systemStats.gpu
            tint: Ui.info
        }
        Gauge {
            icon: Ui.icons.memory
            label: qsTr("Memory")
            value: systemStats.memory
            detail: systemStats.size(systemStats.memoryUsed) + " / " + systemStats.size(systemStats.memoryTotal)
            history: systemStats.memoryHistory
            tint: Ui.good
        }
    }

    // network
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 104
        radius: 14
        color: Ui.card
        clip: true
        Sparkline {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 44
            values: systemStats.downloadHistory
            tint: Ui.info
        }
        Sparkline {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 44
            values: systemStats.uploadHistory
            max: Math.max.apply(null, systemStats.downloadHistory.concat(systemStats.uploadHistory, [1]))
            tint: Ui.accent
        }
        Row {
            x: 12; y: 10
            spacing: 6
            Icon { path: Ui.icons.globe; size: 14; tint: Ui.dim; anchors.verticalCenter: parent.verticalCenter }
            Text { text: qsTr("Network"); color: Ui.dim; font.pixelSize: Ui.small; anchors.verticalCenter: parent.verticalCenter }
        }
        Row {
            x: 12; y: 30
            spacing: 18
            Row {
                spacing: 4
                Icon { path: Ui.icons.down; size: 14; tint: Ui.info; anchors.verticalCenter: parent.verticalCenter }
                Text { text: systemStats.rate(systemStats.download); color: Ui.text; font.pixelSize: Ui.title; font.weight: Font.DemiBold }
            }
            Row {
                spacing: 4
                Icon { path: Ui.icons.up; size: 14; tint: Ui.accent; anchors.verticalCenter: parent.verticalCenter }
                Text { text: systemStats.rate(systemStats.upload); color: Ui.text; font.pixelSize: Ui.title; font.weight: Font.DemiBold }
            }
        }
        Column {
            anchors.right: parent.right; anchors.rightMargin: 12; y: 10
            spacing: 2
            Text {
                anchors.right: parent.right
                visible: systemStats.localIp.length > 0
                text: qsTr("Local %1").arg(systemStats.localIp)
                color: Ui.dim; font.pixelSize: Ui.small
            }
            Text {
                anchors.right: parent.right
                visible: systemStats.showPublicIp && systemStats.publicIp.length > 0
                text: qsTr("Public %1").arg(systemStats.publicIp)
                color: Ui.dim; font.pixelSize: Ui.small
            }
        }
    }

    // the busiest programs
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: procCol.implicitHeight + 20
        radius: 14
        color: Ui.card
        Column {
            id: procCol
            x: 12; y: 10
            width: parent.width - 24
            spacing: 6
            RowLayout {
                width: parent.width
                Text { text: qsTr("Busiest programs"); color: Ui.dim; font.pixelSize: Ui.small; Layout.fillWidth: true }
                Text { text: qsTr("Up %1").arg(systemStats.uptime); color: Ui.faint; font.pixelSize: Ui.small }
            }
            Repeater {
                model: systemStats.processes
                RowLayout {
                    required property var modelData
                    width: procCol.width
                    spacing: 10
                    Text { text: modelData.name; color: Ui.text; font.pixelSize: Ui.body; elide: Text.ElideRight; Layout.fillWidth: true }
                    Rectangle {
                        implicitWidth: 80; implicitHeight: 5; radius: 2.5
                        color: Ui.line
                        Rectangle { width: parent.width * Math.min(1, modelData.cpu / 100); height: parent.height; radius: 2.5; color: Ui.accent }
                    }
                    Text { text: modelData.cpu.toFixed(0) + "%"; color: Ui.dim; font.pixelSize: Ui.small; Layout.preferredWidth: 34; horizontalAlignment: Text.AlignRight }
                    Text { text: systemStats.size(modelData.memory); color: Ui.faint; font.pixelSize: Ui.small; Layout.preferredWidth: 58; horizontalAlignment: Text.AlignRight }
                }
            }
            Text {
                visible: systemStats.processes.length === 0
                text: qsTr("Measuring…")
                color: Ui.faint
                font.pixelSize: Ui.small
            }
        }
    }
}
