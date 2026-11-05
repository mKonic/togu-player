import QtQuick
import QtQuick.Layouts

// A setting picked from a few named options, bound to Settings[key].
SettingRow {
    id: c
    property string key
    property var options: [] // [{text, value}]
    TCombo {
        width: 220
        model: c.options
        Component.onCompleted: currentIndex = Math.max(0, indexOfValue(Settings[c.key]))
        onActivated: Settings.set(c.key, currentValue)
    }
}
