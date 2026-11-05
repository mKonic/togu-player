import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import Togu

// One series: banner, description, comprehension, episodes.
Item {
    id: sp
    property int seriesId: 0
    signal back()
    signal play(string path, int itemId)
    readonly property var info: { Library.revision; return seriesId ? Library.seriesInfo(seriesId) : ({}) }
    readonly property var eps: { Library.revision; return seriesId ? Library.items(seriesId) : [] }
    readonly property var nextUp: {
        for (let i = 0; i < eps.length; i++) if (!eps[i].watched) return eps[i]
        return eps.length ? eps[0] : null
    }

    Flickable {
        anchors.fill: parent
        contentHeight: col.implicitHeight + 40
        clip: true
        ScrollBar.vertical: ScrollBar {}
        Column {
            id: col
            width: parent.width
            spacing: 0
            // Banner
            Item {
                width: parent.width
                height: 300
                Image {
                    anchors.fill: parent
                    source: sp.info.banner || sp.info.poster || ""
                    fillMode: Image.PreserveAspectCrop
                    opacity: 0.55
                    asynchronous: true
                }
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient { GradientStop { position: 0; color: "transparent" } GradientStop { position: 1; color: Theme.bgApp } }
                }
                IconButton { x: 20; y: 18; iconName: "back"; tip: qsTr("Back"); onClicked: sp.back() }
                RowLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 28
                    spacing: 24
                    Rectangle {
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: sp.info.kind === "audio" ? 150 : 222
                        radius: 14
                        color: Theme.raised
                        clip: true
                        Image { anchors.fill: parent; source: sp.info.poster || ""; fillMode: Image.PreserveAspectCrop; asynchronous: true }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Text { text: sp.info.title || ""; color: Theme.text; font.pixelSize: 30; font.weight: Font.Bold; font.family: Theme.round; Layout.fillWidth: true; elide: Text.ElideRight }
                        Row {
                            spacing: 10
                            ComprehensionChip { value: sp.info.comprehension !== undefined ? sp.info.comprehension : -1; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: (sp.info.year ? sp.info.year + " · " : "") + qsTr("%1 of %2 watched").arg(sp.info.watched).arg(sp.info.count); color: Theme.text2; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
                        }
                        Text {
                            visible: !!sp.info.description
                            text: sp.info.description || ""
                            color: Theme.text2; font.pixelSize: 13; wrapMode: Text.Wrap; maximumLineCount: 3; elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        RowLayout {
                            spacing: 8
                            TButton {
                                visible: !!sp.nextUp
                                iconName: "play"
                                text: sp.nextUp ? (sp.nextUp.position > 5 && !sp.nextUp.watched ? qsTr("Resume") : qsTr("Play")) + (sp.nextUp.episode >= 0 ? " " + qsTr("episode %1").arg(sp.nextUp.episode) : "") : ""
                                onClicked: sp.play(sp.nextUp.path, sp.nextUp.id)
                            }
                            TButton { quiet: true; tone: "plain"; iconName: "search"; text: qsTr("Match title…"); onClicked: metaDialog.openFor(sp.seriesId, sp.info.folderTitle) }
                            IconButton {
                                id: moreBtn
                                iconName: "more"
                                onClicked: moreMenu.popup(moreBtn, 0, moreBtn.height)
                                TMenu {
                                    id: moreMenu
                                    MenuItem { text: qsTr("Mark all watched"); onTriggered: Library.setSeriesWatched(sp.seriesId, true) }
                                    MenuItem { text: qsTr("Mark all unwatched"); onTriggered: Library.setSeriesWatched(sp.seriesId, false) }
                                    MenuItem { text: qsTr("Export condensed audio…"); onTriggered: condenseDir.open() }
                                    MenuItem { text: qsTr("Remove online info"); onTriggered: Library.clearMeta(sp.seriesId) }
                                    MenuItem { text: qsTr("Look up online again"); onTriggered: Library.refreshMeta(sp.seriesId) }
                                }
                            }
                        }
                    }
                }
            }
            // Episodes
            Column {
                x: 28
                width: parent.width - 56
                spacing: 6
                topPadding: 10
                Repeater {
                    model: sp.eps
                    delegate: Rectangle {
                        id: row
                        width: parent.width
                        height: 86
                        radius: 14
                        color: rowHover.hovered ? Theme.fill : "transparent"
                        HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: sp.play(modelData.path, modelData.id) }
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 16
                            Rectangle {
                                Layout.preferredWidth: 124
                                Layout.preferredHeight: 70
                                radius: 10
                                color: Theme.raised
                                clip: true
                                Image {
                                    anchors.fill: parent
                                    visible: sp.info.kind !== "audio"
                                    source: sp.info.kind !== "audio" ? "image://thumb/" + modelData.id : ""
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    sourceSize.width: 248
                                }
                                Icon { anchors.centerIn: parent; visible: sp.info.kind === "audio"; name: "audio"; size: 26; color: Theme.text3 }
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    visible: modelData.position > 0 && modelData.duration > 0
                                    width: parent.width * modelData.position / Math.max(1, modelData.duration)
                                    height: 3
                                    color: Theme.purple
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                Text {
                                    text: modelData.special ? qsTr("Special") : modelData.episode >= 0 ? (modelData.season > 0 ? qsTr("S%1 · Episode %2").arg(modelData.season).arg(modelData.episode) : qsTr("Episode %1").arg(modelData.episode)) : modelData.name
                                    color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true
                                }
                                Text { text: modelData.name; color: Theme.text3; font.pixelSize: 12; elide: Text.ElideMiddle; Layout.fillWidth: true; visible: modelData.episode >= 0 || modelData.special }
                                Row {
                                    spacing: 8
                                    ComprehensionChip { value: modelData.comprehension }
                                    Text {
                                        visible: modelData.comprehension < 0
                                        text: modelData.hasSubs ? qsTr("Reading subtitles…") : qsTr("No Japanese subtitles found")
                                        color: Theme.text3; font.pixelSize: 12
                                    }
                                    Text { visible: modelData.duration > 0; text: Togu.formatTime(modelData.duration); color: Theme.text3; font.pixelSize: 12 }
                                }
                            }
                            IconButton {
                                iconName: "check"
                                tip: modelData.watched ? qsTr("Mark unwatched") : qsTr("Mark watched")
                                color: modelData.watched ? Theme.teal : Theme.text3
                                onClicked: Library.setWatched(modelData.id, !modelData.watched)
                            }
                        }
                    }
                }
            }
        }
    }

    MetaDialog { id: metaDialog; anchors.centerIn: parent }
    FolderDialog {
        id: condenseDir
        title: qsTr("Save condensed audio to")
        currentFolder: sp.eps.length ? Togu.fileUrl(sp.eps[0].path.substring(0, sp.eps[0].path.lastIndexOf("/"))) : ""
        onAccepted: {
            Library.exportCondensed(sp.seriesId, selectedFolder)
            Togu.toast(qsTr("Making condensed audio…"), "blue")
        }
    }
    Connections {
        target: Library
        function onCondensedDone(folder, written, skipped) {
            if (written === 0) Togu.toast(qsTr("No episode has indexed Japanese subtitles to condense."), "pink")
            else Togu.toast(skipped ? qsTr("Condensed %1 episodes, skipped %2 without subtitles.").arg(written).arg(skipped)
                                    : qsTr("Condensed %1 episodes.").arg(written), "teal")
        }
    }
}
