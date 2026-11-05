import QtQuick

Item {
    id: b
    property var item
    signal clicked()
    width: 140
    height: art.height + 46
    Rectangle {
        id: art
        width: parent.width
        height: width * 1.45
        radius: 10
        color: Theme.raised
        clip: true
        scale: hover.hovered ? 1.03 : 1
        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
        Image { anchors.fill: parent; source: b.item.cover; fillMode: Image.PreserveAspectCrop; asynchronous: true; sourceSize.width: 280 }
        Text {
            anchors.centerIn: parent
            width: parent.width - 20
            visible: !b.item.cover
            text: b.item.title
            color: Theme.text2
            font.family: Theme.jp
            font.pixelSize: 14
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }
        Rectangle {
            anchors.bottom: parent.bottom
            visible: b.item.progress > 0
            width: parent.width * b.item.progress
            height: 4
            color: Theme.purple
        }
    }
    Text {
        anchors.top: art.bottom
        anchors.topMargin: 6
        width: parent.width
        text: b.item.title
        color: Theme.text
        font.pixelSize: 13
        font.weight: Font.DemiBold
        elide: Text.ElideRight
    }
    Text {
        anchors.top: art.bottom
        anchors.topMargin: 24
        width: parent.width
        text: b.item.progress > 0 ? Math.round(b.item.progress * 100) + "%" + (b.item.author ? " · " + b.item.author : "") : (b.item.author || "")
        color: Theme.text3
        font.pixelSize: 11
        elide: Text.ElideRight
    }
    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: b.clicked() }
}
