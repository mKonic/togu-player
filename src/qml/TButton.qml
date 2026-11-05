import QtQuick
import QtQuick.Controls.Basic

// Pill button. `tone` picks the colour; `flat` makes it a quiet fill.
Button {
    id: b
    property string tone: "purple"
    property bool quiet: false
    property string iconName: ""
    property bool small: false
    implicitHeight: small ? 32 : 40
    leftPadding: small ? 14 : 18
    rightPadding: small ? 14 : 18
    font.pixelSize: small ? 13 : 14
    font.weight: Font.DemiBold
    hoverEnabled: true
    readonly property color base: tone === "plain" ? Theme.text : Theme.toneColor(tone)
    contentItem: Row {
        spacing: 7
        Icon {
            visible: b.iconName !== ""
            name: b.iconName
            size: b.small ? 15 : 17
            color: b.quiet ? b.base : "white"
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: b.text
            font: b.font
            color: b.quiet ? (b.tone === "plain" ? Theme.text : b.base) : "white"
            anchors.verticalCenter: parent.verticalCenter
            visible: text !== ""
        }
    }
    background: Rectangle {
        radius: height / 2
        color: b.quiet ? (b.down ? Theme.fillHover : b.hovered ? Theme.fill : "transparent")
                      : Qt.lighter(b.base, b.down ? 0.9 : b.hovered ? 1.1 : 1.0)
        border.width: b.quiet ? 1 : 0
        border.color: Theme.line
        opacity: b.enabled ? 1 : 0.45
        Behavior on color { ColorAnimation { duration: 90 } }
    }
}
