import QtQuick
import QtQuick.Effects
import Togu

// A YouTube video in the grid: thumbnail with duration, title, channel, views and age.
Item {
    id: card
    property var video
    signal clicked()
    signal channelClicked()
    implicitHeight: art.height + info.implicitHeight + 10
    readonly property bool hovered: hover.hovered

    function dur(s) {
        if (!s) return ""
        const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60), sec = s % 60
        return (h ? h + ":" + String(m).padStart(2, "0") : m) + ":" + String(sec).padStart(2, "0")
    }

    Rectangle {
        id: art
        width: parent.width
        height: width * 9 / 16
        radius: 12
        color: Theme.raised
        Image {
            id: img
            anchors.fill: parent
            source: card.video.thumb
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            sourceSize.width: 480
            visible: false
        }
        MultiEffect {
            anchors.fill: parent
            source: img
            visible: img.status === Image.Ready
            maskEnabled: true
            maskSource: mask
            brightness: card.hovered ? 0.06 : 0
        }
        Rectangle { id: mask; anchors.fill: parent; radius: 12; visible: false; layer.enabled: true }
        Rectangle {
            visible: card.video.duration > 0
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 8
            width: dt.implicitWidth + 10
            height: dt.implicitHeight + 4
            radius: 6
            color: Qt.rgba(0, 0, 0, 0.75)
            Text { id: dt; anchors.centerIn: parent; text: card.dur(card.video.duration); color: "white"; font.pixelSize: 12; font.weight: Font.DemiBold }
        }
    }
    Column {
        id: info
        anchors.top: art.bottom
        anchors.topMargin: 10
        width: parent.width
        spacing: 3
        Text {
            width: parent.width
            text: card.video.title
            color: Theme.text
            font.family: Theme.jp
            font.pixelSize: 14
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: card.video.channel
            color: chHover.hovered ? Theme.text : Theme.text2
            font.family: Theme.jp
            font.pixelSize: 12
            elide: Text.ElideRight
            HoverHandler { id: chHover; enabled: card.video.channelId !== "" }
            TapHandler { enabled: card.video.channelId !== ""; onTapped: card.channelClicked() }
        }
        Text {
            width: parent.width
            visible: text !== ""
            text: [card.video.views, card.video.published].filter(x => x).join(" · ")
            color: Theme.text3
            font.family: Theme.jp
            font.pixelSize: 12
            elide: Text.ElideRight
        }
    }
    HoverHandler { id: hover }
    TapHandler { onTapped: card.clicked() }
}
