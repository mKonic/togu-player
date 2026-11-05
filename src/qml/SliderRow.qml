import QtQuick
import QtQuick.Layouts

SettingRow {
    id: s
    property string key
    property real from: 0
    property real to: 100
    property real step: 1
    property string suffix: ""
    property int decimals: 0
    property real scale: 1
    Row {
        spacing: 10
        TSlider {
            width: 180
            from: s.from; to: s.to; stepSize: s.step
            value: Settings[s.key]
            onMoved: Settings.set(s.key, value)
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            width: 64
            text: (Settings[s.key] * s.scale).toFixed(s.decimals) + s.suffix
            color: Theme.text2
            font.pixelSize: 13
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
