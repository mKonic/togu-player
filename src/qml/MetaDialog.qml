import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Togu

// Pick the right AniList/TMDB entry for a series by hand.
Popup {
    id: d
    property int seriesId: 0
    property var results: []
    property bool searching: false
    function openFor(id, title) {
        seriesId = id
        query.text = title
        results = []
        open()
        search()
    }
    function search() {
        searching = true
        Library.searchMeta(seriesId, query.text)
    }
    modal: true
    width: Math.min(720, parent ? parent.width - 80 : 720)
    height: Math.min(620, parent ? parent.height - 80 : 620)
    padding: 22
    background: Rectangle { radius: 24; color: Theme.raised; border.color: Theme.line }
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }

    Connections {
        target: Library
        function onMetaResults(id, list) { if (id === d.seriesId) { d.results = list; d.searching = false } }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 14
        Text { text: qsTr("Search online"); color: Theme.text; font.pixelSize: 20; font.weight: Font.Bold }
        RowLayout {
            spacing: 8
            TField { id: query; Layout.fillWidth: true; onAccepted: d.search() }
            TButton { iconName: "search"; text: qsTr("Search"); onClicked: d.search() }
        }
        Text {
            visible: !Settings.tmdbKey
            text: qsTr("Searching AniList. Add a TMDB key in Settings → Online to find live-action shows and films too.")
            color: Theme.text3; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true
        }
        BusyIndicator { visible: d.searching; running: visible; Layout.alignment: Qt.AlignHCenter }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: d.results
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                width: ListView.view.width
                height: 108
                radius: 14
                color: rh.hovered ? Theme.fill : "transparent"
                HoverHandler { id: rh; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: { Library.applyMeta(d.seriesId, modelData); d.close() } }
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 14
                    Rectangle {
                        Layout.preferredWidth: 62; Layout.preferredHeight: 92; radius: 8; color: Theme.fill; clip: true
                        Image { anchors.fill: parent; source: modelData.poster; fillMode: Image.PreserveAspectCrop; asynchronous: true }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text { text: modelData.title + (modelData.year ? "  (" + modelData.year + ")" : ""); color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                        Text { text: [modelData.titleNative, modelData.titleEnglish].filter(x => x).join(" · "); color: Theme.text2; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
                        Text { text: modelData.description; color: Theme.text3; font.pixelSize: 12; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight; Layout.fillWidth: true }
                        Chip { text: modelData.source === "anilist" ? "AniList" : "TMDB"; tone: modelData.source === "anilist" ? "blue" : "teal"; fontSize: 10 }
                    }
                }
            }
        }
        Text { visible: !d.searching && d.results.length === 0; text: qsTr("No results."); color: Theme.text2; font.pixelSize: 13 }
    }
}
