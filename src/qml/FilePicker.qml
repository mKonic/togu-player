import QtQuick
import QtQuick.Dialogs
import Togu

// File or folder picker that opens the desktop's own chooser (xdg portal),
// with Qt's built-in dialog only when no portal answers. Same API as
// FileDialog/FolderDialog for what Togu uses.
Item {
    id: fp
    visible: false

    property string title
    property bool folder: false
    property int fileMode: FileDialog.OpenFile
    property var nameFilters: []
    property url currentFolder
    property string defaultSuffix: ""
    property string currentName: ""
    property url selectedFile
    property url selectedFolder
    property var selectedFiles: []
    signal accepted

    property int request: -1

    function open() {
        const save = fileMode === FileDialog.SaveFile
        request = Portal.open({
            title: title,
            folder: folder,
            multiple: fileMode === FileDialog.OpenFiles,
            save: save,
            currentFolder: currentFolder.toString(),
            currentName: currentName || (save && defaultSuffix ? qsTr("untitled") + "." + defaultSuffix : ""),
            filters: folder ? [] : nameFilters
        })
        if (request < 0) fallbackOpen()
    }

    function finish(urls) {
        let list = urls.map(u => String(u))
        // A saved name typed without an extension gets the default one.
        if (fileMode === FileDialog.SaveFile && defaultSuffix && list.length) {
            const last = list[0].substring(list[0].lastIndexOf("/") + 1)
            if (last.indexOf(".") < 0) list[0] += "." + defaultSuffix
        }
        selectedFiles = list
        selectedFile = list[0]
        selectedFolder = list[0]
        accepted()
    }

    function fallbackOpen() {
        fallback.active = false
        fallback.active = true
    }

    Connections {
        target: Portal
        function onFinished(id, urls) {
            if (id !== fp.request) return
            fp.request = -1
            if (urls.length) fp.finish(urls)
        }
        function onFailed(id) {
            if (id !== fp.request) return
            fp.request = -1
            fp.fallbackOpen()
        }
    }

    Loader {
        id: fallback
        active: false
        sourceComponent: fp.folder ? folderDialog : fileDialog
        onLoaded: item.open()
    }
    Component {
        id: fileDialog
        FileDialog {
            title: fp.title
            fileMode: fp.fileMode
            nameFilters: fp.nameFilters
            currentFolder: fp.currentFolder
            defaultSuffix: fp.defaultSuffix
            onAccepted: fp.finish(fileMode === FileDialog.OpenFiles ? selectedFiles : [selectedFile])
        }
    }
    Component {
        id: folderDialog
        FolderDialog {
            title: fp.title
            currentFolder: fp.currentFolder
            onAccepted: fp.finish([selectedFolder])
        }
    }
}
