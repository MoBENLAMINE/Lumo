pragma Singleton
import QtQuick

QtObject {
    readonly property color island: "#09090D"
    // the island and Settings get a little depth toward the bottom
    readonly property color islandLow: "#111118"
    readonly property color card: "#15151C"
    readonly property color cardHover: "#1C1C25"
    readonly property color line: "#262630"
    readonly property color text: "#F3F3F7"
    readonly property color dim: "#9C9CAA"
    readonly property color faint: "#5E5E6C"
    // Claude's terracotta: Lumo's body, and every accent
    readonly property color accent: "#D97757"
    // the light Lumo's face is drawn with
    readonly property color glow: "#8FE3FF"
    readonly property color good: "#3DD68C"
    readonly property color bad: "#FF6B6B"
    readonly property color warn: "#F5C451"
    readonly property color info: "#7DD3FC"

    readonly property string mono: "monospace"
    readonly property int small: 11
    readonly property int body: 13
    readonly property int title: 15

    // Lumo's icons: 24×24 stroke paths for Icon.qml, so every button speaks the
    // same visual language whatever emoji font the desktop has.
    readonly property var icons: ({
        eye: "M2 12s3.5 -7 10 -7s10 7 10 7s-3.5 7 -10 7s-10 -7 -10 -7z M9 12a3 3 0 1 0 6 0a3 3 0 1 0 -6 0",
        eyeOff: "M3 3l18 18 M10.6 5.1a10 10 0 0 1 1.4 -0.1c6.5 0 10 7 10 7a17 17 0 0 1 -3.2 4.2 M6.6 6.6c-2.7 1.8 -4.6 5.4 -4.6 5.4s3.5 7 10 7a9.7 9.7 0 0 0 5.4 -1.6 M9.9 9.9a3 3 0 0 0 4.2 4.2",
        mic: "M12 4a3 3 0 0 0 -3 3v5a3 3 0 0 0 6 0v-5a3 3 0 0 0 -3 -3z M6 11a6 6 0 0 0 12 0 M12 17v3 M9 20h6",
        volume: "M4 9h4l5 -4v14l-5 -4h-4z M16.5 8.5a5 5 0 0 1 0 7 M19 6a8.5 8.5 0 0 1 0 12",
        volumeOff: "M4 9h4l5 -4v14l-5 -4h-4z M17 9.5l5 5 M22 9.5l-5 5",
        gear: "M5.5 12a6.5 6.5 0 1 0 13 0a6.5 6.5 0 1 0 -13 0 M9.5 12a2.5 2.5 0 1 0 5 0a2.5 2.5 0 1 0 -5 0 M12 3v2.5 M12 18.5v2.5 M3 12h2.5 M18.5 12h2.5 M5.6 5.6l1.8 1.8 M16.6 16.6l1.8 1.8 M5.6 18.4l1.8 -1.8 M16.6 7.4l1.8 -1.8",
        jump: "M7 17l10 -10 M9 7h8v8",
        close: "M6 6l12 12 M18 6l-12 12",
        chevronDown: "M6 9l6 6l6 -6",
        chevronUp: "M6 15l6 -6l6 6",
        chevronRight: "M9 6l6 6l-6 6",
        send: "M12 19v-14 M6 11l6 -6l6 6",
        stop: "M7 7h10v10h-10z",
        paperclip: "M20 11.5l-8.2 8.2a5 5 0 0 1 -7.1 -7.1l8.5 -8.5a3.3 3.3 0 0 1 4.7 4.7l-8.5 8.5a1.7 1.7 0 0 1 -2.4 -2.4l7.8 -7.8",
        pencil: "M4 20h4l11 -11l-4 -4l-11 11z M13.5 6.5l4 4",
        check: "M5 12.5l4.5 4.5l9.5 -10",
        play: "M8 5.5v13l10.5 -6.5z",
        pause: "M8 5v14 M16 5v14",
        skipBack: "M18 6v12l-9 -6z M6 6v12",
        skipForward: "M6 6v12l9 -6z M18 6v12",
        shuffle: "M16 4h4v4 M4 20l16 -16 M20 16v4h-4 M15 15l5 5 M4 4l5 5",
        repeat: "M17 2l3 3l-3 3 M4 11v-2a4 4 0 0 1 4 -4h12 M7 22l-3 -3l3 -3 M20 13v2a4 4 0 0 1 -4 4h-12",
        music: "M9 18v-12l11 -2v12 M3 18a3 3 0 1 0 6 0a3 3 0 1 0 -6 0 M14 16a3 3 0 1 0 6 0a3 3 0 1 0 -6 0",
        terminal: "M5 7l5 5l-5 5 M12 17h7",
        shield: "M12 3l8 3v6c0 4.5 -3.4 8.2 -8 9c-4.6 -0.8 -8 -4.5 -8 -9v-6z M9 12l2 2l4 -4",
        question: "M3 12a9 9 0 1 0 18 0a9 9 0 1 0 -18 0 M9.5 9.5a2.5 2.5 0 1 1 3.5 2.3c-0.7 0.3 -1 0.9 -1 1.7v0.5 M12 17.5v0.01",
        plan: "M9 6h11 M9 12h11 M9 18h11 M4.5 6h0.01 M4.5 12h0.01 M4.5 18h0.01",
        sliders: "M4 8h9 M17 8h3 M4 16h3 M11 16h9 M13 8a2 2 0 1 0 4 0a2 2 0 1 0 -4 0 M7 16a2 2 0 1 0 4 0a2 2 0 1 0 -4 0",
        chat: "M6 5h12a2 2 0 0 1 2 2v8a2 2 0 0 1 -2 2h-7l-4 3v-3h-1a2 2 0 0 1 -2 -2v-8a2 2 0 0 1 2 -2z",
        info: "M3 12a9 9 0 1 0 18 0a9 9 0 1 0 -18 0 M12 11v5 M12 8v0.01"
    })

    // Island geometry.
    readonly property int compactHeight: 38
    readonly property int openWidth: 580
    readonly property int maxOpenHeight: 560

    // Lumo's wardrobe. "auto" puts the headphones on while music plays.
    readonly property var outfits: [
        { id: "auto", label: qsTr("Auto") }, { id: "", label: qsTr("None") },
        { id: "crown", label: qsTr("Crown") }, { id: "tarbouche", label: qsTr("Tarbouche") },
        { id: "lion", label: qsTr("Lion") }, { id: "student", label: qsTr("Eng. student") },
        { id: "engineer", label: qsTr("Engineer") }, { id: "military", label: qsTr("Military") },
        { id: "headphones", label: qsTr("Headphones") }, { id: "sunglasses", label: qsTr("Sunglasses") }
    ]

    // The wardrobe choice as it stands today: a choice from an older Lumo (a
    // witch hat, a beanie…) is no longer in the wardrobe and acts as "auto".
    function outfitChoice(choice) {
        if (choice === "" || choice === "auto") return choice
        for (const o of outfits) if (o.id === choice) return choice
        return "auto"
    }

    // What Lumo actually wears for a wardrobe choice ("" for nothing).
    function outfitFor(choice, musicPlaying) {
        const c = outfitChoice(choice)
        if (c === "auto") return musicPlaying ? "headphones" : ""
        return c
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
