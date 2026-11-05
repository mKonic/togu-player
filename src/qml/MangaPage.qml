import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Togu

// mokuro manga: pages right to left, text boxes you can hover to read and look up.
FocusScope {
    id: mp
    signal closeRequested()
    signal cardRequested(var entry, var line, string surface, string image)
    property int page: 0
    property bool spread: false
    property var activeBox: null   // {page, box}
    property var boxTokens: []
    property var stats: ({})

    property string path: ""
    function open(p) { path = p; vol.open(p); clock.poke(); forceActiveFocus() }
    TimeCredit { id: clock; kind: "manga"; path: mp.path; active: mp.visible }
    HoverHandler { onPointChanged: clock.poke() }
    function go(p) {
        const n = vol.pages.length
        page = Math.max(0, Math.min(n - 1, p))
        activeBox = null
        popup.visible = false
        vol.savePage(page)
        clock.poke()
    }
    function next() { go(page + (spread ? 2 : 1)) }
    function prev() { go(page - (spread ? 2 : 1)) }

    MangaVolume {
        id: vol
        onOpened: { mp.page = startPage; mp.stats = stats() }
        onFailed: msg => Togu.toast(msg, "pink")
    }
    Connections { target: Togu; function onStatusesChanged() { if (mp.activeBox) mp.boxTokens = Togu.tokenize(mp.activeBox.box.text) } }

    Rectangle { anchors.fill: parent; color: "#0b0b12" }

    // Pages: right to left, so the first of a spread sits on the right.
    Row {
        id: pagesRow
        anchors.centerIn: parent
        anchors.verticalCenterOffset: bar.height / 2
        layoutDirection: Qt.RightToLeft
        spacing: 0
        Repeater {
            model: mp.spread ? [mp.page, mp.page + 1].filter(i => i < vol.pages.length) : [mp.page]
            delegate: Item {
                id: pageItem
                required property int modelData
                readonly property var pg: vol.pages[modelData] || ({})
                readonly property real availW: (mp.width - 20) / (mp.spread ? 2 : 1)
                readonly property real availH: mp.height - bar.height - 20
                readonly property real s: pg.width ? Math.min(availW / pg.width, availH / pg.height) : 1
                width: (pg.width || 1) * s
                height: (pg.height || 1) * s
                Image { anchors.fill: parent; source: pageItem.pg.image || ""; fillMode: Image.PreserveAspectFit; asynchronous: true; smooth: true; mipmap: true }
                Repeater {
                    model: pageItem.pg.boxes || []
                    delegate: Rectangle {
                        x: modelData.x * pageItem.s
                        y: modelData.y * pageItem.s
                        width: modelData.w * pageItem.s
                        height: modelData.h * pageItem.s
                        color: "transparent"
                        radius: 4
                        border.width: bh.hovered || (mp.activeBox && mp.activeBox.box === modelData) ? 2 : 0
                        border.color: Theme.purple
                        HoverHandler {
                            id: bh
                            onHoveredChanged: if (hovered) {
                                mp.activeBox = { page: pageItem.modelData, box: modelData, item: parent }
                                mp.boxTokens = Togu.tokenize(modelData.text)
                            }
                        }
                    }
                }
            }
        }
    }

    // The hovered box's text, tokenized, near the box.
    Rectangle {
        id: textPanel
        visible: !!mp.activeBox && mp.boxTokens.length > 0
        z: 10
        readonly property point at: mp.activeBox && mp.activeBox.item ? mp.activeBox.item.mapToItem(mp, 0, 0) : Qt.point(0, 0)
        width: Math.min(560, line.implicitWidth + 32)
        height: line.height + 24
        x: Math.max(8, Math.min(mp.width - width - 8, at.x + (mp.activeBox && mp.activeBox.item ? mp.activeBox.item.width / 2 : 0) - width / 2))
        y: at.y - height - 8 > bar.height ? at.y - height - 8 : Math.min(mp.height - height - 8, at.y + (mp.activeBox && mp.activeBox.item ? mp.activeBox.item.height : 0) + 8)
        radius: 16
        color: Theme.raised
        border.color: Theme.line
        HoverHandler { id: panelHover }
        TokenLine {
            id: line
            anchors.centerIn: parent
            tokens: mp.boxTokens
            fontSize: 24
            outline: false
            textColor: Theme.text
            maxWidth: 528
            activeIndex: popup.visible ? popup.tokenIndex : -1
            onWordEntered: (i, item) => mp.lookup(i, item)
            onWordClicked: (i, item) => mp.lookup(i, item)
        }
    }

    WordPopup {
        id: popup
        visible: false
        z: 20
        property int tokenIndex: -1
        property real anchorX: 0
        property real anchorTop: 0
        x: Math.max(8, Math.min(mp.width - width - 8, anchorX - width / 2))
        y: anchorTop - height - 6 > bar.height ? anchorTop - height - 6 : 8
        maxHeight: Math.min(520, mp.height * 0.65)
        onStatusChosen: (key, st) => Togu.setStatus(key, st)
        onCardRequested: entry => mp.card(entry)
        onQueryRequested: text => { entries = Togu.lookup(text); current = 0 }
        onCloseRequested: visible = false
    }
    function lookup(i, item) {
        const tok = boxTokens[i]
        if (!tok || !tok.w) return
        let text = ""
        for (let j = i; j < boxTokens.length; j++) text += boxTokens[j].s
        popup.tokenIndex = i
        popup.tokenKey = tok.k
        popup.tokenSurface = tok.s
        popup.entries = Togu.lookup(text)
        popup.kanjiList = Togu.kanji(popup.entries.length ? popup.entries[0].expression : tok.s)
        popup.libraryLines = []
        popup.current = 0
        const p = item.mapToItem(mp, 0, 0)
        popup.anchorX = p.x + item.width / 2
        popup.anchorTop = textPanel.y
        popup.visible = true
    }
    function card(entry) {
        if (!activeBox) return
        const b = activeBox.box
        const img = vol.crop(activeBox.page, b.x, b.y, b.w, b.h)
        mp.cardRequested(entry, { start: 0, end: 0, text: b.text, text2: "", book: vol.title + " · " + qsTr("p. %1").arg(activeBox.page + 1) }, popup.tokenSurface, img)
    }

    // Clicks on the page turn it: left half forward (right to left), right half back.
    MouseArea {
        anchors.fill: parent
        anchors.topMargin: bar.height
        z: -1
        acceptedButtons: Qt.LeftButton
        onClicked: mouse => { if (popup.visible) { popup.visible = false; return } mouse.x < width / 2 ? mp.next() : mp.prev() }
        onWheel: wheel => wheel.angleDelta.y < 0 ? mp.next() : mp.prev()
    }

    Rectangle {
        id: bar
        width: parent.width
        height: 56
        color: Theme.bg
        border.color: Theme.line
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 12
            spacing: 8
            IconButton { iconName: "back"; tip: qsTr("Back (Esc)"); onClicked: mp.closeRequested() }
            Text { text: vol.title; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.preferredWidth: Math.min(400, mp.width * 0.35) }
            Item { Layout.fillWidth: true }
            ComprehensionChip { value: mp.stats.comprehension !== undefined ? mp.stats.comprehension : -1 }
            Text { text: qsTr("%1 / %2").arg(mp.page + 1).arg(vol.pages.length); color: Theme.text2; font.pixelSize: 13 }
            TSlider {
                Layout.preferredWidth: 220
                from: vol.pages.length - 1; to: 0; stepSize: 1
                value: mp.page
                onMoved: mp.go(Math.round(value))
            }
            IconButton { iconName: "manga"; tip: mp.spread ? qsTr("One page") : qsTr("Two pages"); active: mp.spread; onClicked: mp.spread = !mp.spread }
        }
    }

    Keys.onPressed: event => {
        clock.poke()
        const target = popup.visible ? popup.targetKey : ""
        switch (event.key) {
        case Qt.Key_Left: case Qt.Key_Space: case Qt.Key_PageDown: case Qt.Key_A: mp.next(); break
        case Qt.Key_Right: case Qt.Key_PageUp: case Qt.Key_D: mp.prev(); break
        case Qt.Key_Home: mp.go(0); break
        case Qt.Key_End: mp.go(vol.pages.length - 1); break
        case Qt.Key_Escape: if (popup.visible) popup.visible = false; else mp.closeRequested(); break
        case Qt.Key_U: case Qt.Key_1: if (target) Togu.setStatus(target, "unknown"); break
        case Qt.Key_T: if (target) Togu.setStatus(target, Togu.status(target) === "tracked" ? "unknown" : "tracked"); break
        case Qt.Key_2: if (target) Togu.setStatus(target, "learning"); break
        case Qt.Key_K: case Qt.Key_3: if (target) Togu.setStatus(target, "known"); break
        case Qt.Key_I: case Qt.Key_4: if (target) Togu.setStatus(target, "ignored"); break
        case Qt.Key_Q: if (popup.visible) mp.card(popup.entry); break
        case Qt.Key_E: if (mp.activeBox) mp.card(null); break
        default: return
        }
        event.accepted = true
    }
}
