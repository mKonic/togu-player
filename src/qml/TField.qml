import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: f
    implicitHeight: 40
    leftPadding: 14
    rightPadding: 14
    color: Theme.text
    placeholderTextColor: Theme.text3
    selectionColor: Theme.purple
    selectedTextColor: "white"
    font.pixelSize: 14
    background: Rectangle {
        radius: 12
        color: Theme.fill
        border.width: f.activeFocus ? 2 : 1
        border.color: f.activeFocus ? Theme.purple : Theme.line
    }
}
