import QtQuick
import QtQuick.Controls.Basic

TextArea {
    id: a
    wrapMode: TextEdit.Wrap
    color: Theme.text
    placeholderTextColor: Theme.text3
    selectionColor: Theme.purple
    selectedTextColor: "white"
    font.pixelSize: 14
    leftPadding: 12
    rightPadding: 12
    topPadding: 9
    bottomPadding: 9
    background: Rectangle {
        radius: 12
        color: Theme.fill
        border.width: a.activeFocus ? 2 : 1
        border.color: a.activeFocus ? Theme.purple : Theme.line
    }
}
