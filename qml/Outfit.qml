import QtQuick
import QtQuick.Shapes

// Lumo's clothes, drawn in code at any size `s`, over a body that fills an
// s × s square (its face screen spans 0.15–0.85 × 0.26–0.68, the eyes are
// centred at 0.355 / 0.645 × 0.455). Hats sit on the head and may stick out
// above it. Mascot.qml draws each outfit twice: once behind the body
// (`back: true`, for a mane or ears) and once in front of it.
Item {
    id: o
    property string name: ""
    property real s: 32
    property bool back: false
    width: s
    height: s

    // Parts drawn in front of the body.
    readonly property bool front: !back

    component Dot: Rectangle {
        property real cx; property real cy; property real r
        x: cx * o.s - r * o.s; y: cy * o.s - r * o.s
        width: 2 * r * o.s; height: width; radius: width / 2
        antialiasing: true
    }
    // A filled polygon in s-units: points: [[x, y], …].
    component Poly: Shape {
        id: poly
        property var points: []
        property color fill: "black"
        property color stroke: "transparent"
        property real strokeW: 0
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillColor: poly.fill
            strokeColor: poly.stroke
            strokeWidth: poly.strokeW * o.s
            joinStyle: ShapePath.RoundJoin
            PathPolyline { path: poly.points.map(p => Qt.point(p[0] * o.s, p[1] * o.s)) }
        }
    }

    // ------------------------------------------------------------ crown
    Item {
        anchors.fill: parent
        visible: o.name === "crown" && o.front
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: "#A8781A"
                strokeWidth: Math.max(1, o.s * 0.018)
                joinStyle: ShapePath.RoundJoin
                fillGradient: LinearGradient {
                    x1: 0; y1: -0.2 * o.s; x2: 0; y2: 0.16 * o.s
                    GradientStop { position: 0; color: "#FFE48A" }
                    GradientStop { position: 1; color: "#E9A923" }
                }
                startX: 0.21 * o.s; startY: 0.16 * o.s
                PathLine { x: 0.19 * o.s; y: -0.07 * o.s }
                PathLine { x: 0.275 * o.s; y: 0.035 * o.s }
                PathLine { x: 0.345 * o.s; y: -0.12 * o.s }
                PathLine { x: 0.42 * o.s; y: 0.03 * o.s }
                PathLine { x: 0.5 * o.s; y: -0.2 * o.s }
                PathLine { x: 0.58 * o.s; y: 0.03 * o.s }
                PathLine { x: 0.655 * o.s; y: -0.12 * o.s }
                PathLine { x: 0.725 * o.s; y: 0.035 * o.s }
                PathLine { x: 0.81 * o.s; y: -0.07 * o.s }
                PathLine { x: 0.79 * o.s; y: 0.16 * o.s }
                PathLine { x: 0.21 * o.s; y: 0.16 * o.s }
            }
        }
        // the band and its stones
        Rectangle { x: 0.205 * o.s; y: 0.095 * o.s; width: 0.59 * o.s; height: 0.065 * o.s; radius: height / 3; color: "#D99A1E" }
        Dot { cx: 0.5; cy: 0.127; r: 0.032; color: "#E5384F" }
        Dot { cx: 0.34; cy: 0.127; r: 0.022; color: "#3D8BFF" }
        Dot { cx: 0.66; cy: 0.127; r: 0.022; color: "#2EC27E" }
        // pearls on the points
        Dot { cx: 0.19; cy: -0.075; r: 0.025; color: "#FFF4D6" }
        Dot { cx: 0.345; cy: -0.125; r: 0.025; color: "#FFF4D6" }
        Dot { cx: 0.5; cy: -0.21; r: 0.03; color: "#FFF4D6" }
        Dot { cx: 0.655; cy: -0.125; r: 0.025; color: "#FFF4D6" }
        Dot { cx: 0.81; cy: -0.075; r: 0.025; color: "#FFF4D6" }
    }

    // ------------------------------------------------------------ Moroccan tarbouche
    // Red felt, a cone with a flat top, and a black silk tassel.
    Item {
        anchors.fill: parent
        visible: o.name === "tarbouche" && o.front
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: "#6E0E16"
                strokeWidth: Math.max(1, o.s * 0.016)
                joinStyle: ShapePath.RoundJoin
                fillGradient: LinearGradient {
                    x1: 0.26 * o.s; y1: 0; x2: 0.74 * o.s; y2: 0
                    GradientStop { position: 0; color: "#D8343C" }
                    GradientStop { position: 0.45; color: "#C1222B" }
                    GradientStop { position: 1; color: "#8C121B" }
                }
                startX: 0.25 * o.s; startY: 0.16 * o.s
                PathLine { x: 0.32 * o.s; y: -0.2 * o.s }
                PathQuad { x: 0.68 * o.s; y: -0.2 * o.s; controlX: 0.5 * o.s; controlY: -0.17 * o.s }
                PathLine { x: 0.75 * o.s; y: 0.16 * o.s }
                PathQuad { x: 0.25 * o.s; y: 0.16 * o.s; controlX: 0.5 * o.s; controlY: 0.21 * o.s }
            }
        }
        // the flat top
        Rectangle {
            x: 0.32 * o.s; y: -0.235 * o.s
            width: 0.36 * o.s; height: 0.07 * o.s
            radius: height / 2
            color: "#A3161F"
            border.color: "#6E0E16"; border.width: Math.max(1, o.s * 0.014)
        }
        // the stem of the tassel, in the middle of the top
        Dot { cx: 0.5; cy: -0.2; r: 0.022; color: "#111114" }
        // the silk cord, over the top and down the right side
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillColor: "transparent"
                strokeColor: "#111114"
                strokeWidth: Math.max(1, o.s * 0.028)
                capStyle: ShapePath.RoundCap
                startX: 0.5 * o.s; startY: -0.2 * o.s
                PathQuad { x: 0.71 * o.s; y: -0.1 * o.s; controlX: 0.67 * o.s; controlY: -0.23 * o.s }
                PathLine { x: 0.745 * o.s; y: 0.0 * o.s }
            }
        }
        // the tassel: a knot, then black silk strands hanging down the side
        Dot { cx: 0.745; cy: 0.0; r: 0.034; color: "#111114" }
        Poly {
            points: [[0.715, 0.01], [0.775, 0.01], [0.84, 0.27], [0.79, 0.29], [0.745, 0.3], [0.7, 0.29], [0.66, 0.27], [0.715, 0.01]]
            fill: "#141417"
        }
        // the sheen of the silk
        Repeater {
            model: 4
            Rectangle {
                required property int index
                x: (0.69 + index * 0.033) * o.s; y: 0.06 * o.s
                width: Math.max(1, o.s * 0.009); height: 0.21 * o.s
                rotation: (index - 1.5) * 7
                transformOrigin: Item.Top
                color: "#4A4A55"
                opacity: 0.8
            }
        }
    }

    // ------------------------------------------------------------ lion
    // A mane and round ears behind the body, a tuft in front.
    Item {
        anchors.fill: parent
        visible: o.name === "lion" && o.back
        Repeater {
            model: 14
            Dot {
                required property int index
                readonly property real a: index * 2 * Math.PI / 14
                cx: 0.5 + 0.44 * Math.cos(a)
                cy: 0.52 + 0.44 * Math.sin(a)
                r: 0.17
                color: index % 2 === 0 ? "#7A3E0E" : "#9C5216"
            }
        }
        Repeater {
            model: 14
            Dot {
                required property int index
                readonly property real a: (index + 0.5) * 2 * Math.PI / 14
                cx: 0.5 + 0.38 * Math.cos(a)
                cy: 0.52 + 0.38 * Math.sin(a)
                r: 0.14
                color: "#C2711C"
            }
        }
        // ears
        Dot { cx: 0.22; cy: 0.06; r: 0.11; color: "#7A3E0E" }
        Dot { cx: 0.78; cy: 0.06; r: 0.11; color: "#7A3E0E" }
        Dot { cx: 0.22; cy: 0.06; r: 0.055; color: "#F3C27A" }
        Dot { cx: 0.78; cy: 0.06; r: 0.055; color: "#F3C27A" }
    }
    Item {
        anchors.fill: parent
        visible: o.name === "lion" && o.front
        Poly {
            points: [[0.36, 0.1], [0.42, -0.02], [0.47, 0.07], [0.53, -0.05], [0.58, 0.07], [0.64, -0.01], [0.66, 0.1], [0.36, 0.1]]
            fill: "#9C5216"
        }
    }

    // ------------------------------------------------------------ engineering student
    // Round glasses and a pencil tucked behind the ear.
    Item {
        anchors.fill: parent
        visible: o.name === "student" && o.front
        // the pencil, behind the right side
        Item {
            x: 0.79 * o.s; y: -0.06 * o.s
            width: 0.075 * o.s; height: 0.42 * o.s
            rotation: 28
            transformOrigin: Item.Center
            Rectangle { y: 0; width: parent.width; height: 0.06 * o.s; radius: 0.02 * o.s; color: "#FF8FB1" }
            Rectangle { y: 0.055 * o.s; width: parent.width; height: 0.035 * o.s; color: "#B9BEC8" }
            Rectangle {
                y: 0.09 * o.s; width: parent.width; height: 0.24 * o.s
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: "#FFD54A" }
                    GradientStop { position: 1; color: "#E8A91A" }
                }
            }
            Poly {
                anchors.fill: undefined
                width: o.s; height: o.s
                points: [[0, 0.33], [0.075, 0.33], [0.0375, 0.42], [0, 0.33]]
                fill: "#F0CDA0"
            }
            Poly {
                anchors.fill: undefined
                width: o.s; height: o.s
                points: [[0.025, 0.395], [0.05, 0.395], [0.0375, 0.42], [0.025, 0.395]]
                fill: "#2A2A33"
            }
        }
        // round glasses over the eyes
        Repeater {
            model: 2
            Rectangle {
                required property int index
                readonly property real cx: index === 0 ? 0.355 : 0.645
                x: (cx - 0.13) * o.s; y: (0.455 - 0.13) * o.s
                width: 0.26 * o.s; height: width
                radius: width / 2
                color: Qt.rgba(1, 1, 1, 0.07)
                border.color: "#E0A85A"
                border.width: Math.max(1.2, o.s * 0.03)
                antialiasing: true
            }
        }
        Rectangle { x: 0.475 * o.s; y: 0.43 * o.s; width: 0.05 * o.s; height: Math.max(1, 0.028 * o.s); radius: height / 2; color: "#E0A85A" }
    }

    // ------------------------------------------------------------ engineer
    // A yellow hard hat with a ridge and a brim.
    Item {
        anchors.fill: parent
        visible: o.name === "engineer" && o.front
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: "#B98A00"
                strokeWidth: Math.max(1, o.s * 0.016)
                fillGradient: LinearGradient {
                    x1: 0.15 * o.s; y1: 0; x2: 0.85 * o.s; y2: 0
                    GradientStop { position: 0; color: "#FFD84D" }
                    GradientStop { position: 0.6; color: "#FFC21A" }
                    GradientStop { position: 1; color: "#E0A300" }
                }
                startX: 0.14 * o.s; startY: 0.15 * o.s
                PathCubic { x: 0.86 * o.s; y: 0.15 * o.s; control1X: 0.14 * o.s; control1Y: -0.24 * o.s; control2X: 0.86 * o.s; control2Y: -0.24 * o.s }
                PathLine { x: 0.14 * o.s; y: 0.15 * o.s }
            }
        }
        // the ridge
        Rectangle { x: 0.455 * o.s; y: -0.14 * o.s; width: 0.09 * o.s; height: 0.29 * o.s; radius: width / 2; color: "#FFE27A" }
        // the brim
        Rectangle {
            x: 0.05 * o.s; y: 0.12 * o.s
            width: 0.9 * o.s; height: 0.075 * o.s
            radius: height / 2
            color: "#F2B300"
            border.color: "#B98A00"; border.width: Math.max(1, o.s * 0.014)
        }
        // shine
        Rectangle { x: 0.24 * o.s; y: -0.05 * o.s; width: 0.12 * o.s; height: 0.05 * o.s; radius: height / 2; rotation: -30; color: "white"; opacity: 0.45 }
    }

    // ------------------------------------------------------------ military
    // A Moroccan officer in dress uniform: the navy peaked cap with its gold
    // emblem and braid; the navy tunic with a gold collar and buttons, the red
    // and black braided cord, red shoulder boards and white gloves.
    Item {
        anchors.fill: parent
        visible: o.name === "military" && o.front

        // ---- the tunic, cut to the shape of the body below the face
        Item {
            x: 0.06 * o.s; y: 0.69 * o.s
            width: 0.88 * o.s; height: 0.17 * o.s
            clip: true
            Rectangle {
                y: -0.65 * o.s
                width: 0.88 * o.s; height: 0.82 * o.s
                radius: width * 0.26
                gradient: Gradient {
                    GradientStop { position: 0.75; color: "#232A40" }
                    GradientStop { position: 1; color: "#141826" }
                }
            }
            // gold collar
            Rectangle { width: parent.width; height: Math.max(1, 0.02 * o.s); color: "#E2B84E" }
        }
        // gold buttons
        Dot { cx: 0.5; cy: 0.745; r: 0.019; color: "#F0C95A" }
        Dot { cx: 0.5; cy: 0.81; r: 0.019; color: "#F0C95A" }

        // the braided cord (aiguillette): from the right shoulder across the chest
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillColor: "transparent"; strokeColor: "#151519"
                strokeWidth: Math.max(1.2, 0.042 * o.s); capStyle: ShapePath.RoundCap
                startX: 0.88 * o.s; startY: 0.56 * o.s
                PathCubic { x: 0.4 * o.s; y: 0.72 * o.s; control1X: 0.86 * o.s; control1Y: 0.84 * o.s; control2X: 0.48 * o.s; control2Y: 0.84 * o.s }
            }
            ShapePath {
                fillColor: "transparent"; strokeColor: "#C8202C"
                strokeWidth: Math.max(1.2, 0.042 * o.s)
                strokeStyle: ShapePath.DashLine; dashPattern: [1.1, 1.1]
                startX: 0.88 * o.s; startY: 0.56 * o.s
                PathCubic { x: 0.4 * o.s; y: 0.72 * o.s; control1X: 0.86 * o.s; control1Y: 0.84 * o.s; control2X: 0.48 * o.s; control2Y: 0.84 * o.s }
            }
        }
        // the gold tip at the end of the cord
        Rectangle { x: 0.385 * o.s; y: 0.72 * o.s; width: Math.max(1, 0.03 * o.s); height: 0.08 * o.s; radius: width / 2; color: "#E2B84E" }

        // red shoulder boards edged in gold, and white gloves
        Repeater {
            model: 2
            Item {
                required property int index
                anchors.fill: parent
                Rectangle {
                    x: (index === 0 ? -0.01 : 0.85) * o.s; y: 0.48 * o.s
                    width: 0.16 * o.s; height: 0.055 * o.s
                    radius: height / 2
                    color: "#C8202C"
                    border.color: "#E2B84E"; border.width: Math.max(1, 0.012 * o.s)
                }
                Rectangle {
                    x: (index === 0 ? -0.005 : 0.935) * o.s; y: 0.565 * o.s
                    width: 0.07 * o.s; height: 0.08 * o.s
                    radius: width * 0.4
                    color: "#F4F4F6"
                    border.color: "#C9C9D0"; border.width: Math.max(1, 0.008 * o.s)
                }
            }
        }

        // ---- the cap: navy crown, wider than the head
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: "#07080C"
                strokeWidth: Math.max(1, o.s * 0.016)
                fillGradient: LinearGradient {
                    x1: 0; y1: -0.14 * o.s; x2: 0; y2: 0.12 * o.s
                    GradientStop { position: 0; color: "#2A3049" }
                    GradientStop { position: 1; color: "#141826" }
                }
                startX: 0.2 * o.s; startY: 0.1 * o.s
                PathQuad { x: 0.06 * o.s; y: -0.07 * o.s; controlX: 0.03 * o.s; controlY: 0.04 * o.s }
                PathQuad { x: 0.94 * o.s; y: -0.07 * o.s; controlX: 0.5 * o.s; controlY: -0.22 * o.s }
                PathQuad { x: 0.8 * o.s; y: 0.1 * o.s; controlX: 0.97 * o.s; controlY: 0.04 * o.s }
                PathLine { x: 0.2 * o.s; y: 0.1 * o.s }
            }
        }
        // the band
        Rectangle { x: 0.2 * o.s; y: 0.07 * o.s; width: 0.6 * o.s; height: 0.09 * o.s; radius: 0.02 * o.s; color: "#0D0F16" }
        // the glossy black visor
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillGradient: LinearGradient {
                    x1: 0; y1: 0.15 * o.s; x2: 0; y2: 0.27 * o.s
                    GradientStop { position: 0; color: "#2C2C33" }
                    GradientStop { position: 1; color: "#08080A" }
                }
                strokeColor: "transparent"
                startX: 0.19 * o.s; startY: 0.15 * o.s
                PathLine { x: 0.81 * o.s; y: 0.15 * o.s }
                PathQuad { x: 0.19 * o.s; y: 0.15 * o.s; controlX: 0.5 * o.s; controlY: 0.3 * o.s }
            }
        }
        // the gold braid over the visor
        Rectangle { x: 0.21 * o.s; y: 0.135 * o.s; width: 0.58 * o.s; height: Math.max(1, 0.026 * o.s); radius: height / 2; color: "#E2B84E" }
        // the emblem: a gold medallion, a red disc, a green star
        Dot { cx: 0.5; cy: 0.0; r: 0.075; color: "#E8C15A" }
        Dot { cx: 0.5; cy: 0.0; r: 0.05; color: "#C1272D" }
        Poly {
            readonly property real cx: 0.5
            readonly property real cy: 0.004
            readonly property real ro: 0.038
            readonly property real ri: 0.016
            points: {
                const p = []
                for (let i = 0; i <= 10; i++) {
                    const a = -Math.PI / 2 + i * Math.PI / 5, r = i % 2 === 0 ? ro : ri
                    p.push([cx + r * Math.cos(a), cy + r * Math.sin(a)])
                }
                return p
            }
            fill: "#1E8A44"
        }
    }

    // ------------------------------------------------------------ headphones
    Item {
        anchors.fill: parent
        visible: o.name === "headphones" && o.front
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillColor: "transparent"; strokeColor: "#24242C"; strokeWidth: 0.085 * o.s; capStyle: ShapePath.RoundCap
                startX: 0.06 * o.s; startY: 0.44 * o.s
                PathArc { x: 0.94 * o.s; y: 0.44 * o.s; radiusX: 0.45 * o.s; radiusY: 0.5 * o.s }
            }
            ShapePath {
                fillColor: "transparent"; strokeColor: "#4A4A58"; strokeWidth: 0.025 * o.s; capStyle: ShapePath.RoundCap
                startX: 0.14 * o.s; startY: 0.2 * o.s
                PathArc { x: 0.86 * o.s; y: 0.2 * o.s; radiusX: 0.42 * o.s; radiusY: 0.42 * o.s }
            }
        }
        Repeater {
            model: 2
            Rectangle {
                required property int index
                x: (index === 0 ? -0.05 : 0.87) * o.s; y: 0.33 * o.s
                width: 0.18 * o.s; height: 0.3 * o.s
                radius: 0.08 * o.s
                gradient: Gradient {
                    GradientStop { position: 0; color: "#34343F" }
                    GradientStop { position: 1; color: "#18181E" }
                }
                border.color: "#0E0E12"; border.width: Math.max(1, 0.02 * o.s)
                // the light ring on each cup
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width * 0.5; height: parent.height * 0.5
                    radius: width / 2
                    color: "transparent"
                    border.color: "#8FE3FF"; border.width: Math.max(1, 0.022 * o.s)
                    opacity: 0.9
                }
            }
        }
    }

    // ------------------------------------------------------------ sunglasses
    // Gold aviators with sunset lenses, over the face screen.
    Item {
        anchors.fill: parent
        visible: o.name === "sunglasses" && o.front
        Repeater {
            model: 2
            Rectangle {
                required property int index
                readonly property real cx: index === 0 ? 0.345 : 0.655
                x: (cx - 0.15) * o.s; y: 0.35 * o.s
                width: 0.3 * o.s; height: 0.2 * o.s
                radius: 0.09 * o.s
                antialiasing: true
                gradient: Gradient {
                    GradientStop { position: 0; color: "#1E1730" }
                    GradientStop { position: 0.6; color: "#5A2A4F" }
                    GradientStop { position: 1; color: "#E0775A" }
                }
                border.color: "#E5BE5C"
                border.width: Math.max(1, 0.022 * o.s)
                // glint
                Rectangle {
                    x: parent.width * 0.18; y: parent.height * 0.22
                    width: parent.width * 0.32; height: Math.max(1, parent.height * 0.12)
                    radius: height / 2; rotation: -22
                    color: "white"; opacity: 0.55
                }
            }
        }
        // the bridge
        Rectangle { x: 0.47 * o.s; y: 0.38 * o.s; width: 0.06 * o.s; height: Math.max(1, 0.024 * o.s); radius: height / 2; color: "#E5BE5C" }
    }
}
