import QtQuick
import QtQuick.Layouts

// Label on the left, control on the right.
RowLayout {
    id: r
    property string label: ""
    property string hint: ""
    default property alias control: holder.data
    Layout.fillWidth: true
    spacing: 16
    Column {
        Layout.fillWidth: true
        spacing: 2
        Text { text: r.label; color: Theme.text; font.pixelSize: 14; width: parent.width; wrapMode: Text.Wrap }
        Text { visible: r.hint !== ""; text: r.hint; color: Theme.text3; font.pixelSize: 12; width: parent.width; wrapMode: Text.Wrap }
    }
    Item {
        id: holder
        implicitWidth: childrenRect.width
        implicitHeight: Math.max(36, childrenRect.height)
    }
}
