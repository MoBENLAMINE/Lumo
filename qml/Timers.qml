import QtQuick

// The timer, the stopwatch and focus sessions, kept by the island so they go
// on whatever page shows. Nothing ticks unless one of them runs.
Item {
    id: t
    visible: false

    // ---- timer
    property int timerSet: 5 * 60          // seconds asked for
    property real timerEnd: 0              // ms since epoch while running
    property int timerLeft: timerSet       // seconds, when paused
    readonly property bool timerRunning: timerEnd > 0
    function startTimer() { timerEnd = Date.now() + timerLeft * 1000 }
    function pauseTimer() { timerLeft = Math.max(0, Math.ceil((timerEnd - Date.now()) / 1000)); timerEnd = 0 }
    function resetTimer() { timerEnd = 0; timerLeft = timerSet }
    function setTimer(seconds) { timerSet = Math.max(10, seconds); if (!timerRunning) timerLeft = timerSet }

    // ---- stopwatch
    property real watchStart: 0            // ms since epoch while running
    property int watchKept: 0              // ms counted before the last pause
    property var laps: []                  // ms, newest first
    readonly property bool watchRunning: watchStart > 0
    function watchMs() { return watchKept + (watchRunning ? Date.now() - watchStart : 0) }
    function startWatch() { watchStart = Date.now() }
    function pauseWatch() { watchKept = watchMs(); watchStart = 0 }
    function resetWatch() { watchStart = 0; watchKept = 0; laps = [] }
    function lap() { if (watchRunning) laps = [watchMs()].concat(laps).slice(0, 20) }

    // ---- focus: rounds of focus and break
    property int focusMinutes: 25
    property int breakMinutes: 5
    property int rounds: 4
    property int round: 0                  // 0: not started
    property bool onBreak: false
    property real focusEnd: 0
    property int focusLeft: focusMinutes * 60
    readonly property bool focusRunning: focusEnd > 0
    readonly property bool focusActive: round > 0
    function startFocus() {
        if (round === 0) { round = 1; onBreak = false; focusLeft = focusMinutes * 60 }
        focusEnd = Date.now() + focusLeft * 1000
    }
    function pauseFocus() { focusLeft = Math.max(0, Math.ceil((focusEnd - Date.now()) / 1000)); focusEnd = 0 }
    function stopFocus() { focusEnd = 0; round = 0; onBreak = false; focusLeft = focusMinutes * 60 }

    // what the pill's bubble counts: the soonest running countdown
    readonly property bool anyRunning: timerRunning || watchRunning || focusRunning
    property real now: Date.now()
    readonly property int timerRemaining: timerRunning ? Math.max(0, Math.ceil((timerEnd - now) / 1000)) : timerLeft
    readonly property int focusRemaining: focusRunning ? Math.max(0, Math.ceil((focusEnd - now) / 1000)) : focusLeft
    readonly property string bubbleKind: focusRunning ? "focus" : timerRunning ? "timer" : watchRunning ? "stopwatch" : ""
    readonly property string bubbleText: bubbleKind === "focus" ? clock(focusRemaining)
                                       : bubbleKind === "timer" ? clock(timerRemaining)
                                       : bubbleKind === "stopwatch" ? clock(Math.floor((now - watchStart + watchKept) / 1000)) : ""

    signal finished(string kind, string message)

    function clock(seconds) {
        const s = Math.max(0, Math.floor(seconds))
        const h = Math.floor(s / 3600), m = Math.floor(s / 60) % 60, r = s % 60
        return (h > 0 ? h + ":" + String(m).padStart(2, "0") : m) + ":" + String(r).padStart(2, "0")
    }
    function watchText(ms) {
        return clock(ms / 1000) + "." + String(Math.floor(ms / 10) % 100).padStart(2, "0")
    }

    Timer {
        // 4 times a second for the stopwatch's hundredths, once a second otherwise
        running: t.anyRunning
        interval: t.watchRunning ? 250 : 1000
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            t.now = Date.now()
            if (t.timerRunning && t.now >= t.timerEnd) {
                t.timerEnd = 0
                t.timerLeft = t.timerSet
                t.finished("timer", qsTr("Time's up"))
            }
            if (t.focusRunning && t.now >= t.focusEnd) {
                if (!t.onBreak) {
                    if (t.round >= t.rounds) {
                        t.stopFocus()
                        t.finished("focus", qsTr("Focus done: %1 rounds").arg(t.rounds))
                        return
                    }
                    t.onBreak = true
                    t.focusLeft = t.breakMinutes * 60
                    t.finished("focus", qsTr("Break time"))
                } else {
                    t.onBreak = false
                    t.round += 1
                    t.focusLeft = t.focusMinutes * 60
                    t.finished("focus", qsTr("Back to focus: round %1 of %2").arg(t.round).arg(t.rounds))
                }
                t.focusEnd = Date.now() + t.focusLeft * 1000
            }
        }
    }
}
