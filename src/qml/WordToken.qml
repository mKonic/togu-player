import QtQuick

// One token of a subtitle or text line: furigana runs, the word, a status bar under it,
// card/tracked marks and recommended stars, optional pitch colour and gloss.
Item {
    id: w
    property var tok: ({})
    property real fontSize: 40
    property string fontFamily: Theme.jp
    property int fontWeight: 500
    property color textColor: "white"
    property bool outline: true
    property bool active: false
    property bool inSentence: false
    signal entered()
    signal exited()
    signal clicked(var mouse)

    readonly property bool isWord: !!tok.w
    readonly property string st: tok.st || "unknown"
    readonly property bool hovered: area.containsMouse
    readonly property bool showFuri: {
        if (!isWord || !tok.segs || tok.segs.length === 0) return false
        const f = Settings.furigana
        if (f === "all") return true
        if (f === "unknown") return st === "unknown" || st === "tracked"
        if (f === "hover") return hovered || active
        return false
    }
    readonly property bool showGloss: isWord && !!tok.g && (st === "unknown" || st === "tracked") &&
                                      (Settings.wordGloss === "unknown" || (Settings.wordGloss === "hover" && (hovered || active)))
    readonly property color pitchColor: {
        if (!isWord || !tok.p) return textColor
        if (Settings.pitchColors === "all" || (Settings.pitchColors === "hover" && (hovered || active))) return Theme.pitchColor(tok.p)
        return textColor
    }
    readonly property int rec: tok.rec || 0
    readonly property real furiSize: fontSize * 0.5

    implicitWidth: Math.max(body.implicitWidth, gloss.visible ? Math.min(gloss.implicitWidth, fontSize * 6) : 0)
    implicitHeight: furiSize * 1.15 + body.implicitHeight + (gloss.visible ? gloss.implicitHeight : 0) + 8

    Rectangle {
        // the sentence around the hovered word, faintly
        anchors.fill: body
        anchors.margins: -2
        radius: 4
        color: Qt.rgba(40 / 255, 128 / 255, 1, 0.35)
        visible: w.inSentence && w.isWord && w.rec === 0
    }
    Rectangle {
        // boxed recommended word
        anchors.fill: body
        anchors.margins: -3
        radius: 5
        visible: w.rec > 0 && Settings.statusColors
        color: Qt.rgba(1, 1, 1, 0.92)
        border.width: Math.max(1, w.fontSize * 0.04)
        border.color: Theme.starColor(w.rec)
    }

    Row {
        id: body
        y: w.furiSize * 1.15
        anchors.horizontalCenter: parent.horizontalCenter
        Repeater {
            model: w.isWord && w.tok.segs && w.tok.segs.length ? w.tok.segs : [{ t: w.tok.s, r: "" }]
            delegate: Item {
                required property var modelData
                width: Math.max(seg.implicitWidth, furi.visible ? furi.implicitWidth : 0)
                height: seg.implicitHeight
                Text {
                    id: furi
                    visible: w.showFuri && modelData.r !== ""
                    text: modelData.r
                    font.family: w.fontFamily
                    font.pixelSize: w.furiSize
                    font.weight: Font.Normal
                    color: w.rec > 0 && Settings.statusColors ? Theme.starColor(w.rec) : w.textColor
                    style: w.outline && !(w.rec > 0) ? Text.Outline : Text.Normal
                    styleColor: "#000"
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: -w.furiSize * 1.12
                }
                Text {
                    id: seg
                    text: modelData.t
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.family: w.fontFamily
                    font.pixelSize: w.fontSize
                    font.weight: w.fontWeight
                    color: w.rec > 0 && Settings.statusColors ? (Settings.pitchColors !== "off" ? "#000" : Theme.starColor(w.rec)) : w.pitchColor
                    style: w.outline && !(w.rec > 0 && Settings.statusColors) ? Text.Outline : Text.Normal
                    styleColor: "#000"
                    renderType: Text.QtRendering
                }
            }
        }
    }

    // Status bar under the word: faint until hovered.
    Rectangle {
        visible: w.isWord && Settings.statusColors && w.st !== "known" && w.st !== "ignored" || (w.isWord && Settings.statusColors && (w.hovered || w.active))
        anchors.horizontalCenter: body.horizontalCenter
        y: body.y + body.height + 1
        width: body.width - 4
        height: Math.max(2, w.fontSize * 0.06)
        radius: height / 2
        color: Theme.statusColor(w.st)
        opacity: (w.hovered || w.active) ? 1 : (w.st === "tracked" ? 1 : 0.55)
    }
    // Check for words with a card (or learning), paw for tracked.
    Icon {
        visible: w.isWord && Settings.statusColors && (w.st === "learning" || w.st === "tracked" || (w.tok.c && (w.st === "unknown" || w.hovered || w.active))) && w.rec === 0
        name: w.st === "tracked" ? "paw" : "check"
        size: Math.max(9, w.fontSize * 0.26)
        weight: 3
        color: w.st === "tracked" ? "#672fc3" : w.tok.c && w.st !== "learning" ? Theme.statusColor(w.st) : "#ff9345"
        anchors.horizontalCenter: body.horizontalCenter
        y: body.y + body.height + Math.max(3, w.fontSize * 0.08)
    }
    // Stars under a recommended word.
    Row {
        visible: w.rec > 0 && Settings.statusColors && (Settings.stars === "all" || (Settings.stars === "hover" && (w.hovered || w.active)))
        anchors.horizontalCenter: body.horizontalCenter
        y: body.y + body.height + 4
        Repeater {
            model: w.rec
            Icon { name: "star"; size: Math.max(8, w.fontSize * 0.22); color: Theme.starColor(w.rec) }
        }
    }
    Text {
        id: gloss
        visible: w.showGloss
        text: w.tok.g || ""
        width: Math.min(implicitWidth, w.fontSize * 6)
        elide: Text.ElideRight
        anchors.horizontalCenter: body.horizontalCenter
        y: body.y + body.height + Math.max(4, w.fontSize * 0.12)
        font.family: Theme.sans
        font.pixelSize: Math.max(10, w.fontSize * 0.36)
        color: w.textColor
        opacity: 0.75
        style: w.outline ? Text.Outline : Text.Normal
        styleColor: "#000"
    }

    MouseArea {
        id: area
        anchors.fill: body
        anchors.margins: -2
        hoverEnabled: true
        enabled: w.isWord
        cursorShape: w.isWord ? Qt.PointingHandCursor : Qt.ArrowCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        onEntered: w.entered()
        onExited: w.exited()
        onClicked: mouse => w.clicked(mouse)
    }
}
