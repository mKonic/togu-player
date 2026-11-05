import QtQuick
import QtCore
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import Togu

// Books or manga volumes: continue reading, then everything by folder.
Item {
    id: shelf
    property string kind: "book"
    property string filter: ""
    signal openItem(string path, int itemId)
    readonly property var rootList: { const dep = Library.revision; return dep === undefined ? null : Library.roots(kind) }
    readonly property var items: { const dep = Library.revision; return dep === undefined ? null : Library.shelf(kind, filter) }
    readonly property var recent: items.filter(i => i.lastRead && i.progress < 0.995).sort((a, b) => b.lastRead - a.lastRead).slice(0, 8)
    readonly property var groups: {
        const out = []
        for (const it of items) {
            if (!out.length || out[out.length - 1].series !== it.series) out.push({ series: it.series, title: it.seriesTitle, items: [] })
            out[out.length - 1].items.push(it)
        }
        return out
    }

    FolderDialog {
        id: folderDialog
        title: shelf.kind === "manga" ? qsTr("Add a manga folder") : qsTr("Add a book folder")
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        onAccepted: Library.addRoot(selectedFolder, shelf.kind)
    }
    FileDialog {
        id: fileDialog
        title: qsTr("Open a file")
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        nameFilters: shelf.kind === "manga" ? [qsTr("mokuro volumes (*.mokuro)")] : [qsTr("Books (*.epub *.txt)")]
        onAccepted: shelf.openItem(Togu.localPath(selectedFile), 0)
    }

    Flickable {
        anchors.fill: parent
        contentHeight: col.implicitHeight + 40
        clip: true
        ScrollBar.vertical: ScrollBar {}
        ColumnLayout {
            id: col
            x: 28
            y: 24
            width: parent.width - 56
            spacing: 22
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Text { text: shelf.kind === "manga" ? qsTr("Manga") : qsTr("Read"); color: Theme.text; font.pixelSize: 28; font.weight: Font.Bold; font.family: Theme.round }
                Item { Layout.fillWidth: true }
                Row {
                    visible: Library.busy
                    spacing: 8
                    BusyIndicator { running: Library.busy; implicitWidth: 20; implicitHeight: 20 }
                    Text { text: Library.status; color: Theme.text2; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
                }
                TField { Layout.preferredWidth: 240; placeholderText: qsTr("Search"); visible: shelf.rootList.length > 0; onTextChanged: shelf.filter = text }
                IconButton { iconName: "folder"; tip: qsTr("Open a file"); onClicked: fileDialog.open() }
                IconButton {
                    id: moreBtn
                    iconName: "more"
                    tip: qsTr("Folders")
                    onClicked: moreMenu.popup(moreBtn, 0, moreBtn.height)
                    TMenu {
                        id: moreMenu
                        MenuItem { text: qsTr("Add folder…"); onTriggered: folderDialog.open() }
                        MenuItem { text: qsTr("Scan again"); onTriggered: Library.rescan() }
                        MenuSeparator {}
                        Repeater { model: shelf.rootList; MenuItem { text: qsTr("Remove %1").arg(modelData.path); onTriggered: Library.removeRoot(modelData.id) } }
                    }
                }
            }
            Section {
                visible: shelf.rootList.length === 0
                title: shelf.kind === "manga" ? qsTr("Add your manga") : qsTr("Add your books")
                subtitle: shelf.kind === "manga"
                    ? qsTr("Point Togu at a folder of volumes processed with mokuro (a .mokuro file next to each volume's image folder). Hover a speech bubble to read and look up its text.")
                    : qsTr("Point Togu at a folder of EPUB or text files. Every word gets its status colour, furigana and the lookup popup, in horizontal or vertical text.")
                RowLayout {
                    spacing: 10
                    TButton { iconName: "plus"; text: qsTr("Add folder…"); onClicked: folderDialog.open() }
                    TButton { quiet: true; tone: "plain"; iconName: "folder"; text: qsTr("Open a single file…"); onClicked: fileDialog.open() }
                }
            }
            ColumnLayout {
                visible: shelf.recent.length > 0
                spacing: 10
                Text { text: qsTr("Continue reading"); color: Theme.text; font.pixelSize: 18; font.weight: Font.Bold }
                Flow {
                    Layout.fillWidth: true
                    spacing: 18
                    Repeater { model: shelf.recent; delegate: BookCover { item: modelData; onClicked: shelf.openItem(modelData.path, modelData.id) } }
                }
            }
            Repeater {
                model: shelf.groups
                delegate: ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Text { text: modelData.title; color: Theme.text; font.pixelSize: 18; font.weight: Font.Bold }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 18
                        Repeater { model: modelData.items; delegate: BookCover { item: modelData; onClicked: shelf.openItem(modelData.path, modelData.id) } }
                    }
                }
            }
        }
    }
}
