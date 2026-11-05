pragma Singleton
import QtQuick

// Togu's palette. Dark is the default: this is a player.
QtObject {
    readonly property bool dark: Settings.theme !== "light"

    readonly property color bg: dark ? "#0a002a" : "#ffffff"
    readonly property color bgApp: dark ? "#100536" : "#f7f3ff"
    readonly property color bgApp2: dark ? "#0a002a" : "#ede3ff"
    readonly property color raised: dark ? "#202047" : "#ffffff"
    readonly property color raised2: dark ? "#2b2b60" : "#f4f0fb"
    readonly property color tint: dark ? "#36206d" : "#ede3ff"
    readonly property color text: dark ? "#ffffff" : "#00005a"
    readonly property color text2: dark ? Qt.rgba(1, 1, 1, 0.62) : Qt.rgba(0, 0, 90 / 255, 0.6)
    readonly property color text3: dark ? Qt.rgba(1, 1, 1, 0.4) : Qt.rgba(0, 0, 90 / 255, 0.35)
    readonly property color line: dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 90 / 255, 0.12)
    readonly property color fill: dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 90 / 255, 0.06)
    readonly property color fillHover: dark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0, 0, 90 / 255, 0.1)

    readonly property color purple: dark ? "#b272ff" : "#672fc3"
    readonly property color purpleLow: dark ? "#30145e" : "#ede3ff"
    readonly property color pink: "#fe4670"
    readonly property color pinkLow: dark ? "#73294b" : "#ffe0e7"
    readonly property color orange: dark ? "#fba335" : "#ff9345"
    readonly property color orangeLow: dark ? "#724e34" : "#ffebdc"
    readonly property color blue: "#3c91ff"
    readonly property color blueLow: dark ? "#254785" : "#dff4ff"
    readonly property color teal: "#00c7a4"
    readonly property color tealLow: dark ? "#0d5d60" : "#ddfff9"
    readonly property color tealText: dark ? "#5fe6cf" : "#00a88a"
    readonly property color grey: "#989eae"
    readonly property color greyLow: dark ? "#2b2b60" : "#ededf3"
    readonly property color accent: purple

    // Word statuses, as in togu.
    function statusColor(st) {
        switch (st) {
        case "unknown": return "#fe4670"
        case "learning": return "#ff9345"
        case "tracked": return "#c6a5fa"
        case "known": return "#00c7a4"
        case "ignored": return "#989eae"
        }
        return "transparent"
    }
    function toneColor(t) {
        return ({ teal: teal, blue: blue, purple: purple, orange: orange, pink: pink })[t] || purple
    }
    function toneLow(t) {
        return ({ teal: tealLow, blue: blueLow, purple: purpleLow, orange: orangeLow, pink: pinkLow })[t] || purpleLow
    }
    // Pitch accent patterns: heiban, atamadaka, nakadaka, odaka, kifuku.
    function pitchColor(p) {
        return ({ h: "#2880ff", a: "#fe4670", n: "#fba335", o: "#38b8a1", k: "#8d4bf6" })[p] || text
    }
    function starColor(n) {
        return ["#00005a", "#fe4670", "#ff9345", "#672fc3", "#3c91ff", "#00c7a4"][Math.max(0, Math.min(5, n))]
    }

    readonly property string sans: "Inter"
    readonly property string round: "Fredoka"
    readonly property string jp: "Noto Sans CJK JP"
    readonly property int radius: 14
    readonly property int radiusLg: 24
}
