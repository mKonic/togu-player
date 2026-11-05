import QtQuick

// A line of tokens laid out in centred rows; "\n" in the text starts a new row.
Item {
    id: root
    property var tokens: []
    property real fontSize: 40
    property string fontFamily: Theme.jp
    property int fontWeight: 500
    property color textColor: "white"
    property bool outline: true
    property real maxWidth: 800
    property int activeIndex: -1
    property var sentenceRange: [-1, -1]
    property int align: Text.AlignHCenter
    signal wordEntered(int index, Item item)
    signal wordExited(int index)
    signal wordClicked(int index, Item item, var mouse)

    // rows: arrays of {i, tok}
    readonly property var rows: {
        const out = [[]]
        for (let i = 0; i < tokens.length; i++) {
            const t = tokens[i]
            const s = t.s || ""
            if (!t.w && s.indexOf("\n") >= 0) { out.push([]); continue }
            out[out.length - 1].push({ i: i, tok: t })
        }
        return out.filter(r => r.length > 0)
    }
    implicitWidth: col.implicitWidth
    implicitHeight: col.implicitHeight
    width: Math.min(implicitWidth, maxWidth)
    height: implicitHeight

    function itemAt(index) {
        for (let r = 0; r < rowRep.count; r++) {
            const row = rowRep.itemAt(r)
            for (let c = 0; c < row.cells.count; c++) {
                const it = row.cells.itemAt(c)
                if (it && it.idx === index) return it
            }
        }
        return null
    }

    Column {
        id: col
        anchors.horizontalCenter: root.align === Text.AlignHCenter ? parent.horizontalCenter : undefined
        spacing: 0
        Repeater {
            id: rowRep
            model: root.rows
            delegate: Flow {
                id: rowFlow
                required property var modelData
                property alias cells: cellRep
                width: Math.min(root.maxWidth, rowWidth)
                anchors.horizontalCenter: root.align === Text.AlignHCenter ? parent.horizontalCenter : undefined
                property real rowWidth: {
                    let wsum = 0
                    for (let c = 0; c < cellRep.count; c++) { const it = cellRep.itemAt(c); if (it) wsum += it.implicitWidth }
                    return wsum + 1
                }
                Repeater {
                    id: cellRep
                    model: rowFlow.modelData
                    delegate: WordToken {
                        required property var modelData
                        readonly property int idx: modelData.i
                        tok: modelData.tok
                        fontSize: root.fontSize
                        fontFamily: root.fontFamily
                        fontWeight: root.fontWeight
                        textColor: root.textColor
                        outline: root.outline
                        active: root.activeIndex === idx
                        inSentence: idx >= root.sentenceRange[0] && idx <= root.sentenceRange[1]
                        onEntered: root.wordEntered(idx, this)
                        onExited: root.wordExited(idx)
                        onClicked: mouse => root.wordClicked(idx, this, mouse)
                    }
                }
            }
        }
    }
}
