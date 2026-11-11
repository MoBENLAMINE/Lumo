pragma Singleton
import QtQuick

QtObject {
    // The theme (Prefs): dark, light, glass (the desktop shows through,
    // blurred), custom (your background) or starfall (a night sky).
    property string theme: "dark"
    property color customBackground: "#101820"
    property real surfaceOpacity: 1.0
    readonly property bool light: theme === "light" || (theme === "custom" && customBackground.hslLightness > 0.6)
    function alpha(c, a) { const k = Qt.lighter(c, 1.0); return Qt.rgba(k.r, k.g, k.b, a) }
    readonly property real _a: theme === "glass" ? Math.min(surfaceOpacity, 0.6) : surfaceOpacity

    readonly property color island: theme === "light" ? alpha("#F3F3F6", _a)
                                  : theme === "custom" ? alpha(customBackground, _a)
                                  : theme === "starfall" ? alpha("#0B0A1E", _a)
                                  : theme === "glass" ? alpha("#0A0A10", _a)
                                  : alpha("#09090D", _a)
    // the island and Settings get a little depth toward the bottom
    readonly property color islandLow: theme === "light" ? alpha("#E9E9EF", _a)
                                     : theme === "custom" ? alpha(Qt.darker(customBackground, 1.15), _a)
                                     : theme === "starfall" ? alpha("#16112E", _a)
                                     : theme === "glass" ? alpha("#12121A", _a)
                                     : alpha("#111118", _a)
    readonly property color card: light ? "#FFFFFF"
                                : theme === "glass" ? Qt.rgba(1, 1, 1, 0.07)
                                : theme === "custom" ? Qt.lighter(customBackground, 1.35)
                                : theme === "starfall" ? "#17142F" : "#15151C"
    readonly property color cardHover: light ? "#F0F0F5"
                                     : theme === "glass" ? Qt.rgba(1, 1, 1, 0.12)
                                     : theme === "custom" ? Qt.lighter(customBackground, 1.6)
                                     : theme === "starfall" ? "#211C40" : "#1C1C25"
    readonly property color line: light ? "#E0E0E7" : theme === "glass" ? Qt.rgba(1, 1, 1, 0.12)
                                 : theme === "starfall" ? "#2A2550" : "#262630"
    readonly property color text: light ? "#15151B" : "#F3F3F7"
    readonly property color dim: light ? "#5C5C6A" : "#9C9CAA"
    readonly property color faint: light ? "#9494A2" : "#5E5E6C"
    readonly property color track: light ? "#D6D6DE" : "#2E2E36"
    // Claude's terracotta by default: Lumo's body, and every accent. The island
    // sets it from Prefs (KDE's accent, the wallpaper's, the album's, your own).
    property color accent: "#D97757"
    // Reduce motion: loops calm down, flourishes are skipped (Prefs).
    property bool reduceMotion: false
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
        search: "M4 11a7 7 0 1 0 14 0a7 7 0 1 0 -14 0 M21 21l-5 -5",
        shuffle: "M16 4h4v4 M4 20l16 -16 M20 16v4h-4 M15 15l5 5 M4 4l5 5",
        repeat: "M17 2l3 3l-3 3 M4 11v-2a4 4 0 0 1 4 -4h12 M7 22l-3 -3l3 -3 M20 13v2a4 4 0 0 1 -4 4h-12",
        music: "M9 18v-12l11 -2v12 M3 18a3 3 0 1 0 6 0a3 3 0 1 0 -6 0 M14 16a3 3 0 1 0 6 0a3 3 0 1 0 -6 0",
        terminal: "M5 7l5 5l-5 5 M12 17h7",
        shield: "M12 3l8 3v6c0 4.5 -3.4 8.2 -8 9c-4.6 -0.8 -8 -4.5 -8 -9v-6z M9 12l2 2l4 -4",
        question: "M3 12a9 9 0 1 0 18 0a9 9 0 1 0 -18 0 M9.5 9.5a2.5 2.5 0 1 1 3.5 2.3c-0.7 0.3 -1 0.9 -1 1.7v0.5 M12 17.5v0.01",
        plan: "M9 6h11 M9 12h11 M9 18h11 M4.5 6h0.01 M4.5 12h0.01 M4.5 18h0.01",
        sliders: "M4 8h9 M17 8h3 M4 16h3 M11 16h9 M13 8a2 2 0 1 0 4 0a2 2 0 1 0 -4 0 M7 16a2 2 0 1 0 4 0a2 2 0 1 0 -4 0",
        chat: "M6 5h12a2 2 0 0 1 2 2v8a2 2 0 0 1 -2 2h-7l-4 3v-3h-1a2 2 0 0 1 -2 -2v-8a2 2 0 0 1 2 -2z",
        info: "M3 12a9 9 0 1 0 18 0a9 9 0 1 0 -18 0 M12 11v5 M12 8v0.01",
        // the island's pages
        agents: "M7 8h10a3 3 0 0 1 3 3v5a3 3 0 0 1 -3 3h-10a3 3 0 0 1 -3 -3v-5a3 3 0 0 1 3 -3z M9 13v1 M15 13v1 M12 5v3 M8 19v2 M16 19v2",
        today: "M4 7a2 2 0 0 1 2 -2h12a2 2 0 0 1 2 2v12a2 2 0 0 1 -2 2h-12a2 2 0 0 1 -2 -2z M4 10h16 M8 3v4 M16 3v4 M8 14h3v3h-3z",
        system: "M4 19h16 M6 16v-5 M10 16v-9 M14 16v-6 M18 16v-11",
        tools: "M14.5 5.5a4 4 0 0 0 -5 5l-5.5 5.5a1.8 1.8 0 0 0 2.5 2.5l5.5 -5.5a4 4 0 0 0 5 -5l-2.5 2.5l-2.5 -0.5l-0.5 -2.5z",
        // controls
        wifi: "M2.5 9a14 14 0 0 1 19 0 M5.5 12.5a9.5 9.5 0 0 1 13 0 M8.5 16a5 5 0 0 1 7 0 M12 19.5v0.01",
        wifiOff: "M3 3l18 18 M8.5 16a5 5 0 0 1 7 0 M5.5 12.5a9.5 9.5 0 0 1 4 -2.3 M2.5 9a14 14 0 0 1 4.3 -2.6 M14 10.3a9.5 9.5 0 0 1 4.5 2.2 M12 19.5v0.01",
        bluetooth: "M7 7l10 10l-5 4v-18l5 4l-10 10",
        micOff: "M3 3l18 18 M9 9v3a3 3 0 0 0 5.1 2.1 M15 10.3v-3.3a3 3 0 0 0 -5.7 -1.3 M6 11a6 6 0 0 0 9.9 4.6 M18 11a6 6 0 0 1 -0.3 1.9 M12 17v3 M9 20h6",
        sun: "M8 12a4 4 0 1 0 8 0a4 4 0 1 0 -8 0 M12 2v2 M12 20v2 M4.9 4.9l1.4 1.4 M17.7 17.7l1.4 1.4 M2 12h2 M20 12h2 M4.9 19.1l1.4 -1.4 M17.7 6.3l1.4 -1.4",
        moon: "M20 14.5a8 8 0 1 1 -10.5 -10.5a6.5 6.5 0 0 0 10.5 10.5z",
        battery: "M3 8a2 2 0 0 1 2 -2h12a2 2 0 0 1 2 2v8a2 2 0 0 1 -2 2h-12a2 2 0 0 1 -2 -2z M22 10.5v3",
        bolt: "M13 3l-8 11h6l-1 7l8 -11h-6z",
        leaf: "M5 19c0 -8 5 -14 15 -14c0 10 -6 15 -14 15 M5 19c3 -4 6 -7 10 -9",
        gauge: "M4 17a8 8 0 1 1 16 0 M12 17l4 -5 M12 17v0.01",
        coffee: "M5 9h11v5a5 5 0 0 1 -5 5h-1a5 5 0 0 1 -5 -5z M16 10h1.5a2.5 2.5 0 0 1 0 5h-1.5 M8 3v3 M12 3v3",
        bell: "M6 16v-5a6 6 0 0 1 12 0v5l2 2h-16z M10 20.5a2 2 0 0 0 4 0",
        bellOff: "M3 3l18 18 M8.3 5.6a6 6 0 0 1 9.7 5.4v5l2 2h-14l2 -2v-5 M10 20.5a2 2 0 0 0 4 0",
        output: "M4 9h4l5 -4v14l-5 -4h-4z M17 9.5a3.5 3.5 0 0 1 0 5",
        // system
        cpu: "M7 7h10v10h-10z M10 10h4v4h-4z M9 3v4 M15 3v4 M9 17v4 M15 17v4 M3 9h4 M3 15h4 M17 9h4 M17 15h4",
        memory: "M3 8h18v8h-18z M7 8v8 M11 8v8 M15 8v8 M5 16v3 M19 16v3",
        down: "M12 4v14 M6 12l6 6l6 -6",
        up: "M12 20v-14 M6 12l6 -6l6 6",
        globe: "M3 12a9 9 0 1 0 18 0a9 9 0 1 0 -18 0 M3 12h18 M12 3a14 14 0 0 1 0 18 M12 3a14 14 0 0 0 0 18",
        // tools
        power: "M12 3v9 M6.3 6.3a8 8 0 1 0 11.4 0",
        lock: "M6 11h12v9h-12z M8.5 11v-3a3.5 3.5 0 0 1 7 0v3",
        logout: "M14 4h4a2 2 0 0 1 2 2v12a2 2 0 0 1 -2 2h-4 M10 16l-4 -4l4 -4 M6 12h10",
        restart: "M4 12a8 8 0 1 0 2.3 -5.7 M4 4v4h4",
        timer: "M5 13a7 7 0 1 0 14 0a7 7 0 1 0 -14 0 M12 13v-3.5 M10 3h4 M12 3v3",
        stopwatch: "M5 13a7 7 0 1 0 14 0a7 7 0 1 0 -14 0 M12 13l3 -3 M10 3h4 M17.5 6.5l1.5 -1.5",
        focus: "M4 12a8 8 0 1 0 16 0a8 8 0 1 0 -16 0 M8 12a4 4 0 1 0 8 0a4 4 0 1 0 -8 0 M12 12v0.01",
        picker: "M15 4l5 5 M13.5 5.5l5 5l-9 9h-5v-5z M8 12l4 4",
        camera: "M4 8h3l2 -3h6l2 3h3a1 1 0 0 1 1 1v9a1 1 0 0 1 -1 1h-16a1 1 0 0 1 -1 -1v-9a1 1 0 0 1 1 -1z M9 13a3 3 0 1 0 6 0a3 3 0 1 0 -6 0",
        shelf: "M4 13l2.5 -7h11l2.5 7 M4 13v5a1 1 0 0 0 1 1h14a1 1 0 0 0 1 -1v-5 M4 13h4.5a3.5 3.5 0 0 0 7 0h4.5",
        file: "M7 3h7l5 5v12a1 1 0 0 1 -1 1h-11a1 1 0 0 1 -1 -1v-16a1 1 0 0 1 1 -1z M14 3v5h5",
        folder: "M3 7a2 2 0 0 1 2 -2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1 -2 2h-14a2 2 0 0 1 -2 -2z",
        note: "M6 4h12v16h-12z M9 9h6 M9 13h6 M9 17h3",
        copy: "M9 9h11v11h-11z M5 15h-1v-11h11v1",
        plus: "M12 5v14 M5 12h14",
        minus: "M5 12h14",
        trash: "M4 7h16 M9 7v-3h6v3 M6 7l1 13h10l1 -13",
        flag: "M5 21v-17 M5 4h11l-2 4l2 4h-11",
        // weather
        cloud: "M7 18a4.5 4.5 0 0 1 -0.5 -9a6 6 0 0 1 11.5 1.5a4 4 0 0 1 0 7.5z",
        cloudSun: "M8 4v1.5 M3.5 9h1.5 M4.8 5.8l1 1 M11.2 5.8l-1 1 M6 11a3 3 0 1 1 5 -2.2 M9 19a4 4 0 0 1 -0.3 -8a5 5 0 0 1 9.6 1.4a3.4 3.4 0 0 1 0 6.6z",
        fog: "M4 9h16 M6 13h12 M4 17h16",
        rain: "M7 14a4.5 4.5 0 0 1 -0.5 -9a6 6 0 0 1 11.5 1.5a4 4 0 0 1 0 7.5 M8 17l-1 3 M12 17l-1 3 M16 17l-1 3",
        snow: "M7 14a4.5 4.5 0 0 1 -0.5 -9a6 6 0 0 1 11.5 1.5a4 4 0 0 1 0 7.5 M8 18v0.01 M12 18v0.01 M16 18v0.01 M10 21v0.01 M14 21v0.01",
        storm: "M7 14a4.5 4.5 0 0 1 -0.5 -9a6 6 0 0 1 11.5 1.5a4 4 0 0 1 0 7.5 M13 13l-2.5 4h3l-2.5 4",
        // music
        expand: "M4 9v-5h5 M20 9v-5h-5 M4 15v5h5 M20 15v5h-5",
        lyrics: "M5 6h14 M5 10h10 M5 14h14 M5 18h7",
        mixer: "M6 4v16 M12 4v16 M18 4v16 M4 14h4 M10 8h4 M16 16h4"
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

    // The locale's short time, never with seconds ("14:05", "2:05 PM").
    function timeFormat() {
        return Qt.locale().timeFormat(Locale.ShortFormat).replace(/[:.]ss/, "").replace(/ t$/, "").trim()
    }

    function ago(ms) {
        const s = Math.max(0, Math.round((Date.now() - ms) / 1000))
        if (s < 5) return qsTr("now")
        if (s < 60) return qsTr("%1 s").arg(s)
        if (s < 3600) return qsTr("%1 min").arg(Math.round(s / 60))
        return qsTr("%1 h").arg(Math.round(s / 3600))
    }
}
