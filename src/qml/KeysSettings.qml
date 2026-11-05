import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Togu

// Player shortcuts: click + to add a key, × to remove one.
ColumnLayout {
    id: ks
    spacing: 16
    property string capturing: ""   // action id waiting for a key

    // Takes the next key press while capturing.
    Item {
        id: catcher
        focus: ks.capturing !== ""
        Keys.onPressed: event => {
            event.accepted = true
            if (event.key === Qt.Key_Escape) { ks.capturing = ""; return }
            const s = KeyMap.spec(event)
            if (!s) return
            const other = KeyMap.conflict(ks.capturing, s)
            if (other) {
                KeyMap.setKeys(other.id, KeyMap.keysFor(other.id).filter(k => k !== s))
                Togu.toast(qsTr("%1 no longer does “%2”").arg(s).arg(other.label), "orange")
            }
            const mine = KeyMap.keysFor(ks.capturing)
            if (mine.indexOf(s) < 0) KeyMap.setKeys(ks.capturing, mine.concat([s]))
            ks.capturing = ""
        }
    }
    onCapturingChanged: if (capturing) catcher.forceActiveFocus()

    RowLayout {
        Layout.fillWidth: true
        Text {
            Layout.fillWidth: true
            text: qsTr("Esc always goes back. Click a key to remove it; + waits for a new one.")
            color: Theme.text2; font.pixelSize: 13; wrapMode: Text.Wrap
        }
        TButton {
            small: true; quiet: true; tone: "plain"
            text: qsTr("Reset all")
            enabled: Object.keys(Settings.playerKeys || {}).length > 0
            onClicked: KeyMap.resetAll()
        }
    }

    Repeater {
        model: KeyMap.groups
        Section {
            id: grp
            title: modelData.label
            required property var modelData
            Repeater {
                model: KeyMap.actions.filter(a => a.group === grp.modelData.id)
                RowLayout {
                    id: row
                    required property var modelData
                    readonly property var keys: { const dep = Settings.playerKeys; return dep === undefined ? null : KeyMap.keysFor(modelData.id) }
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    spacing: 6
                    Text { text: row.modelData.label; color: Theme.text; font.pixelSize: 14; Layout.fillWidth: true }
                    Repeater {
                        model: row.keys
                        Rectangle {
                            id: cap
                            required property string modelData
                            implicitWidth: kt.implicitWidth + 18
                            implicitHeight: 28
                            radius: 8
                            color: kh.hovered ? Theme.pinkLow : Theme.fill
                            border.color: Theme.line
                            Text {
                                id: kt
                                anchors.centerIn: parent
                                text: kh.hovered ? "×  " + cap.modelData : cap.modelData
                                color: Theme.text; font.pixelSize: 13; font.weight: Font.DemiBold
                            }
                            HoverHandler { id: kh }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: KeyMap.setKeys(row.modelData.id, row.keys.filter(k => k !== cap.modelData))
                            }
                        }
                    }
                    Rectangle {
                        implicitWidth: ks.capturing === row.modelData.id ? waitText.implicitWidth + 18 : 28
                        implicitHeight: 28
                        radius: 8
                        color: ks.capturing === row.modelData.id ? Theme.purpleLow : (ah.hovered ? Theme.fillHover : "transparent")
                        border.color: ks.capturing === row.modelData.id ? Theme.purple : Theme.line
                        Text {
                            id: waitText
                            anchors.centerIn: parent
                            text: ks.capturing === row.modelData.id ? qsTr("Press a key…") : "+"
                            color: Theme.text; font.pixelSize: ks.capturing === row.modelData.id ? 12 : 16
                        }
                        HoverHandler { id: ah }
                        MouseArea { anchors.fill: parent; onClicked: ks.capturing = ks.capturing === row.modelData.id ? "" : row.modelData.id }
                    }
                    Text {
                        visible: !KeyMap.isDefault(row.modelData.id)
                        text: qsTr("Reset")
                        color: Theme.purple; font.pixelSize: 12
                        Layout.preferredWidth: 40
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: KeyMap.reset(row.modelData.id) }
                    }
                    Item { visible: KeyMap.isDefault(row.modelData.id); Layout.preferredWidth: 40 }
                }
            }
        }
    }
}
