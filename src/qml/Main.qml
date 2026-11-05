import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import Togu

ApplicationWindow {
    id: win
    width: 1440
    height: 900
    minimumWidth: 900
    minimumHeight: 560
    visible: true
    title: player.visible && player.session.title ? player.session.title + " — Togu" : "Togu"
    color: Theme.bg
    font.family: Theme.sans

    property string page: "watch"
    property bool wasMaximized: false

    function playFile(path, itemId) {
        player.visible = true
        player.open(path, itemId)
    }
    function openBook(path) {
        readerLoader.active = true
        readerLoader.item.visible = true
        readerLoader.item.open(path)
    }
    function openManga(path) {
        mangaLoader.active = true
        mangaLoader.item.visible = true
        mangaLoader.item.open(path)
    }
    function closePlayer() {
        player.session.close()
        player.mpv.stop()
        player.visible = false
        if (win.visibility === Window.FullScreen) win.showNormal()
        player.fullscreen = false
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient { GradientStop { position: 0; color: Theme.bgApp } GradientStop { position: 1; color: Theme.bgApp2 } }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        visible: !player.visible && !(readerLoader.item && readerLoader.item.visible) && !(mangaLoader.item && mangaLoader.item.visible)
        NavRail {
            Layout.fillHeight: true
            current: win.page
            onPicked: p => win.page = p
        }
        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: ({ watch: libraryPage, listen: libraryPage, read: readPage, manga: mangaPage,
                                words: wordsPage, stats: statsPage, queue: queuePage, settings: settingsPage })[win.page]
        }
    }

    Component { id: libraryPage; LibraryPage { kind: win.page === "listen" ? "audio" : "video"; onPlay: (path, id) => win.playFile(path, id) } }
    Component { id: readPage; ShelfPage { kind: "book"; onOpenItem: (path, id) => win.openBook(path) } }
    Component { id: mangaPage; ShelfPage { kind: "manga"; onOpenItem: (path, id) => win.openManga(path) } }
    Component { id: wordsPage; WordsPage {} }
    Component { id: statsPage; StatsPage {} }
    Component { id: queuePage; QueuePage {} }
    Component { id: settingsPage; SettingsPage {} }

    PlayerPage {
        id: player
        anchors.fill: parent
        visible: false
        onCloseRequested: win.closePlayer()
        onFullscreenToggled: {
            if (win.visibility === Window.FullScreen) { win.showNormal(); player.fullscreen = false }
            else { win.showFullScreen(); player.fullscreen = true }
        }
        onCardRequested: (entry, line, surface) => cardCreator.openFor(entry, line, surface, player.session, player.mpv)
    }

    // The reader loads its web engine only when a book is opened.
    Loader {
        id: readerLoader
        anchors.fill: parent
        active: false
        sourceComponent: ReaderPage {
            visible: false
            onCloseRequested: { visible = false; readerLoader.active = false }
            onCardRequested: (entry, line, surface) => cardCreator.openFor(entry, line, surface, null, null)
        }
    }
    Loader {
        id: mangaLoader
        anchors.fill: parent
        active: false
        sourceComponent: MangaPage {
            visible: false
            onCloseRequested: { visible = false; mangaLoader.active = false }
            onCardRequested: (entry, line, surface, image) => cardCreator.openFor(entry, line, surface, null, null, image)
        }
    }

    CardCreator { id: cardCreator; anchors.centerIn: parent }

    // A queued line: play it, then open the Card Creator on it.
    Connections {
        target: Queue
        function onOpenRequested(q) {
            win.playFile(q.path, q.item)
            player.session.openAt(q.path, q.item, q.start)
            queueOpen.q = q
            queueOpen.restart()
        }
    }
    Timer {
        id: queueOpen
        property var q
        interval: 900
        onTriggered: {
            const entries = q.word ? Togu.lookup(q.word.split("\u001f")[0]) : []
            cardCreator.openFor(entries.length ? entries[0] : null, { start: q.start, end: q.end, text: q.text, text2: q.text2 }, "", player.session, player.mpv)
            Queue.remove(q.id)
        }
    }

    Toasts { id: toasts; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 40; z: 100 }
    Connections {
        target: Togu
        function onToast(text, tone) { toasts.show(text, tone) }
        function onDictImported(title, error) { toasts.show(error ? title + ": " + error : qsTr("%1 imported").arg(title), error ? "pink" : "teal") }
    }

    DropArea {
        anchors.fill: parent
        onDropped: drop => {
            if (!drop.hasUrls) return
            const p = Togu.localPath(drop.urls[0])
            if (/\.(ass|ssa|srt|vtt|lrc|rar|7z|zip)$/i.test(p) && player.visible) player.session.importSubtitle(drop.urls[0], false)
            else if (/\.zip$/i.test(p)) Togu.importDicts(drop.urls)
            else if (/\.(epub|txt)$/i.test(p)) win.openBook(p)
            else if (/\.mokuro$/i.test(p)) win.openManga(p)
            else win.playFile(p, 0)
        }
    }

    Component.onCompleted: {
        if (startupFiles.length) playFile(startupFiles[0], 0)
    }
}
