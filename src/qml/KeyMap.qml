pragma Singleton
import QtQuick

// Player shortcuts: actions with default keys, overridden per action in Settings.playerKeys.
// A key is written like "Alt+Shift+I" or "Space".
QtObject {
    readonly property var actions: [
        { id: "pause", group: "playback", label: qsTr("Play / pause"), keys: ["Space", "P"] },
        { id: "prevLine", group: "playback", label: qsTr("Previous line"), keys: ["A", "Left"] },
        { id: "nextLine", group: "playback", label: qsTr("Next line"), keys: ["D", "Right"] },
        { id: "replayLine", group: "playback", label: qsTr("Replay line"), keys: ["S", "Down"] },
        { id: "seekBack", group: "playback", label: qsTr("Seek back"), keys: ["Alt+A", "Alt+Left"] },
        { id: "seekForward", group: "playback", label: qsTr("Seek forward"), keys: ["Alt+D", "Alt+Right"] },
        { id: "slower", group: "playback", label: qsTr("Slower"), keys: ["["] },
        { id: "faster", group: "playback", label: qsTr("Faster"), keys: ["]"] },
        { id: "normalSpeed", group: "playback", label: qsTr("Normal speed"), keys: ["Backspace"] },
        { id: "frameBack", group: "playback", label: qsTr("Frame back"), keys: [","] },
        { id: "frameForward", group: "playback", label: qsTr("Frame forward"), keys: ["."] },
        { id: "mute", group: "playback", label: qsTr("Mute"), keys: ["M"] },
        { id: "fullscreen", group: "playback", label: qsTr("Fullscreen"), keys: ["F"] },

        { id: "togglePrimary", group: "subs", label: qsTr("Show / hide Japanese subtitles"), keys: ["W", "Up"] },
        { id: "toggleSecondary", group: "subs", label: qsTr("Show / hide second subtitles"), keys: ["Shift+W", "Shift+Up"] },
        { id: "toggleAll", group: "subs", label: qsTr("Show / hide all subtitles"), keys: ["R"] },
        { id: "copyLine", group: "subs", label: qsTr("Copy line"), keys: ["C"] },
        { id: "sidePanel", group: "subs", label: qsTr("Subtitle list"), keys: ["G"] },
        { id: "bookmark", group: "subs", label: qsTr("Bookmark line"), keys: ["B"] },
        { id: "nextWord", group: "subs", label: qsTr("Next unknown word"), keys: ["N"] },
        { id: "prevWord", group: "subs", label: qsTr("Previous unknown word"), keys: ["Alt+N"] },
        { id: "subSmaller", group: "subs", label: qsTr("Smaller subtitles"), keys: ["Alt+-"] },
        { id: "subLarger", group: "subs", label: qsTr("Larger subtitles"), keys: ["Alt+=", "Alt++"] },
        { id: "offsetBack", group: "subs", label: qsTr("Subtitles earlier"), keys: ["Alt+I"] },
        { id: "offsetForward", group: "subs", label: qsTr("Subtitles later"), keys: ["Alt+O"] },
        { id: "offset2Back", group: "subs", label: qsTr("Second subtitles earlier"), keys: ["Alt+Shift+I"] },
        { id: "offset2Forward", group: "subs", label: qsTr("Second subtitles later"), keys: ["Alt+Shift+O"] },
        { id: "lineStartsNow", group: "subs", label: qsTr("Current line starts now"), keys: ["Alt+L"] },
        { id: "autoSync", group: "subs", label: qsTr("Sync subtitles to the audio"), keys: ["Alt+Y"] },

        { id: "unknown", group: "words", label: qsTr("Mark unknown"), keys: ["U", "1"] },
        { id: "tracked", group: "words", label: qsTr("Mark tracked"), keys: ["T"] },
        { id: "learning", group: "words", label: qsTr("Mark learning"), keys: ["2"] },
        { id: "known", group: "words", label: qsTr("Mark known"), keys: ["K", "3"] },
        { id: "ignored", group: "words", label: qsTr("Mark ignored"), keys: ["I", "4"] },
        { id: "cardWord", group: "words", label: qsTr("Card for the word"), keys: ["Q"] },
        { id: "cardLine", group: "words", label: qsTr("Card for the line"), keys: ["E"] },
        { id: "queueLine", group: "words", label: qsTr("Save line for later"), keys: ["L"] }
    ]
    readonly property var groups: [
        { id: "playback", label: qsTr("Playback") },
        { id: "subs", label: qsTr("Subtitles") },
        { id: "words", label: qsTr("Words and cards") }
    ]

    function keysFor(id) {
        const o = Settings.playerKeys || {}
        if (o[id] !== undefined) return o[id]
        const a = actions.find(x => x.id === id)
        return a ? a.keys : []
    }
    function isDefault(id) { return (Settings.playerKeys || {})[id] === undefined }
    function setKeys(id, keys) {
        const o = Object.assign({}, Settings.playerKeys || {})
        o[id] = keys
        Settings.set("playerKeys", o)
    }
    function reset(id) {
        const o = Object.assign({}, Settings.playerKeys || {})
        delete o[id]
        Settings.set("playerKeys", o)
    }
    function resetAll() { Settings.set("playerKeys", ({})) }

    readonly property var names: ({
        [Qt.Key_Space]: "Space", [Qt.Key_Left]: "Left", [Qt.Key_Right]: "Right", [Qt.Key_Up]: "Up", [Qt.Key_Down]: "Down",
        [Qt.Key_Backspace]: "Backspace", [Qt.Key_Return]: "Enter", [Qt.Key_Enter]: "Enter", [Qt.Key_Tab]: "Tab",
        [Qt.Key_Home]: "Home", [Qt.Key_End]: "End", [Qt.Key_PageUp]: "PageUp", [Qt.Key_PageDown]: "PageDown",
        [Qt.Key_Delete]: "Delete", [Qt.Key_Insert]: "Insert"
    })
    // "Alt+Shift+I" for a key event; empty for lone modifiers and Escape (always back/close).
    function spec(event) {
        const k = event.key
        if (k === Qt.Key_Shift || k === Qt.Key_Control || k === Qt.Key_Alt || k === Qt.Key_Meta || k === Qt.Key_AltGr || k === Qt.Key_Escape) return ""
        let name = names[k]
        if (!name && k >= Qt.Key_F1 && k <= Qt.Key_F12) name = "F" + (k - Qt.Key_F1 + 1)
        if (!name && k >= 0x21 && k <= 0x7e) name = String.fromCharCode(k).toUpperCase()
        if (!name) return ""
        const m = event.modifiers
        let s = ""
        if (m & Qt.ControlModifier) s += "Ctrl+"
        if (m & Qt.AltModifier) s += "Alt+"
        // Shift is already part of symbols like "+".
        if ((m & Qt.ShiftModifier) && (name.length > 1 || /[A-Z0-9]/.test(name))) s += "Shift+"
        return s + name
    }
    // The action bound to a key event, or "".
    function action(event) {
        const s = spec(event)
        if (!s) return ""
        for (const a of actions) if (keysFor(a.id).indexOf(s) >= 0) return a.id
        return ""
    }
    // Other actions already using a key.
    function conflict(id, key) {
        for (const a of actions) if (a.id !== id && keysFor(a.id).indexOf(key) >= 0) return a
        return null
    }
}
