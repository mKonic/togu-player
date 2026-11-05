import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Togu

ColumnLayout {
    id: as
    spacing: 16
    property var fieldNames: []
    Component.onCompleted: { Cards.refresh(); if (Settings.ankiModel) Cards.modelFields(Settings.ankiModel) }
    Connections {
        target: Cards
        function onFieldsReady(model, fields) {
            if (model !== Settings.ankiModel) return
            as.fieldNames = fields
            // A new note type: start from the guesses (the Lapis preset for Lapis).
            const cur = Settings.ankiFields || {}
            if (!fields.some(f => cur[f] !== undefined)) Settings.set("ankiFields", Cards.guessFields(fields, model))
        }
    }
    Section {
        title: qsTr("Connection")
        subtitle: qsTr("Togu adds cards through the AnkiConnect add-on, so Anki needs to be open.")
        SettingRow {
            label: Cards.connected ? qsTr("Connected to Anki") : qsTr("Not connected")
            hint: Cards.connected ? "" : Cards.ankiError
            TButton { small: true; quiet: true; tone: "plain"; iconName: "refresh"; text: qsTr("Check again"); onClicked: Cards.refresh() }
        }
        SettingRow {
            label: qsTr("AnkiConnect address")
            TField { width: 260; text: Settings.ankiUrl; onEditingFinished: { Settings.set("ankiUrl", text.trim()); Cards.refresh() } }
        }
        SettingRow {
            label: qsTr("AnkiConnect key"); hint: qsTr("Only if you set one in AnkiConnect's config")
            TField { width: 260; text: Settings.ankiKey; echoMode: TextInput.Password; onEditingFinished: Settings.set("ankiKey", text) }
        }
    }
    Section {
        title: qsTr("Cards")
        enabled: Cards.connected
        SettingRow {
            label: qsTr("Deck")
            TCombo {
                width: 260
                model: Cards.decks.map(d => ({ text: d, value: d }))
                currentIndex: Math.max(-1, Cards.decks.indexOf(Settings.ankiDeck))
                displayText: Settings.ankiDeck || qsTr("Choose…")
                onActivated: Settings.set("ankiDeck", currentValue)
            }
        }
        SettingRow {
            label: qsTr("Note type")
            TCombo {
                width: 260
                model: Cards.models.map(m => ({ text: m, value: m }))
                currentIndex: Math.max(-1, Cards.models.indexOf(Settings.ankiModel))
                displayText: Settings.ankiModel || qsTr("Choose…")
                onActivated: { Settings.set("ankiModel", currentValue); Settings.set("ankiFields", ({})); Cards.modelFields(currentValue) }
            }
        }
        SettingRow {
            label: qsTr("Tags"); hint: qsTr("Separated by spaces")
            TField { width: 260; text: Settings.ankiTags; onEditingFinished: Settings.set("ankiTags", text) }
        }
        SwitchRow { label: qsTr("Allow duplicates"); key: "ankiDuplicates" }
    }
    Section {
        title: qsTr("Fields")
        visible: as.fieldNames.length > 0
        subtitle: qsTr("What goes in each field. Mix text with markers like {expression} or {sentence-bold}.")
        RowLayout {
            TButton {
                small: true; quiet: true; tone: "plain"; text: qsTr("Fill in automatically")
                onClicked: Settings.set("ankiFields", Cards.guessFields(as.fieldNames, Settings.ankiModel))
            }
        }
        Repeater {
            model: as.fieldNames
            delegate: SettingRow {
                label: modelData
                TCombo {
                    width: 300
                    editable: true
                    model: [{ text: qsTr("(empty)"), value: "" }].concat(Cards.markers.map(m => ({ text: "{" + m.key + "}  " + m.about, value: "{" + m.key + "}" })))
                    displayText: (Settings.ankiFields || {})[modelData] || qsTr("(empty)")
                    onActivated: {
                        const f = Object.assign({}, Settings.ankiFields || {})
                        f[modelData] = currentValue
                        Settings.set("ankiFields", f)
                    }
                    onAccepted: {
                        const f = Object.assign({}, Settings.ankiFields || {})
                        f[modelData] = editText
                        Settings.set("ankiFields", f)
                    }
                }
            }
        }
    }
    AnkiStatusSettings { visible: Cards.connected }
}
