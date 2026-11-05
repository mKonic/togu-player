import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Togu

// Word statuses from Anki: learning decks, known decks, and which field holds the word.
ColumnLayout {
    id: st
    spacing: 16
    property var modelFields: ({})
    Component.onCompleted: Cards.allModelFields()
    Connections { target: Cards; function onAllFieldsReady(m) { st.modelFields = m } }
    function toggle(key, deck, on) {
        const list = (Settings[key] || []).filter(d => d !== deck)
        if (on) list.push(deck)
        Settings.set(key, list)
    }
    function modelConf(model) {
        const conf = (Settings.ankiStatusModels || {})[model]
        if (conf) return conf
        const g = Cards.guessFields(st.modelFields[model] || [], model)
        let word = "", reading = ""
        for (const f in g) { if (g[f] === "{expression}" && !word) word = f; if (g[f] === "{reading}" && !reading) reading = f }
        return { word: word, reading: reading, templates: [] }
    }
    function setConf(model, k, v) {
        const all = Object.assign({}, Settings.ankiStatusModels || {})
        const c = Object.assign({}, st.modelConf(model))
        c[k] = v
        all[model] = c
        Settings.set("ankiStatusModels", all)
    }

    Section {
        title: qsTr("Learning status from Anki")
        subtitle: qsTr("Words in your learning decks take their status from how you're doing on their cards: new cards are tracked, cards in learning or under %1 days are learning, and cards at %1 days or more (FSRS stability when your deck uses FSRS) are known. A known word goes back to learning only when you fail it. A status you set by hand holds until you review that card again.").arg(Settings.ankiMatureDays)
        SwitchRow { label: qsTr("Use my Anki decks for word statuses"); key: "ankiStatusSync" }
        RowLayout {
            visible: Settings.ankiStatusSync
            spacing: 10
            TButton { small: true; iconName: "sync"; text: qsTr("Sync now"); enabled: !Cards.statusSyncing; onClicked: Cards.syncStatuses(false) }
            BusyIndicator { running: Cards.statusSyncing; visible: running; implicitWidth: 22; implicitHeight: 22 }
            Text { text: Cards.statusSyncReport; color: Theme.text3; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
        }
    }
    Section {
        visible: Settings.ankiStatusSync
        title: qsTr("Decks")
        GridLayout {
            columns: 3
            columnSpacing: 18
            rowSpacing: 6
            Text { text: qsTr("Deck"); color: Theme.text3; font.pixelSize: 12 }
            Text { text: qsTr("Learning"); color: Theme.text3; font.pixelSize: 12 }
            Text { text: qsTr("All known"); color: Theme.text3; font.pixelSize: 12 }
            Repeater {
                model: Cards.decks.length * 3
                delegate: Loader {
                    readonly property string deck: Cards.decks[Math.floor(index / 3)]
                    readonly property int col: index % 3
                    sourceComponent: col === 0 ? deckName : col === 1 ? learnSwitch : knownSwitch
                    Component { id: deckName; Text { text: deck; color: Theme.text; font.pixelSize: 14 } }
                    Component { id: learnSwitch; TSwitch { checked: (Settings.ankiStatusDecks || []).indexOf(deck) >= 0; onToggled: st.toggle("ankiStatusDecks", deck, checked) } }
                    Component { id: knownSwitch; TSwitch { checked: (Settings.ankiKnownDecks || []).indexOf(deck) >= 0; onToggled: st.toggle("ankiKnownDecks", deck, checked) } }
                }
            }
        }
        Text { text: qsTr("\"All known\" marks every word in a deck as known whatever its schedule, e.g. a beginner deck you've finished."); color: Theme.text3; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true }
        SliderRow { label: qsTr("Known from"); hint: qsTr("Anki calls cards mature at 21 days"); key: "ankiMatureDays"; from: 7; to: 90; step: 1; suffix: qsTr(" days") }
        SliderRow { label: qsTr("Leech after"); hint: qsTr("Words failed this often stay in learning until they reach the known line"); key: "ankiLeechLapses"; from: 3; to: 20; step: 1; suffix: qsTr(" lapses") }
    }
    Section {
        visible: Settings.ankiStatusSync && Object.keys(st.modelFields).length > 0
        title: qsTr("Where the word is")
        subtitle: qsTr("For each note type, the field with the word and the one with its reading.")
        Repeater {
            model: Object.keys(st.modelFields).filter(m => st.modelFields[m].length > 2)
            delegate: RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Text { text: modelData; color: Theme.text; font.pixelSize: 14; Layout.preferredWidth: 180; elide: Text.ElideRight }
                TCombo {
                    Layout.preferredWidth: 200
                    model: st.modelFields[modelData].map(f => ({ text: f, value: f }))
                    displayText: qsTr("Word: ") + (st.modelConf(modelData).word || "—")
                    onActivated: st.setConf(modelData, "word", currentValue)
                }
                TCombo {
                    Layout.preferredWidth: 220
                    model: [{ text: qsTr("(none)"), value: "" }].concat(st.modelFields[modelData].map(f => ({ text: f, value: f })))
                    displayText: qsTr("Reading: ") + (st.modelConf(modelData).reading || "—")
                    onActivated: st.setConf(modelData, "reading", currentValue)
                }
            }
        }
    }
}
