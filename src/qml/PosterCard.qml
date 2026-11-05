import QtQuick
import QtQuick.Effects

// A series in the grid: cover, title, comprehension and progress.
Item {
    id: card
    property var series
    property bool square: false
    signal clicked()
    width: 170
    height: art.height + 58
    readonly property bool hovered: hover.hovered

    Rectangle {
        id: art
        width: parent.width
        height: card.square ? width : width * 1.48
        radius: 14
        color: Theme.raised
        clip: true
        scale: card.hovered ? 1.03 : 1
        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        Image {
            id: img
            anchors.fill: parent
            source: card.series.poster
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            sourceSize.width: 340
            visible: false
        }
        MultiEffect {
            anchors.fill: parent
            source: img
            visible: img.status === Image.Ready
            maskEnabled: true
            maskSource: mask
        }
        Rectangle { id: mask; anchors.fill: parent; radius: 14; visible: false; layer.enabled: true }
        Column {
            anchors.centerIn: parent
            visible: img.status !== Image.Ready
            spacing: 8
            width: parent.width - 24
            Icon { anchors.horizontalCenter: parent.horizontalCenter; name: card.square ? "music" : "film"; size: 34; color: Theme.text3 }
            Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: card.series.title; color: Theme.text2; wrapMode: Text.Wrap; font.pixelSize: 13; maximumLineCount: 3; elide: Text.ElideRight }
        }
        ComprehensionChip {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 8
            value: card.series.comprehension
            compact: true
        }
        // Watched progress
        Rectangle {
            visible: card.series.watched > 0
            anchors.bottom: parent.bottom
            width: parent.width * card.series.watched / Math.max(1, card.series.count)
            height: 4
            color: Theme.purple
        }
    }
    Text {
        anchors.top: art.bottom
        anchors.topMargin: 8
        width: parent.width
        text: card.series.title
        color: Theme.text
        font.pixelSize: 14
        font.weight: Font.DemiBold
        elide: Text.ElideRight
    }
    Text {
        anchors.top: art.bottom
        anchors.topMargin: 28
        width: parent.width
        text: (card.series.count === 1 ? qsTr("1 file") : qsTr("%1 files").arg(card.series.count))
              + (card.series.watched > 0 ? " · " + qsTr("%1 watched").arg(card.series.watched) : "")
        color: Theme.text3
        font.pixelSize: 12
    }
    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: card.clicked() }
}
