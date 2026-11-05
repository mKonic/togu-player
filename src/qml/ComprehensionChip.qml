import QtQuick
import "Difficulty.js" as D

// "87% · Good" in the rating's colour; hidden when there is no score yet.
Rectangle {
    id: c
    property int value: -1
    property bool compact: false
    visible: value >= 0
    implicitWidth: t.implicitWidth + 16
    implicitHeight: 22
    radius: 11
    color: Theme.toneColor(D.tone(value))
    Text {
        id: t
        anchors.centerIn: parent
        text: c.compact ? c.value + "%" : c.value + "% · " + D.label(c.value)
        color: "white"
        font.pixelSize: 11
        font.weight: Font.Bold
    }
}
