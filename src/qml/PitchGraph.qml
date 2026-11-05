import QtQuick

// Pitch accent drawn over the morae: dots high or low, a line between them, the
// particle after the word hollow.
Item {
    id: g
    property string reading: ""
    property int position: 0
    property color color: Theme.pitchColor(position === 0 ? "h" : position === 1 ? "a" : "n")
    property real moraWidth: 17
    readonly property var morae: Togu.pitchMorae(reading, position)
    implicitWidth: morae.length * moraWidth
    implicitHeight: 34

    Canvas {
        id: cv
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const hi = 6, lo = 18
            ctx.strokeStyle = g.color
            ctx.fillStyle = g.color
            ctx.lineWidth = 1.6
            ctx.beginPath()
            for (let i = 0; i < g.morae.length; i++) {
                const x = i * g.moraWidth + g.moraWidth / 2, y = g.morae[i].high ? hi : lo
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
            }
            ctx.stroke()
            for (let i = 0; i < g.morae.length; i++) {
                const x = i * g.moraWidth + g.moraWidth / 2, y = g.morae[i].high ? hi : lo
                ctx.beginPath()
                ctx.arc(x, y, 3, 0, Math.PI * 2)
                if (g.morae[i].particle) { ctx.fillStyle = Theme.raised; ctx.fill(); ctx.stroke(); ctx.fillStyle = g.color }
                else ctx.fill()
            }
        }
        Component.onCompleted: requestPaint()
    }
    onMoraeChanged: cv.requestPaint()
    onColorChanged: cv.requestPaint()
    Row {
        y: 22
        Repeater {
            model: g.morae
            Text {
                width: g.moraWidth
                horizontalAlignment: Text.AlignHCenter
                text: modelData.m
                font.family: Theme.jp
                font.pixelSize: 10
                color: Theme.text2
            }
        }
    }
}
