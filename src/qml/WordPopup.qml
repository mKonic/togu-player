import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Togu's word popup: entries, definitions per dictionary, pitch, frequency, kanji,
// and the status and card controls along the bottom.
Rectangle {
    id: pop
    property var entries: []
    property var kanjiList: []
    property var libraryLines: []
    signal lineRequested(var hit)
    property int current: 0
    property string tokenKey: ""      // the hovered token's key (status target when no entry)
    property string tokenSurface: ""
    property bool busy: false
    readonly property var entry: entries.length ? entries[Math.min(current, entries.length - 1)] : null
    readonly property string targetKey: entry ? entry.key : tokenKey
    readonly property string targetStatus: entry ? Togu.status(entry.key) : Togu.status(tokenKey)
    readonly property bool hovered: hover.hovered
    property int statusRev: 0
    signal statusChosen(string key, string status)
    signal cardRequested(var entry)
    signal queryRequested(string text)
    signal closeRequested()

    width: 440
    height: Math.min(implicitBodyHeight + head.height + foot.height, maxHeight)
    property real maxHeight: 520
    readonly property real implicitBodyHeight: body.contentHeight + 16
    color: Theme.raised
    topLeftRadius: 24
    topRightRadius: 24
    bottomLeftRadius: 24
    bottomRightRadius: 4
    border.color: Theme.line
    clip: true

    Connections {
        target: Togu
        function onStatusesChanged() { pop.statusRev++ }
    }

    HoverHandler { id: hover }

    // ----- head: word, reading, pitch, frequency
    Column {
        id: head
        width: parent.width
        topPadding: 16
        leftPadding: 20
        rightPadding: 16
        bottomPadding: 8
        spacing: 8
        RowLayout {
            width: parent.width - 36
            spacing: 12
            Furigana {
                segs: pop.entry ? pop.entry.segs : [{ t: pop.tokenSurface, r: "" }]
                size: 30
            }
            Column {
                Layout.fillWidth: true
                spacing: 4
                Row {
                    spacing: 6
                    Repeater {
                        model: pop.entry ? pop.entry.pitches.slice(0, 2) : []
                        delegate: Row {
                            spacing: 4
                            Repeater {
                                model: modelData.positions.slice(0, 2)
                                PitchGraph { reading: pop.entry.reading; position: modelData }
                            }
                        }
                    }
                }
                Row {
                    spacing: 2
                    visible: pop.entry && pop.entry.stars > 0
                    Repeater {
                        model: pop.entry ? pop.entry.stars : 0
                        Icon { name: "star"; size: 12; color: Theme.starColor(pop.entry.stars) }
                    }
                }
            }
            IconButton {
                iconName: "close"; size: 30
                Layout.alignment: Qt.AlignTop
                onClicked: pop.closeRequested()
            }
        }
        Flow {
            width: parent.width - 36
            spacing: 6
            visible: pop.entry && pop.entry.freqs.length > 0
            Repeater {
                model: pop.entry ? pop.entry.freqs.slice(0, 4) : []
                delegate: Rectangle {
                    height: 22
                    width: ft.implicitWidth + 14
                    radius: 11
                    color: Theme.fill
                    Text {
                        id: ft
                        anchors.centerIn: parent
                        text: modelData.dict.replace(/\s*\[.*\]\s*/g, "") + "  " + modelData.display
                        font.pixelSize: 11
                        color: Theme.text2
                    }
                }
            }
        }
        // Other entries for the same text
        Flow {
            width: parent.width - 36
            spacing: 6
            visible: pop.entries.length > 1
            Repeater {
                model: pop.entries.slice(0, 8)
                delegate: Rectangle {
                    height: 26
                    width: et.implicitWidth + 16
                    radius: 13
                    color: index === pop.current ? Theme.purple : Theme.fill
                    Text {
                        id: et
                        anchors.centerIn: parent
                        text: modelData.expression + (modelData.reading !== modelData.expression ? "・" + modelData.reading : "")
                        font.family: Theme.jp
                        font.pixelSize: 12
                        color: index === pop.current ? "white" : Theme.text
                    }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: pop.current = index }
                }
            }
        }
    }

    // ----- body: definitions
    Flickable {
        id: body
        anchors.top: head.bottom
        anchors.bottom: foot.top
        width: parent.width
        contentHeight: bodyCol.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        Column {
            id: bodyCol
            width: body.width
            leftPadding: 20
            rightPadding: 20
            spacing: 14
            Text {
                visible: !pop.entry && !pop.busy
                text: Togu.dicts.length ? qsTr("No dictionary entry") : qsTr("No dictionaries yet. Add some in Settings → Dictionaries.")
                color: Theme.text2
                font.pixelSize: 14
                width: parent.width - 40
                wrapMode: Text.Wrap
            }
            Repeater {
                model: pop.entry ? pop.entry.blocks : []
                delegate: Column {
                    width: bodyCol.width - 40
                    spacing: 4
                    Row {
                        spacing: 6
                        Text {
                            text: modelData.dict.replace(/\s*\[.*\]\s*/g, "")
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            font.capitalization: Font.AllUppercase
                            font.letterSpacing: 0.6
                            color: Theme.purple
                        }
                        Repeater {
                            model: modelData.tags.slice(0, 4)
                            Chip { text: modelData; tone: "blue"; fontSize: 10 }
                        }
                    }
                    Text {
                        width: parent.width
                        text: modelData.html
                        textFormat: Text.RichText
                        wrapMode: Text.Wrap
                        color: Theme.text
                        font.pixelSize: 14
                        font.family: Theme.sans
                        lineHeight: 1.15
                        linkColor: Theme.blue
                        onLinkActivated: link => {
                            if (link.startsWith("togu:lookup?")) pop.queryRequested(decodeURIComponent(link.slice(12)))
                            else Togu.openExternally(link)
                        }
                        MouseArea { anchors.fill: parent; acceptedButtons: Qt.NoButton; cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor }
                    }
                }
            }
            // The word in your own library
            Column {
                visible: pop.libraryLines.length > 0
                width: bodyCol.width - 40
                spacing: 4
                Text {
                    text: qsTr("In your library")
                    font.pixelSize: 11; font.weight: Font.DemiBold; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6
                    color: Theme.teal
                }
                Repeater {
                    model: pop.libraryLines.slice(0, 6)
                    delegate: Rectangle {
                        width: parent.width
                        height: lc.implicitHeight + 10
                        radius: 8
                        color: lh.hovered ? Theme.fill : "transparent"
                        HoverHandler { id: lh; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: pop.lineRequested(modelData) }
                        Column {
                            id: lc
                            x: 6; y: 5
                            width: parent.width - 12
                            Text { width: parent.width; text: modelData.text; color: Theme.text; font.family: Theme.jp; font.pixelSize: 14; wrapMode: Text.Wrap }
                            Text {
                                width: parent.width
                                text: modelData.seriesTitle + (modelData.episode >= 0 ? " · " + qsTr("ep. %1").arg(modelData.episode) : "") + " · " + Togu.formatTime(modelData.start)
                                color: Theme.text3; font.pixelSize: 11; elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
            // Kanji of the word
            Repeater {
                model: pop.kanjiList.slice(0, 4)
                delegate: Row {
                    spacing: 12
                    width: bodyCol.width - 40
                    Text {
                        text: modelData.character
                        font.family: Theme.jp
                        font.pixelSize: 34
                        color: Theme.text
                    }
                    Column {
                        width: parent.width - 60
                        spacing: 2
                        Text { width: parent.width; wrapMode: Text.Wrap; text: modelData.meanings.slice(0, 5).join(", "); color: Theme.text; font.pixelSize: 13 }
                        Text { width: parent.width; wrapMode: Text.Wrap; text: modelData.onyomi.join("、"); color: Theme.pink; font.family: Theme.jp; font.pixelSize: 12; visible: text !== "" }
                        Text { width: parent.width; wrapMode: Text.Wrap; text: modelData.kunyomi.join("、"); color: Theme.blue; font.family: Theme.jp; font.pixelSize: 12; visible: text !== "" }
                    }
                }
            }
            Item { width: 1; height: 4 }
        }
    }

    // ----- foot: statuses and card
    Rectangle {
        id: foot
        anchors.bottom: parent.bottom
        width: parent.width
        height: 58
        color: Theme.raised2
        bottomLeftRadius: 24
        bottomRightRadius: 4
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 10
            spacing: 6
            Text {
                Layout.fillWidth: true
                text: pop.entry ? pop.entry.expression : pop.tokenSurface
                font.family: Theme.jp
                font.pixelSize: 16
                font.weight: Font.Bold
                color: Theme.text
                elide: Text.ElideRight
            }
            Repeater {
                model: [
                    { st: "unknown", tip: qsTr("Unknown (U · 1)"), icon: "close" },
                    { st: "tracked", tip: qsTr("Track (T)"), icon: "paw" },
                    { st: "learning", tip: qsTr("Learning (2)"), icon: "clock" },
                    { st: "known", tip: qsTr("Known (K · 3)"), icon: "check" },
                    { st: "ignored", tip: qsTr("Ignore (I · 4)"), icon: "eyeOff" }
                ]
                delegate: AbstractButton {
                    id: sb
                    readonly property bool on: { const dep = pop.statusRev; return dep === undefined ? null : pop.targetKey !== "" && Togu.status(pop.targetKey) === modelData.st }
                    implicitWidth: 34
                    implicitHeight: 34
                    hoverEnabled: true
                    onClicked: pop.statusChosen(pop.targetKey, modelData.st)
                    background: Rectangle {
                        radius: 17
                        color: sb.on ? Theme.statusColor(modelData.st) : sb.hovered ? Theme.fillHover : Theme.fill
                    }
                    contentItem: Item {
                        Icon {
                            anchors.centerIn: parent
                            name: modelData.icon
                            size: 16
                            color: sb.on ? "white" : Theme.statusColor(modelData.st)
                        }
                    }
                    ToolTip.visible: hovered
                    ToolTip.delay: 500
                    ToolTip.text: modelData.tip
                }
            }
            Rectangle { width: 1; height: 26; color: Theme.line }
            IconButton {
                iconName: "cardAdd"
                tip: qsTr("Create card (Q)")
                size: 38
                color: Theme.purple
                onClicked: pop.cardRequested(pop.entry)
            }
        }
    }
}
