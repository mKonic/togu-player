import QtQuick
import QtQuick.Controls.Basic

Menu {
    id: m
    padding: 6
    background: Rectangle {
        implicitWidth: 240
        radius: 16
        color: Theme.raised
        border.color: Theme.line
    }
    delegate: MenuItem {
        id: mi
        implicitHeight: 36
        contentItem: Row {
            spacing: 10
            leftPadding: 6
            Icon {
                visible: mi.checkable
                name: "check"
                size: 15
                opacity: mi.checked ? 1 : 0
                color: Theme.purple
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: mi.text
                color: mi.enabled ? Theme.text : Theme.text3
                font.pixelSize: 14
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        indicator: Item {}
        arrow: Item {}
        background: Rectangle { radius: 10; color: mi.highlighted ? Theme.fill : "transparent" }
    }
}
