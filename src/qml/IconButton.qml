import QtQuick
import QtQuick.Controls.Basic

// Round icon-only button with a tooltip.
AbstractButton {
    id: b
    property string iconName: "info"
    property string tip: ""
    property color color: Theme.text
    property real size: 40
    property real iconSize: size * 0.5
    property bool active: false
    opacity: enabled ? 1 : 0.4
    implicitWidth: size
    implicitHeight: size
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: b.iconName
            size: b.iconSize
            color: b.active ? Theme.purple : b.color
        }
    }
    background: Rectangle {
        radius: width / 2
        color: b.down ? Theme.fillHover : (b.hovered || b.active) ? Theme.fill : "transparent"
        Behavior on color { ColorAnimation { duration: 90 } }
    }
    ToolTip.visible: hovered && tip !== ""
    ToolTip.delay: 600
    ToolTip.text: tip
}
