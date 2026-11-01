import QtQuick
import QtQuick.Shapes

// Lumo's clothes, drawn in code at any size `s`, over a body that fills an
// s × s square. Hats sit on the head and may stick out above it.
Item {
    id: o
    property string name: ""
    property real s: 32
    width: s
    height: s

    component Cone: Shape {
        id: cone
        property real ax; property real ay   // apex
        property real lx; property real rx; property real by   // base
        property color fill
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillColor: cone.fill
            strokeColor: "transparent"
            startX: cone.lx * o.s; startY: cone.by * o.s
            PathQuad { x: cone.ax * o.s; y: cone.ay * o.s; controlX: (cone.lx + cone.ax) / 2 * o.s - o.s * 0.02; controlY: (cone.by + cone.ay) / 2 * o.s }
            PathQuad { x: cone.rx * o.s; y: cone.by * o.s; controlX: (cone.rx + cone.ax) / 2 * o.s + o.s * 0.03; controlY: (cone.by + cone.ay) / 2 * o.s }
            PathLine { x: cone.lx * o.s; y: cone.by * o.s }
        }
    }
    component Dot: Rectangle {
        property real cx; property real cy; property real r
        x: cx * o.s - r * o.s; y: cy * o.s - r * o.s
        width: 2 * r * o.s; height: width; radius: width / 2
        antialiasing: true
    }

    // ---- witch hat
    Item {
        anchors.fill: parent
        visible: o.name === "witch"
        Cone { ax: 0.62; ay: -0.46; lx: 0.24; rx: 0.78; by: 0.14; fill: "#3B2A5C" }
        Rectangle { x: 0.04 * o.s; y: 0.09 * o.s; width: 0.92 * o.s; height: 0.1 * o.s; radius: height / 2; color: "#2B1F45" }
        Rectangle { x: 0.26 * o.s; y: 0.05 * o.s; width: 0.5 * o.s; height: 0.06 * o.s; color: "#F5A65B" }
        Rectangle { x: 0.46 * o.s; y: 0.045 * o.s; width: 0.09 * o.s; height: 0.07 * o.s; color: "#FFD866"; border.color: "#3B2A5C"; border.width: Math.max(1, o.s * 0.015) }
    }

    // ---- Santa hat
    Item {
        anchors.fill: parent
        visible: o.name === "santa"
        Cone { ax: 0.86; ay: -0.26; lx: 0.18; rx: 0.82; by: 0.16; fill: "#D93A3A" }
        Rectangle { x: 0.12 * o.s; y: 0.08 * o.s; width: 0.76 * o.s; height: 0.14 * o.s; radius: height / 2; color: "#F7F7F7" }
        Dot { cx: 0.88; cy: -0.26; r: 0.09; color: "#F7F7F7" }
    }

    // ---- party hat
    Item {
        anchors.fill: parent
        visible: o.name === "party"
        Cone { ax: 0.5; ay: -0.42; lx: 0.32; rx: 0.68; by: 0.14; fill: "#6EA8FE" }
        Dot { cx: 0.45; cy: 0.02; r: 0.035; color: "#FFD866" }
        Dot { cx: 0.55; cy: -0.12; r: 0.03; color: "#FF8FB1" }
        Dot { cx: 0.5; cy: -0.24; r: 0.025; color: "#FFD866" }
        Dot { cx: 0.5; cy: -0.44; r: 0.07; color: "#FF8FB1" }
    }

    // ---- crown
    Shape {
        anchors.fill: parent
        visible: o.name === "crown"
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillColor: "#FFC93C"; strokeColor: "#C9962A"; strokeWidth: Math.max(1, o.s * 0.02); joinStyle: ShapePath.RoundJoin
            startX: 0.22 * o.s; startY: 0.15 * o.s
            PathLine { x: 0.2 * o.s; y: -0.08 * o.s }
            PathLine { x: 0.36 * o.s; y: 0.04 * o.s }
            PathLine { x: 0.5 * o.s; y: -0.16 * o.s }
            PathLine { x: 0.64 * o.s; y: 0.04 * o.s }
            PathLine { x: 0.8 * o.s; y: -0.08 * o.s }
            PathLine { x: 0.78 * o.s; y: 0.15 * o.s }
            PathLine { x: 0.22 * o.s; y: 0.15 * o.s }
        }
    }
    Dot { visible: o.name === "crown"; cx: 0.5; cy: 0.07; r: 0.035; color: "#FF5C7A" }
    Dot { visible: o.name === "crown"; cx: 0.34; cy: 0.09; r: 0.025; color: "#6EA8FE" }
    Dot { visible: o.name === "crown"; cx: 0.66; cy: 0.09; r: 0.025; color: "#3DD68C" }

    // ---- sunglasses (over the eyes)
    Item {
        anchors.fill: parent
        visible: o.name === "sunglasses"
        Rectangle { x: 0.17 * o.s; y: 0.31 * o.s; width: 0.3 * o.s; height: 0.2 * o.s; radius: 0.07 * o.s; color: "#121216" }
        Rectangle { x: 0.53 * o.s; y: 0.31 * o.s; width: 0.3 * o.s; height: 0.2 * o.s; radius: 0.07 * o.s; color: "#121216" }
        Rectangle { x: 0.46 * o.s; y: 0.35 * o.s; width: 0.08 * o.s; height: Math.max(1, 0.03 * o.s); color: "#121216" }
        Rectangle { x: 0.22 * o.s; y: 0.34 * o.s; width: 0.09 * o.s; height: Math.max(1, 0.03 * o.s); radius: height / 2; color: "#FFFFFF"; opacity: 0.55; rotation: -20 }
        Rectangle { x: 0.58 * o.s; y: 0.34 * o.s; width: 0.09 * o.s; height: Math.max(1, 0.03 * o.s); radius: height / 2; color: "#FFFFFF"; opacity: 0.55; rotation: -20 }
    }

    // ---- bow
    Item {
        anchors.fill: parent
        visible: o.name === "bow"
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillColor: "#FF6FA5"; strokeColor: "transparent"
                startX: 0.74 * o.s; startY: 0.04 * o.s
                PathLine { x: 0.56 * o.s; y: -0.08 * o.s }
                PathLine { x: 0.6 * o.s; y: 0.16 * o.s }
                PathLine { x: 0.74 * o.s; y: 0.04 * o.s }
                PathLine { x: 0.92 * o.s; y: -0.08 * o.s }
                PathLine { x: 0.9 * o.s; y: 0.16 * o.s }
                PathLine { x: 0.74 * o.s; y: 0.04 * o.s }
            }
        }
        Dot { cx: 0.74; cy: 0.04; r: 0.05; color: "#E04E8A" }
    }

    // ---- headphones
    Item {
        anchors.fill: parent
        visible: o.name === "headphones"
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillColor: "transparent"; strokeColor: "#2A2A33"; strokeWidth: 0.08 * o.s; capStyle: ShapePath.RoundCap
                startX: 0.06 * o.s; startY: 0.46 * o.s
                PathArc { x: 0.94 * o.s; y: 0.46 * o.s; radiusX: 0.45 * o.s; radiusY: 0.5 * o.s }
            }
        }
        Rectangle { x: -0.04 * o.s; y: 0.36 * o.s; width: 0.17 * o.s; height: 0.28 * o.s; radius: 0.07 * o.s; color: "#E8875F"; border.color: "#2A2A33"; border.width: Math.max(1, 0.03 * o.s) }
        Rectangle { x: 0.87 * o.s; y: 0.36 * o.s; width: 0.17 * o.s; height: 0.28 * o.s; radius: 0.07 * o.s; color: "#E8875F"; border.color: "#2A2A33"; border.width: Math.max(1, 0.03 * o.s) }
    }

    // ---- graduation cap
    Item {
        anchors.fill: parent
        visible: o.name === "grad"
        Rectangle { x: 0.3 * o.s; y: 0.02 * o.s; width: 0.4 * o.s; height: 0.14 * o.s; radius: 0.03 * o.s; color: "#1D1D24" }
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillColor: "#26262E"; strokeColor: "transparent"
                startX: 0.5 * o.s; startY: -0.14 * o.s
                PathLine { x: 0.98 * o.s; y: -0.01 * o.s }
                PathLine { x: 0.5 * o.s; y: 0.11 * o.s }
                PathLine { x: 0.02 * o.s; y: -0.01 * o.s }
                PathLine { x: 0.5 * o.s; y: -0.14 * o.s }
            }
        }
        Rectangle { x: 0.84 * o.s; y: -0.03 * o.s; width: Math.max(1, 0.025 * o.s); height: 0.24 * o.s; color: "#FFC93C" }
        Dot { cx: 0.855; cy: 0.23; r: 0.04; color: "#FFC93C" }
    }

    // ---- flower
    Item {
        anchors.fill: parent
        visible: o.name === "flower"
        Repeater {
            model: 5
            Dot {
                required property int index
                cx: 0.27 + 0.09 * Math.cos(index * 2 * Math.PI / 5)
                cy: 0.07 + 0.09 * Math.sin(index * 2 * Math.PI / 5)
                r: 0.065
                color: "#FFFFFF"
            }
        }
        Dot { cx: 0.27; cy: 0.07; r: 0.055; color: "#FFD866" }
    }

    // ---- beanie
    Item {
        anchors.fill: parent
        visible: o.name === "beanie"
        Rectangle { x: 0.1 * o.s; y: -0.14 * o.s; width: 0.8 * o.s; height: 0.36 * o.s; radius: 0.32 * o.s; color: "#3DD68C" }
        Rectangle { x: 0.08 * o.s; y: 0.11 * o.s; width: 0.84 * o.s; height: 0.11 * o.s; radius: 0.04 * o.s; color: "#2BB673" }
        Dot { cx: 0.5; cy: -0.15; r: 0.08; color: "#E8FFF3" }
    }
}
