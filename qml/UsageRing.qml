import QtQuick
import QtQuick.Shapes

// One plan-usage window (5 h or 7 d) as a ring with its percentage.
Item {
    id: ring
    property real percent: 0
    property string label: "5h"
    property var resetsAt: 0 // epoch seconds
    property int size: 30

    width: size
    height: size

    readonly property color tone: percent >= 90 ? Ui.bad : percent >= 70 ? Ui.warn : Ui.good
    property real shown: percent
    Behavior on shown { NumberAnimation { duration: 600; easing.type: Easing.OutCubic } }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: Ui.line; strokeWidth: 3; fillColor: "transparent"; capStyle: ShapePath.RoundCap
            PathAngleArc { centerX: ring.size / 2; centerY: ring.size / 2; radiusX: ring.size / 2 - 2; radiusY: radiusX; startAngle: 0; sweepAngle: 360 }
        }
        ShapePath {
            strokeColor: ring.tone; strokeWidth: 3; fillColor: "transparent"; capStyle: ShapePath.RoundCap
            PathAngleArc { centerX: ring.size / 2; centerY: ring.size / 2; radiusX: ring.size / 2 - 2; radiusY: radiusX; startAngle: -90; sweepAngle: 3.6 * Math.min(100, ring.shown) }
        }
    }
    Text {
        anchors.centerIn: parent
        text: Math.round(ring.percent)
        color: Ui.text
        font.pixelSize: ring.size * 0.32
        font.weight: Font.DemiBold
    }
    Text {
        anchors.top: parent.bottom
        anchors.topMargin: 1
        anchors.horizontalCenter: parent.horizontalCenter
        text: ring.label
        color: Ui.faint
        font.pixelSize: 9
    }

    HoverHandler { id: hover }
    Rectangle {
        visible: hover.hovered && ring.resetsAt > 0
        z: 10
        anchors.top: parent.bottom
        anchors.topMargin: 14
        anchors.right: parent.right
        width: tip.implicitWidth + 16
        height: tip.implicitHeight + 10
        radius: 6
        color: Ui.cardHover
        border.color: Ui.line
        Text {
            id: tip
            anchors.centerIn: parent
            color: Ui.text
            font.pixelSize: Ui.small
            text: qsTr("%1% used · resets %2").arg(Math.round(ring.percent))
                  .arg(new Date(ring.resetsAt * 1000).toLocaleString(Qt.locale(), "ddd HH:mm"))
        }
    }
}
