import QtQuick
import QtQuick.Shapes

// A stroke icon drawn from a 24×24 SVG path (see Ui.icons), crisp at any size.
Shape {
    id: icon
    property string path
    property color tint: Ui.dim
    property real size: 18
    property real stroke: 1.8
    implicitWidth: size
    implicitHeight: size
    preferredRendererType: Shape.CurveRenderer
    ShapePath {
        strokeColor: icon.tint
        strokeWidth: icon.stroke
        fillColor: "transparent"
        capStyle: ShapePath.RoundCap
        joinStyle: ShapePath.RoundJoin
        scale: Qt.size(icon.size / 24, icon.size / 24)
        PathSvg { path: icon.path }
    }
}
