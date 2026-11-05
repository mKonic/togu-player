import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Togu

// The series' name and description, typed by hand.
Popup {
    id: d
    property int seriesId: 0
    function openFor(id, title, description) {
        seriesId = id
        nameField.text = title || ""
        descField.text = description || ""
        open()
        nameField.forceActiveFocus()
    }
    function save() {
        Library.editInfo(seriesId, nameField.text, descField.text)
        close()
    }
    modal: true
    width: Math.min(620, parent ? parent.width - 80 : 620)
    padding: 22
    background: Rectangle { radius: 24; color: Theme.raised; border.color: Theme.line }
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        Text { text: qsTr("Edit info"); color: Theme.text; font.pixelSize: 20; font.weight: Font.Bold }
        Text { text: qsTr("Name"); color: Theme.text2; font.pixelSize: 13; font.weight: Font.DemiBold }
        TField { id: nameField; Layout.fillWidth: true; onAccepted: d.save() }
        Text { text: qsTr("Description"); color: Theme.text2; font.pixelSize: 13; font.weight: Font.DemiBold }
        TArea { id: descField; Layout.fillWidth: true; Layout.preferredHeight: 180 }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            TButton { quiet: true; tone: "plain"; text: qsTr("Cancel"); onClicked: d.close() }
            TButton { text: qsTr("Save"); onClicked: d.save() }
        }
    }
}
