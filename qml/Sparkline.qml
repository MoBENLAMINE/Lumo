import QtQuick
import QtQuick.Shapes

// A small line chart of recent values, filled softly underneath.
Shape {
    id: spark
    property var values: []
    property real max: 0            // 0: the largest value shown
    property color tint: Ui.accent
    preferredRendererType: Shape.CurveRenderer
    readonly property real ceiling: max > 0 ? max : Math.max(1e-6, Math.max.apply(null, values.length ? values : [1]))
    readonly property var points: {
        const n = values.length
        const out = []
        if (n < 2) return out
        for (let i = 0; i < n; ++i)
            out.push(Qt.point(i / (n - 1) * width, height - Math.min(1, values[i] / ceiling) * (height - 2) - 1))
        return out
    }
    ShapePath {
        strokeColor: "transparent"
        fillGradient: LinearGradient {
            x1: 0; y1: 0; x2: 0; y2: spark.height
            GradientStop { position: 0; color: Qt.rgba(spark.tint.r, spark.tint.g, spark.tint.b, 0.28) }
            GradientStop { position: 1; color: Qt.rgba(spark.tint.r, spark.tint.g, spark.tint.b, 0) }
        }
        PathPolyline { path: spark.points.length ? [Qt.point(0, spark.height)].concat(spark.points, [Qt.point(spark.width, spark.height)]) : [] }
    }
    ShapePath {
        strokeColor: spark.tint
        strokeWidth: 1.6
        fillColor: "transparent"
        joinStyle: ShapePath.RoundJoin
        PathPolyline { path: spark.points }
    }
}
