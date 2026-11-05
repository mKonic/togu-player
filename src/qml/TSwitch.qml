import QtQuick
import QtQuick.Controls.Basic

Switch {
    id: s
    hoverEnabled: true
    indicator: Rectangle {
        implicitWidth: 40
        implicitHeight: 22
        x: s.leftPadding
        y: (s.height - height) / 2
        radius: 11
        color: s.checked ? Theme.purple : Theme.fillHover
        Behavior on color { ColorAnimation { duration: 120 } }
        Rectangle {
            x: s.checked ? parent.width - width - 3 : 3
            y: 3
            width: 16
            height: 16
            radius: 8
            color: "white"
            Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        }
    }
    contentItem: Text {
        text: s.text
        leftPadding: s.indicator.width + 10
        color: Theme.text
        font.pixelSize: 14
        verticalAlignment: Text.AlignVCenter
    }
}
