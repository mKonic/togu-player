import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Togu

// Every word with a status: search, filter, change.
Item {
    id: wp
    property string status: "all"
    property string sort: "recent"
    property string query: ""
    property int rev: 0
    property var selected: ({})     // key -> true
    readonly property int selectedCount: Object.keys(selected).length
    readonly property var result: { rev; return Togu.wordList(status, query, sort, 500) }
    Connections { target: Togu; function onStatusesChanged() { wp.rev++ } }
    Timer { id: searchDelay; interval: 200; onTriggered: wp.query = search.text }

    readonly property var statuses: [
        { id: "all", t: qsTr("All") },
        { id: "known", t: qsTr("Known") },
        { id: "learning", t: qsTr("Learning") },
        { id: "tracked", t: qsTr("Tracked") },
        { id: "unknown", t: qsTr("Unknown") },
        { id: "ignored", t: qsTr("Ignored") }
    ]
    function toggle(key) {
        const s = Object.assign({}, selected)
        if (s[key]) delete s[key]; else s[key] = true
        selected = s
    }
    function setSelected(st) {
        Togu.setStatus(Object.keys(selected), st)
        selected = ({})
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 14

        RowLayout {
            spacing: 10
            Text { text: qsTr("Words"); color: Theme.text; font.pixelSize: 28; font.weight: Font.Bold; font.family: Theme.round; Layout.fillWidth: true }
            TField {
                id: search
                Layout.preferredWidth: 260
                placeholderText: qsTr("Search")
                onTextChanged: searchDelay.restart()
            }
            TCombo {
                Layout.preferredWidth: 170
                model: [{ text: qsTr("Newest first"), value: "recent" }, { text: qsTr("Most frequent"), value: "freq" }, { text: qsTr("By reading"), value: "alpha" }]
                onActivated: wp.sort = currentValue
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: wp.statuses
                TButton {
                    small: true
                    quiet: wp.status !== modelData.id
                    tone: wp.status === modelData.id ? "purple" : "plain"
                    text: modelData.t + "  " + (wp.result.counts[modelData.id] || 0).toLocaleString(Qt.locale(), "f", 0)
                    onClicked: { wp.status = modelData.id; wp.selected = ({}) }
                }
            }
        }

        // Bulk actions for the selected rows.
        Rectangle {
            visible: wp.selectedCount > 0
            Layout.fillWidth: true
            implicitHeight: 48
            radius: 14
            color: Theme.tint
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 8
                spacing: 6
                Text { text: qsTr("%n selected", "", wp.selectedCount); color: Theme.text; font.pixelSize: 13; font.weight: Font.DemiBold; Layout.fillWidth: true }
                Repeater {
                    model: ["known", "learning", "tracked", "unknown", "ignored"]
                    TButton { small: true; quiet: true; tone: "plain"; text: wp.statuses.find(s => s.id === modelData).t; onClicked: wp.setSelected(modelData) }
                }
                IconButton { iconName: "close"; tip: qsTr("Clear selection"); onClicked: wp.selected = ({}) }
            }
        }

        Text {
            visible: wp.result.rows.length === 0
            text: wp.query ? qsTr("No saved word matches “%1”.").arg(wp.query)
                           : qsTr("Words you mark while watching or reading show up here. Anki sync and word list imports fill it too.")
            color: Theme.text2; font.pixelSize: 14; wrapMode: Text.Wrap; Layout.fillWidth: true
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: wp.result.rows
            ScrollBar.vertical: ScrollBar {}
            footer: Text {
                width: list.width
                visible: wp.result.total > wp.result.rows.length
                height: visible ? 40 : 0
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Showing %1 of %2. Search to narrow it down.").arg(wp.result.rows.length).arg(wp.result.total)
                color: Theme.text3; font.pixelSize: 12
            }
            delegate: Rectangle {
                id: row
                width: ListView.view.width
                height: 56
                radius: 12
                readonly property bool sel: !!wp.selected[modelData.key]
                color: sel ? Theme.purpleLow : (hov.hovered ? Theme.fill : "transparent")
                HoverHandler { id: hov }
                TapHandler { onTapped: wp.toggle(modelData.key) }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 8
                    spacing: 12
                    Rectangle { width: 4; height: 28; radius: 2; color: Theme.statusColor(modelData.status) }
                    Column {
                        Layout.preferredWidth: 200
                        Text {
                            width: parent.width
                            text: modelData.lemma
                            color: Theme.text; font.family: Theme.jp; font.pixelSize: 19; elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            visible: modelData.reading !== "" && modelData.reading !== modelData.lemma
                            text: modelData.reading
                            color: Theme.text2; font.family: Theme.jp; font.pixelSize: 12; elide: Text.ElideRight
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData.gloss || ""
                        color: Theme.text2; font.pixelSize: 13; elide: Text.ElideRight
                    }
                    Chip { visible: modelData.rank > 0; text: "#" + modelData.rank; tone: "blue" }
                    Chip { visible: modelData.card; text: qsTr("Anki"); tone: "teal" }
                    // Quick status change without selecting.
                    Row {
                        spacing: 2
                        Repeater {
                            model: [{ s: "unknown", k: "1" }, { s: "learning", k: "2" }, { s: "known", k: "3" }, { s: "ignored", k: "4" }]
                            Rectangle {
                                width: 26; height: 26; radius: 13
                                color: dotHov.hovered ? Theme.fillHover : "transparent"
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 12; height: 12; radius: 6
                                    color: Theme.statusColor(modelData.s)
                                    opacity: wp.result.rows[row.index] && wp.result.rows[row.index].status === modelData.s ? 1 : 0.35
                                }
                                HoverHandler { id: dotHov }
                                TapHandler { onTapped: Togu.setStatus(wp.result.rows[row.index].key, modelData.s) }
                                ToolTip.visible: dotHov.hovered
                                ToolTip.delay: 500
                                ToolTip.text: wp.statuses.find(x => x.id === modelData.s).t
                            }
                        }
                    }
                }
                required property int index
                required property var modelData
            }
        }
    }
}
