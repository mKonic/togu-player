import QtQuick

// Short messages stacked at the bottom centre.
Column {
    id: t
    spacing: 8
    function show(text, tone) {
        model.append({ msg: text, tone: tone || "purple" })
        if (model.count > 3) model.remove(0)
    }
    ListModel { id: model }
    Repeater {
        model: model
        delegate: Rectangle {
            id: d
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(label.implicitWidth + 40, 560)
            height: label.implicitHeight + 20
            radius: height / 2
            color: Theme.raised
            border.color: Theme.line
            opacity: 0
            Component.onCompleted: opacity = 1
            Behavior on opacity { NumberAnimation { duration: 140 } }
            Rectangle {
                width: 8; height: 8; radius: 4
                color: Theme.toneColor(tone)
                anchors.verticalCenter: parent.verticalCenter
                x: 14
            }
            Text {
                id: label
                x: 30
                width: Math.min(implicitWidth, 510)
                anchors.verticalCenter: parent.verticalCenter
                text: msg
                color: Theme.text
                font.pixelSize: 14
                wrapMode: Text.Wrap
            }
            Timer {
                interval: 3200; running: true
                onTriggered: model.remove(index)
            }
        }
    }
}
