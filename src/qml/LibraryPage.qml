import QtQuick
import QtCore
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import Togu

// A media library (video or audio): continue row, series grid, series page.
Item {
    id: lib
    property string kind: "video"
    property int openSeries: 0
    property string sort: "recent"
    property string filter: ""
    signal play(string path, int itemId)

    readonly property var rootList: { const dep = Library.revision; return dep === undefined ? null : Library.roots(kind) }
    readonly property var seriesList: { const dep = Library.revision; return dep === undefined ? null : Library.series(kind, sort, filter) }
    readonly property var continueList: { const dep = Library.revision; return dep === undefined ? null : Library.continueItems(kind) }

    FolderDialog {
        id: folderDialog
        title: lib.kind === "audio" ? qsTr("Add an audio folder") : qsTr("Add a video folder")
        currentFolder: StandardPaths.writableLocation(lib.kind === "audio" ? StandardPaths.MusicLocation : StandardPaths.MoviesLocation)
        onAccepted: Library.addRoot(selectedFolder, lib.kind)
    }
    FileDialog {
        id: fileDialog
        title: qsTr("Open a file")
        currentFolder: StandardPaths.writableLocation(lib.kind === "audio" ? StandardPaths.MusicLocation : StandardPaths.MoviesLocation)
        nameFilters: lib.kind === "audio" ? [qsTr("Audio (*.mp3 *.m4a *.m4b *.flac *.ogg *.opus *.wav *.aac)")] : [qsTr("Video (*.mkv *.mp4 *.webm *.avi *.mov *.m4v *.ts)")]
        onAccepted: lib.play(Togu.localPath(selectedFile), 0)
    }

    // ---------------------------------------------------------------- grid view
    Flickable {
        anchors.fill: parent
        visible: lib.openSeries === 0
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
                Text {
                    text: lib.kind === "audio" ? qsTr("Listen") : qsTr("Watch")
                    color: Theme.text; font.pixelSize: 28; font.weight: Font.Bold; font.family: Theme.round
                }
                Item { Layout.fillWidth: true }
                Row {
                    visible: Library.busy
                    spacing: 8
                    BusyIndicator { running: Library.busy; implicitWidth: 20; implicitHeight: 20; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: Library.status; color: Theme.text2; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
                }
                TField {
                    Layout.preferredWidth: 240
                    placeholderText: qsTr("Search")
                    visible: lib.rootList.length > 0
                    onTextChanged: lib.filter = text
                }
                TCombo {
                    Layout.preferredWidth: 200
                    visible: lib.rootList.length > 0
                    model: [{ text: qsTr("Recently watched"), value: "recent" }, { text: qsTr("Title"), value: "title" },
                            { text: qsTr("Comprehension"), value: "comprehension" }, { text: qsTr("Recently added"), value: "added" }]
                    onActivated: lib.sort = currentValue
                }
                IconButton { iconName: "folder"; tip: qsTr("Open a file"); onClicked: fileDialog.open() }
                IconButton {
                    id: foldersBtn
                    iconName: "more"
                    tip: qsTr("Folders")
                    onClicked: foldersMenu.popup(foldersBtn, 0, foldersBtn.height)
                    TMenu {
                        id: foldersMenu
                        MenuItem { text: qsTr("Add folder…"); onTriggered: folderDialog.open() }
                        MenuItem { text: qsTr("Scan again"); onTriggered: Library.rescan() }
                        MenuSeparator {}
                        Repeater {
                            model: lib.rootList
                            MenuItem { text: qsTr("Remove %1").arg(modelData.path); onTriggered: Library.removeRoot(modelData.id) }
                        }
                    }
                }
            }

            // First run
            Section {
                visible: lib.rootList.length === 0
                title: lib.kind === "audio" ? qsTr("Add your audio") : qsTr("Add your shows")
                subtitle: lib.kind === "audio"
                    ? qsTr("Point Togu at a folder of podcasts, drama CDs or audiobooks. Files with an .srt, .lrc or .ass next to them get the full subtitle view.")
                    : qsTr("Point Togu at a folder with your anime, dramas or films. Each subfolder becomes a series; subtitles next to the videos or inside MKV files are found on their own.")
                RowLayout {
                    spacing: 10
                    TButton { iconName: "plus"; text: qsTr("Add folder…"); onClicked: folderDialog.open() }
                    TButton { quiet: true; tone: "plain"; iconName: "folder"; text: qsTr("Open a single file…"); onClicked: fileDialog.open() }
                }
            }
            StarterDicts { id: starter; visible: (Togu.dicts.length === 0 && !Togu.dictBusy) || starter.running }

            // Continue
            ColumnLayout {
                visible: lib.continueList.length > 0
                Layout.fillWidth: true
                spacing: 10
                Text { text: lib.kind === "audio" ? qsTr("Continue listening") : qsTr("Continue watching"); color: Theme.text; font.pixelSize: 18; font.weight: Font.Bold }
                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 190
                    orientation: ListView.Horizontal
                    spacing: 14
                    clip: true
                    model: lib.continueList
                    delegate: ContinueCard {
                        item: modelData
                        onClicked: lib.play(modelData.path, modelData.id)
                    }
                }
            }

            // Series grid
            Text {
                visible: lib.seriesList.length > 0
                text: lib.kind === "audio" ? qsTr("Library") : qsTr("Series")
                color: Theme.text; font.pixelSize: 18; font.weight: Font.Bold
            }
            Flow {
                Layout.fillWidth: true
                spacing: 18
                Repeater {
                    model: lib.seriesList
                    delegate: PosterCard {
                        series: modelData
                        square: lib.kind === "audio"
                        onClicked: lib.openSeries = modelData.id
                    }
                }
            }
            Text {
                visible: lib.rootList.length > 0 && lib.seriesList.length === 0 && !Library.busy
                text: lib.filter ? qsTr("Nothing matches \"%1\".").arg(lib.filter) : qsTr("No media found in these folders yet.")
                color: Theme.text2; font.pixelSize: 14
            }
        }
    }

    SeriesPage {
        anchors.fill: parent
        visible: lib.openSeries !== 0
        seriesId: lib.openSeries
        onBack: lib.openSeries = 0
        onPlay: (path, id) => lib.play(path, id)
    }
}
