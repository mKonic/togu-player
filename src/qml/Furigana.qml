import QtQuick

// A word with its reading above each kanji run (no outline; for panels and the popup).
Row {
    id: f
    property var segs: []
    property real size: 28
    property color color: Theme.text
    property int weight: Font.Bold
    Repeater {
        model: f.segs
        delegate: Column {
            required property var modelData
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: modelData.r || " "
                font.family: Theme.jp
                font.pixelSize: f.size * 0.45
                color: Theme.text2
                opacity: modelData.r ? 1 : 0
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: modelData.t
                font.family: Theme.jp
                font.pixelSize: f.size
                font.weight: f.weight
                color: f.color
            }
        }
    }
}
