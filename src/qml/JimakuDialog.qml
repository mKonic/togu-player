import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Togu

// Find Japanese subtitles on jimaku.cc for the playing episode.
Popup {
    id: d
    property var session: null
    property var entries: []
    property var files: []
    property int entryId: 0
    property double episode: -1
    property string message: ""
    function openFor(s) {
        session = s
        const g = Jimaku.guess(s.itemId)
        query.text = g.query || ""
        episode = g.episode !== undefined ? g.episode : -1
        entries = []; files = []; entryId = 0; message = ""
        open()
        Jimaku.search(query.text, g.anilistId || 0)
    }
    modal: true
    width: Math.min(680, parent ? parent.width - 80 : 680)
    height: Math.min(600, parent ? parent.height - 80 : 600)
    padding: 22
    background: Rectangle { radius: 24; color: Theme.raised; border.color: Theme.line }
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }

    Connections {
        target: Jimaku
        function onSearchResults(list) { d.entries = list; d.message = list.length ? "" : qsTr("Nothing on Jimaku for that title.") }
        function onFileResults(list) { d.files = list; d.message = list.length ? "" : qsTr("No files for this episode. Clear the episode number to see them all.") }
        function onFailed(msg) { d.message = msg }
        function onDownloaded(path) {
            if (d.session) d.session.importSubtitle(Togu.fileUrl(path), false)
            d.close()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        RowLayout {
            Text { text: qsTr("Jimaku"); color: Theme.text; font.pixelSize: 20; font.weight: Font.Bold; Layout.fillWidth: true }
            BusyIndicator { running: Jimaku.busy; visible: running; implicitWidth: 24; implicitHeight: 24 }
            IconButton { iconName: "close"; size: 32; onClicked: d.close() }
        }
        RowLayout {
            spacing: 8
            TField { id: query; Layout.fillWidth: true; placeholderText: qsTr("Series title"); onAccepted: { d.entryId = 0; Jimaku.search(text, 0) } }
            TField {
                Layout.preferredWidth: 90
                placeholderText: qsTr("Episode")
                text: d.episode >= 0 ? d.episode : ""
                onEditingFinished: { d.episode = text === "" ? -1 : Number(text); if (d.entryId) Jimaku.files(d.entryId, d.episode) }
            }
            TButton { iconName: "search"; text: qsTr("Search"); onClicked: { d.entryId = 0; Jimaku.search(query.text, 0) } }
        }
        Text { visible: d.message !== ""; text: d.message; color: Theme.orange; font.pixelSize: 13; wrapMode: Text.Wrap; Layout.fillWidth: true }
        Text {
            visible: d.entryId !== 0
            text: "← " + qsTr("Back to results")
            color: Theme.blue; font.pixelSize: 13
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { d.entryId = 0; d.files = [] } }
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: d.entryId ? d.files : d.entries
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                width: ListView.view.width
                height: 54
                radius: 12
                color: rh.hovered ? Theme.fill : "transparent"
                HoverHandler { id: rh; cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    onTapped: {
                        if (d.entryId) Jimaku.download(modelData.url, modelData.name, d.entryId, d.episode)
                        else { d.entryId = modelData.id; Jimaku.files(modelData.id, d.episode) }
                    }
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10
                    Icon { name: d.entryId ? (modelData.archive ? "layers" : "subtitles") : "film"; size: 18; color: Theme.text2 }
                    Column {
                        Layout.fillWidth: true
                        Text { width: parent.width; text: modelData.name; color: Theme.text; font.pixelSize: 14; elide: Text.ElideMiddle }
                        Text {
                            width: parent.width
                            text: d.entryId ? (modelData.size / 1024).toFixed(0) + " KB" + (modelData.archive ? " · " + qsTr("archive") : "")
                                            : [modelData.japanese, modelData.english].filter(x => x).join(" · ")
                            color: Theme.text3; font.pixelSize: 12; elide: Text.ElideRight
                        }
                    }
                    Icon { name: d.entryId ? "download" : "forward"; size: 16; color: Theme.text3 }
                }
            }
        }
    }
}
