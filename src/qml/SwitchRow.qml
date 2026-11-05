import QtQuick
import QtQuick.Layouts

SettingRow {
    id: s
    property string key
    TSwitch { checked: !!Settings[s.key]; onToggled: Settings.set(s.key, checked) }
}
