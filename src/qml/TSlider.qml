import QtQuick
import QtQuick.Controls.Basic

Slider {
    id: s
    property color accent: Theme.purple
    // Fill from the right end to the handle (right-to-left reading).
    property bool fillFromEnd: false
    hoverEnabled: true
    background: Rectangle {
        x: s.leftPadding
        y: s.topPadding + s.availableHeight / 2 - height / 2
        width: s.availableWidth
        height: s.hovered || s.pressed ? 6 : 4
        radius: height / 2
        color: Theme.fillHover
        Rectangle {
            x: s.fillFromEnd ? s.visualPosition * parent.width : 0
            width: (s.fillFromEnd ? 1 - s.visualPosition : s.visualPosition) * parent.width
            height: parent.height
            radius: height / 2
            color: s.accent
        }
        Behavior on height { NumberAnimation { duration: 90 } }
    }
    handle: Rectangle {
        x: s.leftPadding + s.visualPosition * (s.availableWidth - width)
        y: s.topPadding + s.availableHeight / 2 - height / 2
        width: 14
        height: 14
        radius: 7
        color: "white"
        visible: s.hovered || s.pressed
    }
}
