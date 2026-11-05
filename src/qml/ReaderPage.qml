import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtWebEngine
import Togu

// The book reader: the text in a web view (for vertical writing and ruby), Togu's popup on top.
FocusScope {
    id: rp
    property alias reader: reader
    signal closeRequested()
    signal cardRequested(var entry, var line, string surface)
    property bool ready: false
    property int hoverP: -1
    property int hoverT: -1

    function open(path) { ready = false; reader.open(path); forceActiveFocus() }
    function js(code) { view.runJavaScript(code) }
    function reload() { ready = false; view.loadHtml(reader.pageShell(), "file:///") }

    Reader {
        id: reader
        onOpened: rp.reload()
        onChapterReady: body => {
            rp.pendingBody = body
            if (rp.ready) rp.pushBody()
        }
        onStatusUpdates: map => rp.js("togu.status(" + JSON.stringify(map) + ")")
        onFailed: msg => Togu.toast(msg, "pink")
    }
    property string pendingBody: ""
    function pushBody() {
        js("togu.setBody(" + JSON.stringify(pendingBody) + "," + reader.restoreFraction + ")")
        closePopup()
    }
    // Restyle when the look changes.
    Connections {
        target: Settings
        function onValueChanged(key) {
            if (["readerVertical", "readerSize", "readerFont", "readerTheme", "furigana", "pitchColors", "wordGloss", "statusColors"].indexOf(key) >= 0 && reader.path) {
                rp.reload()
            }
        }
    }

    Rectangle { anchors.fill: parent; color: Settings.readerTheme === "light" ? "#fbf8f1" : "#100536" }

    WebEngineView {
        id: view
        anchors.top: bar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        backgroundColor: Settings.readerTheme === "light" ? "#fbf8f1" : "#100536"
        settings.javascriptCanAccessClipboard: false
        settings.localContentCanAccessFileUrls: true
        settings.showScrollBars: true
        onLoadingChanged: info => {
            if (info.status === WebEngineView.LoadSucceededStatus) {
                rp.ready = true
                if (rp.pendingBody) rp.pushBody()
            }
        }
        onJavaScriptConsoleMessage: (level, message) => {
            if (!message.startsWith("togu:")) return
            const e = JSON.parse(message.slice(5))
            if (e.type === "hover" && Settings.lookupMode === "hover") rp.lookup(e)
            else if (e.type === "click") rp.lookup(e)
            else if (e.type === "blank") rp.closePopup()
            else if (e.type === "leave" && !popup.hovered) leaveTimer.restart()
            else if (e.type === "scroll") { if (popup.visible && !popup.hovered) rp.closePopup() }
            else if (e.type === "pos") reader.savePosition(e.f)
            else if (e.type === "edge") {
                if (e.dir > 0 && reader.chapter < reader.chapterTitles.length - 1) reader.showChapter(reader.chapter + 1, 0)
                else if (e.dir < 0 && reader.chapter > 0) reader.showChapter(reader.chapter - 1, 1)
            }
            else if (e.type === "key") rp.key(e)
        }
        onNavigationRequested: request => {
            // Links in books open outside; the page itself is only ever ours.
            if (request.navigationType === WebEngineNavigationRequest.LinkClickedNavigation) {
                request.reject()
                Togu.openExternally(request.url)
            }
        }
    }
    Timer { id: leaveTimer; interval: 300; onTriggered: if (!popup.hovered) rp.closePopup() }

    // ---------------------------------------------------------- toolbar
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
            IconButton { iconName: "back"; tip: qsTr("Back (Esc)"); onClicked: rp.closeRequested() }
            Column {
                Layout.preferredWidth: Math.min(360, rp.width * 0.3)
                Text { width: parent.width; text: reader.title; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold; elide: Text.ElideRight }
                Text { width: parent.width; text: reader.author; color: Theme.text3; font.pixelSize: 12; elide: Text.ElideRight; visible: text !== "" }
            }
            TCombo {
                Layout.preferredWidth: 260
                model: reader.chapterTitles.map((t, i) => ({ text: (i + 1) + ". " + t, value: i }))
                currentIndex: reader.chapter
                onActivated: reader.showChapter(currentValue, 0)
            }
            BusyIndicator { running: reader.loading; visible: running; implicitWidth: 22; implicitHeight: 22 }
            Item { Layout.fillWidth: true }
            ComprehensionChip { value: reader.stats.comprehension !== undefined ? reader.stats.comprehension : -1 }
            IconButton {
                iconName: "layers"
                tip: Settings.readerVertical ? qsTr("Horizontal text") : qsTr("Vertical text")
                active: Settings.readerVertical
                onClicked: Settings.set("readerVertical", !Settings.readerVertical)
            }
            IconButton { iconName: "minus"; tip: qsTr("Smaller"); onClicked: Settings.set("readerSize", Math.max(14, Settings.readerSize - 2)) }
            IconButton { iconName: "plus"; tip: qsTr("Larger"); onClicked: Settings.set("readerSize", Math.min(48, Settings.readerSize + 2)) }
            IconButton {
                iconName: "eye"
                tip: Settings.readerTheme === "light" ? qsTr("Dark page") : qsTr("Light page")
                onClicked: Settings.set("readerTheme", Settings.readerTheme === "light" ? "dark" : "light")
            }
        }
    }

    // ---------------------------------------------------------- popup
    WordPopup {
        id: popup
        visible: false
        z: 20
        property real anchorX: 0
        property real anchorTop: 0
        property real anchorBottom: 0
        maxHeight: Math.min(560, rp.height * 0.7)
        x: Math.max(8, Math.min(rp.width - width - 8, anchorX - width / 2))
        y: anchorTop - height - 6 >= view.y ? anchorTop - height - 6 : Math.min(rp.height - height - 8, anchorBottom + 6)
        onStatusChosen: (key, st) => Togu.setStatus(key, st)
        onCardRequested: entry => rp.card(entry)
        onQueryRequested: text => { entries = Togu.lookup(text); current = 0 }
        onCloseRequested: rp.closePopup()
        onLineRequested: hit => Togu.toast(qsTr("That line is in %1").arg(hit.seriesTitle), "blue")
        onHoveredChanged: if (!hovered) leaveTimer.restart()
    }
    function lookup(e) {
        const tok = reader.token(e.p, e.t)
        if (!tok.w || (popup.visible && hoverP === e.p && hoverT === e.t)) return
        hoverP = e.p; hoverT = e.t
        popup.tokenKey = tok.k
        popup.tokenSurface = tok.s
        popup.entries = Togu.lookup(reader.textFrom(e.p, e.t))
        popup.kanjiList = Togu.kanji(popup.entries.length ? popup.entries[0].expression : tok.s)
        popup.libraryLines = Library.linesWithWord(popup.entries.length ? popup.entries[0].key : tok.k, 6)
        popup.current = 0
        popup.anchorX = view.x + e.x + e.w / 2
        popup.anchorTop = view.y + e.y
        popup.anchorBottom = view.y + e.y + e.h
        popup.visible = true
        js("togu.active(" + e.p + "," + e.t + ")")
    }
    function closePopup() {
        popup.visible = false
        hoverP = -1; hoverT = -1
        js("togu.active(-1,-1)")
    }
    function card(entry) {
        const sentence = hoverP >= 0 ? reader.sentenceAt(hoverP, hoverT) : ""
        rp.cardRequested(entry, { start: 0, end: 0, text: sentence, text2: "", book: reader.title + " · " + (reader.chapterTitles[reader.chapter] || "") }, popup.tokenSurface)
    }
    function key(e) {
        const target = popup.visible ? popup.targetKey : ""
        switch (e.key) {
        case "ArrowRight": case "PageDown": case " ": case "d": case "D":
            js("togu.page(" + (Settings.readerVertical && e.key === "ArrowRight" ? -1 : 1) + ")"); break
        case "ArrowLeft": case "PageUp": case "a": case "A":
            js("togu.page(" + (Settings.readerVertical && e.key === "ArrowLeft" ? 1 : -1) + ")"); break
        case "ArrowDown": js("togu.page(1)"); break
        case "ArrowUp": js("togu.page(-1)"); break
        case "Escape": if (popup.visible) closePopup(); else rp.closeRequested(); break
        case "u": case "1": if (target) Togu.setStatus(target, "unknown"); break
        case "t": if (target) Togu.setStatus(target, Togu.status(target) === "tracked" ? "unknown" : "tracked"); break
        case "2": if (target) Togu.setStatus(target, "learning"); break
        case "k": case "3": if (target) Togu.setStatus(target, "known"); break
        case "i": case "4": if (target) Togu.setStatus(target, "ignored"); break
        case "q": if (popup.visible) card(popup.entry); break
        }
    }
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Escape) { if (popup.visible) closePopup(); else rp.closeRequested(); event.accepted = true }
    }
}
