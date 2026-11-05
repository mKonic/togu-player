import QtQuick
import QtQuick.Layouts

// A titled group of settings rows on a raised card.
Rectangle {
    id: s
    property string title: ""
    property string subtitle: ""
    default property alias content: col.data
    Layout.fillWidth: true
    implicitHeight: col.implicitHeight + 36
    radius: 20
    color: Theme.raised
    border.color: Theme.line
    ColumnLayout {
        id: col
        x: 20
        y: 18
        width: parent.width - 40
        spacing: 12
        Text { visible: s.title !== ""; text: s.title; color: Theme.text; font.pixelSize: 17; font.weight: Font.Bold }
        Text { visible: s.subtitle !== ""; text: s.subtitle; color: Theme.text2; font.pixelSize: 13; wrapMode: Text.Wrap; Layout.fillWidth: true }
    }
}
