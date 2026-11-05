import QtQuick
import QtQuick.Layouts

// Pick-and-download list of the usual dictionaries.
Section {
    id: sd
    title: qsTr("Starter dictionaries")
    subtitle: qsTr("Downloaded from where Yomitan users get them, then imported.")
    property var picked: ({})
    // Set while a download started from this card runs, so its progress shows here.
    property bool running: false
    Connections { target: Togu; function onDictProgressChanged() { if (!Togu.dictBusy) sd.running = false } }
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
        visible: !sd.running
        iconName: "download"
        text: qsTr("Download and import")
        enabled: !Togu.dictBusy && sd.ids().length > 0
        onClicked: { sd.running = true; Togu.downloadDicts(sd.ids()) }
    }
    ColumnLayout {
        visible: sd.running
        Layout.fillWidth: true
        spacing: 6
        Text { text: Togu.dictProgress; color: Theme.text2; font.pixelSize: 13; elide: Text.ElideRight; Layout.fillWidth: true }
        Rectangle {
            Layout.fillWidth: true
            height: 6
            radius: 3
            color: Theme.fill
            Rectangle { width: parent.width * Math.max(0, Math.min(1, Togu.dictFraction)); height: parent.height; radius: 3; color: Theme.purple }
        }
    }
}
