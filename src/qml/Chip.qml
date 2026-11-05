import QtQuick

// Small rounded label, e.g. a tag or a status.
Rectangle {
    id: c
    property string text: ""
    property string tone: "purple"
    property bool strong: false
    property real fontSize: 11
    implicitWidth: label.implicitWidth + 14
    implicitHeight: label.implicitHeight + 6
    radius: height / 2
    color: strong ? Theme.toneColor(tone) : Theme.toneLow(tone)
    Text {
        id: label
        anchors.centerIn: parent
        text: c.text
        font.pixelSize: c.fontSize
        font.weight: Font.DemiBold
        color: c.strong ? "white" : (Theme.dark ? "white" : Theme.toneColor(c.tone))
    }
}
