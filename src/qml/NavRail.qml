import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// The left rail: media kinds, then words, queue, stats and settings.
Rectangle {
    id: rail
    property string current: "watch"
    signal picked(string page)
    implicitWidth: 92
    color: Qt.rgba(0, 0, 0, Theme.dark ? 0.18 : 0.02)
    border.color: Theme.line

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 18
        anchors.bottomMargin: 16
        spacing: 4
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 18
            text: "togu"
            font.family: Theme.round
            font.pixelSize: 26
            font.weight: Font.DemiBold
            color: Theme.purple
        }
        Repeater {
            model: [
                { id: "watch", icon: "film", label: qsTr("Watch") },
                { id: "listen", icon: "music", label: qsTr("Listen") },
                { id: "read", icon: "book", label: qsTr("Read") },
                { id: "manga", icon: "manga", label: qsTr("Manga") },
            ]
            delegate: RailButton { item: modelData }
        }
        Rectangle { Layout.alignment: Qt.AlignHCenter; width: 40; height: 1; color: Theme.line; Layout.topMargin: 6; Layout.bottomMargin: 6 }
        Repeater {
            model: [
                { id: "words", icon: "words", label: qsTr("Words") },
                { id: "queue", icon: "queue", label: qsTr("Queue") },
                { id: "stats", icon: "stats", label: qsTr("Stats") },
            ]
            delegate: RailButton { item: modelData }
        }
        Item { Layout.fillHeight: true }
        RailButton { item: ({ id: "settings", icon: "settings", label: qsTr("Settings") }) }
    }

    component RailButton: AbstractButton {
        id: b
        property var item
        readonly property bool on: rail.current === item.id
        Layout.alignment: Qt.AlignHCenter
        implicitWidth: 76
        implicitHeight: 62
        hoverEnabled: true
        onClicked: rail.picked(item.id)
        background: Rectangle {
            radius: 16
            color: b.on ? Theme.tint : b.hovered ? Theme.fill : "transparent"
        }
        contentItem: Column {
            spacing: 4
            topPadding: 9
            Icon { anchors.horizontalCenter: parent.horizontalCenter; name: b.item.icon; size: 22; color: b.on ? Theme.purple : Theme.text2 }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: b.item.label
                font.pixelSize: 11
                font.weight: b.on ? Font.DemiBold : Font.Medium
                color: b.on ? Theme.text : Theme.text2
            }
        }
    }
}
