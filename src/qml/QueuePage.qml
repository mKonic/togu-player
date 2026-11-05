import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Togu

// Lines marked while watching (L) to turn into cards later, and cards already made.
Item {
    id: qp
    property int rev: 0
    property string tab: "queue"
    readonly property var queue: { rev; return Queue.list() }
    readonly property var made: { rev; return Cards.history(100) }
    Connections { target: Cards; function onAdded() { qp.rev++ } }
    Connections { target: Queue; function onChanged() { qp.rev++ } }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 16
        RowLayout {
            Text { text: qsTr("Queue"); color: Theme.text; font.pixelSize: 28; font.weight: Font.Bold; font.family: Theme.round; Layout.fillWidth: true }
            TButton { visible: qp.tab === "queue" && qp.queue.length > 0; quiet: true; tone: "plain"; iconName: "trash"; text: qsTr("Clear"); onClicked: Queue.clear() }
        }
        Row {
            spacing: 6
            Repeater {
                model: [{ id: "queue", t: qsTr("To make (%1)").arg(qp.queue.length) }, { id: "made", t: qsTr("Made") }]
                TButton { small: true; quiet: qp.tab !== modelData.id; tone: qp.tab === modelData.id ? "purple" : "plain"; text: modelData.t; onClicked: qp.tab = modelData.id }
            }
        }
        Text {
            visible: qp.tab === "queue" && qp.queue.length === 0
            text: qsTr("Press L while watching to put the current line here, then make its card when you have time.")
            color: Theme.text2; font.pixelSize: 14; wrapMode: Text.Wrap; Layout.fillWidth: true
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: qp.tab === "queue" ? qp.queue : qp.made
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                width: ListView.view.width
                height: col.implicitHeight + 24
                radius: 16
                color: Theme.raised
                border.color: Theme.line
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12
                    Column {
                        id: col
                        Layout.fillWidth: true
                        spacing: 3
                        Text { width: parent.width; text: qp.tab === "queue" ? modelData.text : (modelData.expression || modelData.sentence); color: Theme.text; font.family: Theme.jp; font.pixelSize: 17; wrapMode: Text.Wrap }
                        Text { width: parent.width; visible: text !== ""; text: qp.tab === "queue" ? (modelData.text2 || "") : (modelData.expression ? modelData.sentence : ""); color: Theme.text2; font.pixelSize: 13; wrapMode: Text.Wrap }
                        Text {
                            width: parent.width
                            text: qp.tab === "queue" ? (modelData.source || "") : new Date(modelData.time * 1000).toLocaleString(Qt.locale(), Locale.ShortFormat)
                            color: Theme.text3; font.pixelSize: 12
                        }
                    }
                    TButton { visible: qp.tab === "queue"; small: true; iconName: "cardAdd"; text: qsTr("Make card"); onClicked: Queue.open(modelData.id) }
                    IconButton { visible: qp.tab === "queue"; iconName: "trash"; size: 34; onClicked: Queue.remove(modelData.id) }
                    TButton { visible: qp.tab === "made" && !!modelData.note; small: true; quiet: true; tone: "plain"; iconName: "external"; text: qsTr("Show in Anki"); onClicked: Cards.browse(modelData.note) }
                }
            }
        }
    }
}
