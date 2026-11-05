import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Togu

// Immersion time and word growth.
Item {
    id: sp
    property int days: 30
    readonly property var daily: { const dep = Stats.revision; return dep === undefined ? null : Stats.daily(days) }
    readonly property var topList: { const dep = Stats.revision; return dep === undefined ? null : Stats.topSeries(days, 8) }
    readonly property var totals: { const dep = Stats.revision; return dep === undefined ? null : Stats.totals() }
    readonly property var known: { const dep = Stats.revision; return dep === undefined ? null : Stats.knownHistory(days) }
    // Chart purple: one step darker than Theme.purple in dark mode so bars sit inside the lightness band.
    readonly property color ink: Theme.dark ? "#a066ff" : "#672fc3"

    function dur(s) {
        s = Math.round(s / 60)
        if (s < 60) return qsTr("%1 min").arg(s)
        const h = Math.floor(s / 60), m = s % 60
        return m ? qsTr("%1 h %2 min").arg(h).arg(m) : qsTr("%1 h").arg(h)
    }
    function kindName(k) {
        return ({ video: qsTr("Video"), audio: qsTr("Audio"), book: qsTr("Reading"), manga: qsTr("Manga") })[k] || k
    }

    Flickable {
        anchors.fill: parent
        contentHeight: col.implicitHeight + 56
        clip: true
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: col
            x: 28; y: 28
            width: parent.width - 56
            spacing: 18

            RowLayout {
                Text { text: qsTr("Stats"); color: Theme.text; font.pixelSize: 28; font.weight: Font.Bold; font.family: Theme.round; Layout.fillWidth: true }
                Repeater {
                    model: [7, 30, 90]
                    TButton {
                        small: true
                        quiet: sp.days !== modelData
                        tone: sp.days === modelData ? "purple" : "plain"
                        text: qsTr("%1 days").arg(modelData)
                        onClicked: sp.days = modelData
                    }
                }
            }

            // ------------------------------------------------ tiles
            GridLayout {
                Layout.fillWidth: true
                columns: Math.max(2, Math.min(5, Math.floor(width / 200)))
                columnSpacing: 12
                rowSpacing: 12

                Tile {
                    label: qsTr("Today")
                    value: sp.dur(sp.totals.today || 0)
                    note: qsTr("of %1 goal").arg(sp.dur(sp.totals.goal || 0))
                    progress: sp.totals.goal ? Math.min(1, sp.totals.today / sp.totals.goal) : -1
                }
                Tile {
                    label: qsTr("Streak")
                    value: sp.totals.streak === 1 ? qsTr("1 day") : qsTr("%1 days").arg(sp.totals.streak || 0)
                    note: qsTr("a minute or more each day")
                }
                Tile { label: qsTr("Last 7 days"); value: sp.dur(sp.totals.week || 0); note: qsTr("%1 all time").arg(sp.dur(sp.totals.all || 0)) }
                Tile { label: qsTr("Known words"); value: (sp.totals.known || 0).toLocaleString(Qt.locale(), "f", 0); note: qsTr("%1 learning").arg(sp.totals.learning || 0) }
                Tile { label: qsTr("Cards"); value: (sp.totals.cards || 0).toLocaleString(Qt.locale(), "f", 0); note: qsTr("%1 in the last 7 days").arg(sp.totals.cardsWeek || 0) }
            }

            // ------------------------------------------------ daily bars
            Panel {
                title: qsTr("Immersion per day")
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                BarChart {
                    anchors.fill: parent
                    points: sp.daily
                    color: sp.ink
                    goal: sp.totals.goal || 0
                    fmt: sp.dur
                    kindName: sp.kindName
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                // ------------------------------------------------ top series
                Panel {
                    title: qsTr("Most watched and read")
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 300
                    Text {
                        visible: sp.topList.length === 0
                        text: qsTr("Nothing yet in this range.")
                        color: Theme.text3; font.pixelSize: 13
                    }
                    Column {
                        width: parent.width
                        spacing: 10
                        Repeater {
                            model: sp.topList
                            Item {
                                width: parent.width
                                height: 26
                                readonly property real maxS: sp.topList.length ? sp.topList[0].seconds : 1
                                Text {
                                    id: name
                                    width: parent.width * 0.42
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.title || qsTr("Untitled")
                                    color: Theme.text; font.pixelSize: 13; elide: Text.ElideRight
                                }
                                Rectangle {
                                    id: bar
                                    anchors.left: name.right
                                    anchors.leftMargin: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    height: 10
                                    radius: 4
                                    width: Math.max(4, (parent.width - name.width - 100) * modelData.seconds / maxS)
                                    color: sp.ink
                                }
                                Text {
                                    anchors.left: bar.right
                                    anchors.leftMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: sp.dur(modelData.seconds)
                                    color: Theme.text2; font.pixelSize: 12
                                }
                            }
                        }
                    }
                }

                // ------------------------------------------------ known words
                Panel {
                    title: qsTr("Known words")
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 300
                    LineChart {
                        anchors.fill: parent
                        points: sp.known
                        color: sp.ink
                        emptyText: qsTr("Togu records your known words once a day. The line appears after the second day.")
                        tip: d => "<b>" + d.label + "</b>  " + qsTr("%1 known").arg(d.known) + "<br>" + qsTr("%1 learning").arg(d.learning)
                    }
                }
            }

            // ------------------------------------------------ comprehension
            Panel {
                title: qsTr("Library comprehension")
                Layout.fillWidth: true
                Layout.preferredHeight: 260
                LineChart {
                    anchors.fill: parent
                    points: sp.known.filter(d => d.comprehension !== undefined && d.comprehension !== null)
                    field: "comprehension"
                    color: sp.ink
                    ceilV: 100
                    fmtAxis: v => Math.round(v) + "%"
                    emptyText: qsTr("How much of your library you understand, recorded once a day. The line appears after the second day.")
                    tip: d => "<b>" + d.label + "</b>  " + qsTr("%1% of words known").arg(Math.round(d.comprehension))
                }
            }
        }
    }

    // ------------------------------------------------------------ pieces
    component Tile: Rectangle {
        property string label
        property string value
        property string note
        property real progress: -1
        Layout.fillWidth: true
        Layout.fillHeight: true
        implicitHeight: tc.implicitHeight + 28
        radius: 16
        color: Theme.raised
        border.color: Theme.line
        Column {
            id: tc
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 14
            spacing: 4
            Text { text: label; color: Theme.text2; font.pixelSize: 12; font.weight: Font.DemiBold }
            Text { text: value; color: Theme.text; font.pixelSize: 24; font.weight: Font.Bold; font.family: Theme.round }
            Rectangle {
                visible: progress >= 0
                width: parent.width
                height: 6
                radius: 3
                color: Theme.fill
                Rectangle { width: Math.max(progress > 0 ? 6 : 0, parent.width * progress); height: parent.height; radius: 3; color: progress >= 1 ? Theme.teal : sp.ink }
            }
            Text { text: note; color: Theme.text3; font.pixelSize: 12; width: parent.width; elide: Text.ElideRight }
        }
    }

    component Panel: Rectangle {
        property string title
        default property alias content: body.data
        radius: 16
        color: Theme.raised
        border.color: Theme.line
        Text { id: head; x: 16; y: 14; text: title; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
        Item {
            id: body
            anchors.fill: parent
            anchors.topMargin: head.y + head.height + 12
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            anchors.bottomMargin: 14
        }
    }

    component Tip: Rectangle {
        property alias text: tt.text
        width: tt.implicitWidth + 16
        height: tt.implicitHeight + 10
        radius: 8
        color: Theme.dark ? "#2b2b60" : "#00005a"
        z: 10
        Text { id: tt; anchors.centerIn: parent; color: "white"; font.pixelSize: 12; lineHeight: 1.2 }
    }

    // Seconds per day; hover a bar for the day and the split by kind.
    component BarChart: Item {
        id: bc
        property var points: []
        property color color
        property real goal: 0
        property var fmt
        property var kindName
        property int hover: -1
        readonly property real axisW: 52
        readonly property real labelH: 20
        readonly property real plotW: width - axisW
        readonly property real plotH: height - labelH
        readonly property real maxV: {
            let m = goal
            for (const d of points) m = Math.max(m, d.seconds)
            return m
        }
        // Nice tick step in minutes.
        readonly property var ticks: {
            const mins = Math.max(1, maxV / 60)
            const steps = [5, 10, 15, 30, 60, 120, 180, 240, 360, 480, 720]
            let step = steps[steps.length - 1]
            for (const s of steps) if (mins / s <= 4) { step = s; break }
            const out = []
            for (let v = 0; v <= mins + step * 0.01; v += step) out.push(v * 60)
            if (out[out.length - 1] < maxV) out.push(out[out.length - 1] + step * 60)
            return out
        }
        readonly property real topV: ticks.length ? ticks[ticks.length - 1] : 1
        function yFor(v) { return plotH - plotH * v / Math.max(1, topV) }

        Repeater {
            model: bc.ticks
            Item {
                y: bc.yFor(modelData)
                width: bc.width
                Rectangle { x: bc.axisW; width: bc.plotW; height: 1; color: Theme.line; opacity: index === 0 ? 1 : 0.6 }
                Text {
                    width: bc.axisW - 8
                    anchors.verticalCenter: parent.top
                    horizontalAlignment: Text.AlignRight
                    text: modelData === 0 ? "0" : (modelData % 3600 === 0 ? modelData / 3600 + " h" : modelData / 60 + " m")
                    color: Theme.text3; font.pixelSize: 11
                }
            }
        }
        // Goal reference line.
        Item {
            visible: bc.goal > 0
            x: bc.axisW
            y: bc.yFor(bc.goal)
            width: bc.plotW
            Row {
                spacing: 4
                Repeater { model: Math.ceil(bc.plotW / 8); Rectangle { width: 4; height: 1; color: Theme.text2 } }
            }
            Text { anchors.right: parent.right; anchors.bottom: parent.top; anchors.bottomMargin: 2; text: qsTr("Goal"); color: Theme.text2; font.pixelSize: 11 }
        }
        Repeater {
            model: bc.points
            Item {
                readonly property real slot: bc.plotW / Math.max(1, bc.points.length)
                x: bc.axisW + index * slot
                width: slot
                height: bc.height
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.max(2, Math.min(28, parent.slot - 2))
                    y: bc.yFor(modelData.seconds)
                    height: bc.plotH - y
                    topLeftRadius: Math.min(4, width / 2)
                    topRightRadius: Math.min(4, width / 2)
                    color: bc.color
                    opacity: bc.hover < 0 || bc.hover === index ? 1 : 0.55
                    visible: modelData.seconds > 0
                }
                // Sparse date labels: about seven across.
                Text {
                    visible: index % Math.max(1, Math.ceil(bc.points.length / 7)) === (bc.points.length - 1) % Math.max(1, Math.ceil(bc.points.length / 7))
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: bc.plotH + 5
                    text: modelData.label
                    color: Theme.text3; font.pixelSize: 11
                }
            }
        }
        Tip {
            visible: bc.hover >= 0
            readonly property var d: bc.hover >= 0 ? bc.points[bc.hover] : null
            text: {
                if (!d) return ""
                let s = "<b>" + d.label + "</b>  " + bc.fmt(d.seconds)
                for (const k in d.kinds) s += "<br>" + bc.kindName(k) + ": " + bc.fmt(d.kinds[k])
                return s
            }
            x: {
                const slot = bc.plotW / Math.max(1, bc.points.length)
                const cx = bc.axisW + (bc.hover + 0.5) * slot
                return Math.max(0, Math.min(bc.width - width, cx - width / 2))
            }
            y: d ? Math.max(0, bc.yFor(d.seconds) - height - 8) : 0
        }
        // One hover area over the bars and the tip, so the tip can't steal the hover.
        MouseArea {
            x: bc.axisW
            width: bc.plotW
            height: bc.height
            hoverEnabled: bc.points.length > 0
            onPositionChanged: mouse => bc.hover = Math.max(0, Math.min(bc.points.length - 1,
                Math.floor(mouse.x / Math.max(1, bc.plotW) * bc.points.length)))
            onExited: bc.hover = -1
        }
    }

    // Known words over time with a crosshair.
    component LineChart: Item {
        id: lc
        property var points: []
        property color color
        property string field: "known"
        property var tip: d => ""
        property string emptyText: ""
        property var fmtAxis: v => Math.round(v).toLocaleString(Qt.locale(), "f", 0)
        property real floorV: 0
        property real ceilV: Infinity
        property int hover: -1
        readonly property real axisW: 52
        readonly property real labelH: 20
        readonly property real plotW: width - axisW
        readonly property real plotH: height - labelH
        readonly property real minV: { let m = Infinity; for (const d of points) m = Math.min(m, d[field]); return m === Infinity ? 0 : m }
        readonly property real maxV: { let m = 0; for (const d of points) m = Math.max(m, d[field]); return m }
        // Range padded so a flat line sits in the middle.
        readonly property real lo: Math.max(floorV, Math.floor((minV - Math.max(5, (maxV - minV) * 0.1)) / 10) * 10)
        readonly property real hi: Math.min(ceilV, Math.ceil((maxV + Math.max(5, (maxV - minV) * 0.1)) / 10) * 10)
        function xFor(i) { return axisW + (points.length < 2 ? plotW / 2 : plotW * i / (points.length - 1)) }
        function yFor(v) { return plotH - plotH * (v - lo) / Math.max(1, hi - lo) }

        Text {
            visible: lc.points.length < 2
            anchors.centerIn: parent
            width: parent.width * 0.8
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: lc.emptyText
            color: Theme.text3; font.pixelSize: 13
        }
        Repeater {
            model: lc.points.length < 2 ? [] : [lc.lo, (lc.lo + lc.hi) / 2, lc.hi]
            Item {
                y: lc.yFor(modelData)
                width: lc.width
                Rectangle { x: lc.axisW; width: lc.plotW; height: 1; color: Theme.line; opacity: index === 0 ? 1 : 0.6 }
                Text {
                    width: lc.axisW - 8
                    anchors.verticalCenter: parent.top
                    horizontalAlignment: Text.AlignRight
                    text: lc.fmtAxis(modelData)
                    color: Theme.text3; font.pixelSize: 11
                }
            }
        }
        Text { visible: lc.points.length >= 2; x: lc.axisW; y: lc.plotH + 5; text: lc.points.length ? lc.points[0].label : ""; color: Theme.text3; font.pixelSize: 11 }
        Text { visible: lc.points.length >= 2; anchors.right: parent.right; y: lc.plotH + 5; text: lc.points.length ? lc.points[lc.points.length - 1].label : ""; color: Theme.text3; font.pixelSize: 11 }
        Canvas {
            id: canvas
            anchors.fill: parent
            visible: lc.points.length >= 2
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                if (lc.points.length < 2) return
                ctx.strokeStyle = lc.color
                ctx.lineWidth = 2
                ctx.lineJoin = "round"
                ctx.beginPath()
                for (let i = 0; i < lc.points.length; ++i) {
                    const x = lc.xFor(i), y = lc.yFor(lc.points[i][lc.field])
                    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                }
                ctx.stroke()
            }
            Connections { target: lc; function onPointsChanged() { canvas.requestPaint() } function onWidthChanged() { canvas.requestPaint() } function onHeightChanged() { canvas.requestPaint() } }
        }
        Rectangle {
            visible: lc.hover >= 0
            x: lc.hover >= 0 ? lc.xFor(lc.hover) : 0
            width: 1
            height: lc.plotH
            color: Theme.text3
        }
        Rectangle {
            visible: lc.hover >= 0
            width: 10; height: 10; radius: 5
            color: lc.color
            border.color: Theme.raised
            border.width: 2
            x: lc.hover >= 0 ? lc.xFor(lc.hover) - 5 : 0
            y: lc.hover >= 0 ? lc.yFor(lc.points[lc.hover][lc.field]) - 5 : 0
        }
        Tip {
            visible: lc.hover >= 0
            readonly property var d: lc.hover >= 0 ? lc.points[lc.hover] : null
            text: d ? lc.tip(d) : ""
            x: lc.hover >= 0 ? Math.max(0, Math.min(lc.width - width, lc.xFor(lc.hover) - width / 2)) : 0
            y: d ? Math.max(0, lc.yFor(d[field]) - height - 12) : 0
        }
        MouseArea {
            anchors.fill: parent
            hoverEnabled: lc.points.length >= 2
            onPositionChanged: mouse => {
                const f = (mouse.x - lc.axisW) / Math.max(1, lc.plotW)
                lc.hover = Math.max(0, Math.min(lc.points.length - 1, Math.round(f * (lc.points.length - 1))))
            }
            onExited: lc.hover = -1
        }
    }
}
