import QtQuick
import QtQuick.Layouts

// Pick-and-download list of the usual dictionaries.
Section {
    id: sd
    title: qsTr("Starter dictionaries")
    subtitle: qsTr("Downloaded from where Yomitan users get them, then imported.")
    property var picked: ({})
    function ids() { return Togu.starterDicts.filter(d => !d.installed && (sd.picked[d.id] !== undefined ? sd.picked[d.id] : d.preselected)).map(d => d.id) }
    Repeater {
        model: Togu.starterDicts
        delegate: RowLayout {
            Layout.fillWidth: true
            spacing: 12
            TSwitch {
                enabled: !modelData.installed && !Togu.dictBusy
                checked: modelData.installed || (sd.picked[modelData.id] !== undefined ? sd.picked[modelData.id] : modelData.preselected)
                onToggled: { const p = Object.assign({}, sd.picked); p[modelData.id] = checked; sd.picked = p }
            }
            Column {
                Layout.fillWidth: true
                Text { text: modelData.title; color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                Text { text: modelData.about; color: Theme.text3; font.pixelSize: 12 }
            }
            Chip { visible: modelData.installed; text: qsTr("Installed"); tone: "teal" }
        }
    }
    TButton {
        iconName: "download"
        text: qsTr("Download and import")
        enabled: !Togu.dictBusy && sd.ids().length > 0
        onClicked: Togu.downloadDicts(sd.ids())
    }
}
