pragma Singleton
import QtQuick

QtObject {
    readonly property color island: "#0A0A0C"
    readonly property color card: "#17171B"
    readonly property color cardHover: "#1F1F25"
    readonly property color line: "#2A2A31"
    readonly property color text: "#F2F2F5"
    readonly property color dim: "#9A9AA5"
    readonly property color faint: "#5E5E68"
    readonly property color accent: "#E8875F"
    readonly property color good: "#3DD68C"
    readonly property color bad: "#FF6B6B"
    readonly property color warn: "#F5C451"
    readonly property color info: "#7DD3FC"

    readonly property string mono: "monospace"
    readonly property int small: 11
    readonly property int body: 13
    readonly property int title: 15

    // Island geometry.
    readonly property int compactHeight: 38
    readonly property int openWidth: 580
    readonly property int maxOpenHeight: 560

    // What Lumo wears on its own at this time of year ("" for nothing).
    function seasonal(d, musicPlaying) {
        const m = d.getMonth() + 1, day = d.getDate()
        if (m === 10 && day >= 20) return "witch"
        if (m === 12 && day >= 1 && day <= 26) return "santa"
        if ((m === 12 && day === 31) || (m === 1 && day <= 2)) return "party"
        if (m === 2 && day === 14) return "bow"
        return musicPlaying ? "headphones" : ""
    }

    function statusColor(status) {
        switch (status) {
        case "working": return info
        case "waiting": return warn
        case "done": return good
        case "error": return bad
        default: return dim
        }
    }

    function ago(ms) {
        const s = Math.max(0, Math.round((Date.now() - ms) / 1000))
        if (s < 5) return qsTr("now")
        if (s < 60) return qsTr("%1 s").arg(s)
        if (s < 3600) return qsTr("%1 min").arg(Math.round(s / 60))
        return qsTr("%1 h").arg(Math.round(s / 3600))
    }
}
