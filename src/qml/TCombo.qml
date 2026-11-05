import QtQuick
import QtQuick.Controls.Basic

// Drop-down with a plain list of {text, value} or strings.
ComboBox {
    id: c
    implicitHeight: 38
    font.pixelSize: 14
    textRole: "text"
    valueRole: "value"
    hoverEnabled: true
    contentItem: Text {
        leftPadding: 14
        rightPadding: 30
        text: c.displayText
        font: c.font
        color: Theme.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Icon {
        name: "down"
        size: 14
        color: Theme.text2
        x: c.width - width - 12
        y: (c.height - height) / 2
    }
    background: Rectangle {
        radius: 12
        color: c.hovered ? Theme.fillHover : Theme.fill
        border.width: 1
        border.color: Theme.line
    }
    delegate: ItemDelegate {
        width: c.width
        height: 34
        highlighted: c.highlightedIndex === index
        contentItem: Text {
            text: model[c.textRole] !== undefined ? model[c.textRole] : modelData
            color: Theme.text
            font.pixelSize: 14
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle { color: highlighted ? Theme.fill : "transparent"; radius: 8 }
    }
    popup: Popup {
        y: c.height + 4
        width: c.width
        implicitHeight: Math.min(contentItem.implicitHeight + 12, 360)
        padding: 6
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: c.popup.visible ? c.delegateModel : null
            currentIndex: c.highlightedIndex
        }
        background: Rectangle { radius: 14; color: Theme.raised; border.color: Theme.line }
    }
}
