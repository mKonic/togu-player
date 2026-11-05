import QtQuick

// The next thing to play in a series, with how far along it is.
Item {
    id: c
    property var item
    signal clicked()
    width: 300
    height: 186
    Rectangle {
        id: art
        width: parent.width
        height: 168
        radius: 16
        color: Theme.raised
        clip: true
        Image {
            anchors.fill: parent
            source: c.item.banner || "image://thumb/" + c.item.id
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            sourceSize.width: 600
        }
        Rectangle {
            anchors.fill: parent
            gradient: Gradient { GradientStop { position: 0.35; color: "transparent" } GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0.85) } }
        }
        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 14
            spacing: 2
            Text { width: parent.width; text: c.item.seriesTitle; color: "white"; font.pixelSize: 15; font.weight: Font.Bold; elide: Text.ElideRight }
            Text {
                width: parent.width
                text: (c.item.episode >= 0 ? qsTr("Episode %1").arg(c.item.episode) : c.item.name)
                      + (c.item.duration > 0 && c.item.position > 0 ? " · " + qsTr("%1 left").arg(Togu.formatTime(c.item.duration - c.item.position)) : "")
                color: Qt.rgba(1, 1, 1, 0.75); font.pixelSize: 12; elide: Text.ElideRight
            }
        }
        Rectangle {
            anchors.centerIn: parent
            width: 52; height: 52; radius: 26
            color: Qt.rgba(1, 1, 1, hover.hovered ? 0.95 : 0.8)
            Icon { anchors.centerIn: parent; anchors.horizontalCenterOffset: 2; name: "play"; size: 22; color: "#0a002a" }
        }
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width * (c.item.duration > 0 ? c.item.position / c.item.duration : 0)
            height: 4
            color: Theme.purple
        }
    }
    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: c.clicked() }
}
