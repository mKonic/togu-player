import QtQuick
import QtCore
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

// Settings in tabs, so no single page grows long.
Item {
    id: sp
    property string tab: "subtitles"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 18
        Text { text: qsTr("Settings"); color: Theme.text; font.pixelSize: 28; font.weight: Font.Bold; font.family: Theme.round }
        Row {
            spacing: 6
            Repeater {
                model: [
                    { id: "subtitles", text: qsTr("Subtitles") },
                    { id: "playback", text: qsTr("Playback") },
                    { id: "dicts", text: qsTr("Dictionaries") },
                    { id: "anki", text: qsTr("Anki") },
                    { id: "media", text: qsTr("Card media") },
                    { id: "online", text: qsTr("Online") },
                    { id: "keys", text: qsTr("Keys") },
                    { id: "general", text: qsTr("General") },
                ]
                delegate: AbstractButton {
                    id: tb
                    implicitHeight: 36
                    implicitWidth: tt.implicitWidth + 28
                    hoverEnabled: true
                    onClicked: sp.tab = modelData.id
                    background: Rectangle { radius: 18; color: sp.tab === modelData.id ? Theme.purple : tb.hovered ? Theme.fill : "transparent" }
                    contentItem: Text {
                        id: tt
                        text: modelData.text
                        color: sp.tab === modelData.id ? "white" : Theme.text2
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: loader.implicitHeight + 40
            clip: true
            ScrollBar.vertical: ScrollBar {}
            Loader {
                id: loader
                width: Math.min(parent.width - 16, 860)
                sourceComponent: ({ subtitles: subtitlesTab, playback: playbackTab, dicts: dictsTab, anki: ankiTab, media: mediaTab,
                                    online: onlineTab, keys: keysTab, general: generalTab })[sp.tab]
            }
        }
    }

    Component {
        id: subtitlesTab
        ColumnLayout {
            spacing: 16
            Section {
                title: qsTr("Look")
                SliderRow { label: qsTr("Size"); key: "subSize"; from: 20; to: 100; step: 1; suffix: " px" }
                SliderRow { label: qsTr("Weight"); key: "subWeight"; from: 300; to: 900; step: 100 }
                SliderRow { label: qsTr("Distance from the bottom"); key: "subBottom"; from: 0; to: 0.4; step: 0.01; valueScale: 100; suffix: "%" }
                SliderRow { label: qsTr("Box behind lines"); key: "subBackground"; from: 0; to: 1; step: 0.05; valueScale: 100; suffix: "%" }
                SwitchRow { label: qsTr("Outline"); key: "subOutline" }
                SettingRow {
                    label: qsTr("Font")
                    TCombo {
                        width: 240
                        model: [{ text: "Noto Sans CJK JP", value: "Noto Sans CJK JP" }, { text: "Noto Serif CJK JP", value: "Noto Serif CJK JP" },
                                { text: "M PLUS Rounded 1c", value: "M PLUS Rounded 1c" }, { text: "Kosugi Maru", value: "Kosugi Maru" }]
                        Component.onCompleted: currentIndex = Math.max(0, indexOfValue(Settings.subFont))
                        onActivated: Settings.set("subFont", currentValue)
                    }
                }
                SliderRow { label: qsTr("Second subtitles size"); key: "sub2Size"; from: 14; to: 60; step: 1; suffix: " px" }
                ChoiceRow { label: qsTr("Second subtitles position"); key: "sub2Position"; options: [{ text: qsTr("Top of the video"), value: "top" }, { text: qsTr("Under the Japanese line"), value: "below" }] }
            }
            Section {
                title: qsTr("Words")
                SwitchRow { label: qsTr("Learning status colours"); hint: qsTr("A bar under each word in its status colour"); key: "statusColors" }
                ChoiceRow { label: qsTr("Furigana"); key: "furigana"
                    options: [{ text: qsTr("Unknown words"), value: "unknown" }, { text: qsTr("All words"), value: "all" }, { text: qsTr("On hover"), value: "hover" }, { text: qsTr("Off"), value: "off" }] }
                ChoiceRow { label: qsTr("Pitch accent colours"); key: "pitchColors"
                    options: [{ text: qsTr("Off"), value: "off" }, { text: qsTr("On hover"), value: "hover" }, { text: qsTr("All words"), value: "all" }] }
                ChoiceRow { label: qsTr("Word translations"); hint: qsTr("A few words of meaning under unknown words"); key: "wordGloss"
                    options: [{ text: qsTr("Off"), value: "off" }, { text: qsTr("Unknown words"), value: "unknown" }, { text: qsTr("On hover"), value: "hover" }] }
                ChoiceRow { label: qsTr("Frequency stars"); hint: qsTr("Under the one unknown word of a recommended sentence"); key: "stars"
                    options: [{ text: qsTr("On hover"), value: "hover" }, { text: qsTr("Always"), value: "all" }, { text: qsTr("Off"), value: "off" }] }
                SwitchRow { label: qsTr("Blur the Japanese line until hovered"); key: "subBlur" }
            }
        }
    }

    Component {
        id: playbackTab
        ColumnLayout {
            spacing: 16
            Section {
                title: qsTr("Playback")
                SwitchRow { label: qsTr("Resume where you stopped"); key: "resumePlayback" }
                ChoiceRow { label: qsTr("Pause at the end of each line"); key: "autoPause"
                    options: [{ text: qsTr("Off"), value: "off" }, { text: qsTr("On"), value: "end" }] }
                SwitchRow { label: qsTr("Pause while the pointer is on the subtitles"); key: "pauseOnHover" }
                ChoiceRow { label: qsTr("Look words up"); key: "lookupMode"
                    options: [{ text: qsTr("On hover"), value: "hover" }, { text: qsTr("On click"), value: "click" }] }
                SliderRow { label: qsTr("Seek step") + KeyMap.hint("seekBack"); key: "seekStep"; from: 1; to: 30; step: 1; suffix: " s" }
            }
            Section {
                title: qsTr("Subtitle timing")
                SwitchRow { label: qsTr("Auto-sync"); hint: qsTr("Line subtitles up with the speech the first time they're paired with a video. A manual adjustment pins that video until you sync again."); key: "autoSync" }
                SliderRow { label: qsTr("Small step") + KeyMap.hint("offsetBack"); key: "offsetStep"; from: 0.01; to: 0.5; step: 0.01; valueScale: 1000; suffix: " ms" }
                SliderRow { label: qsTr("Big step"); key: "offsetStepBig"; from: 0.1; to: 5; step: 0.1; decimals: 1; suffix: " s" }
            }
        }
    }

    Component {
        id: dictsTab
        ColumnLayout {
            spacing: 16
            Section {
                title: qsTr("Dictionaries")
                subtitle: qsTr("Yomitan-format dictionaries (.zip): definitions, frequency lists, pitch accent and kanji. The arrows set the order popups show them in.")
                RowLayout {
                    spacing: 10
                    TButton { iconName: "plus"; text: qsTr("Add dictionaries…"); enabled: !Togu.dictBusy; onClicked: dictDialog.open() }
                    Text { visible: Togu.dictBusy; text: Togu.dictProgress; color: Theme.text2; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight }
                }
                ProgressBar {
                    visible: Togu.dictBusy
                    Layout.fillWidth: true
                    value: Togu.dictFraction
                    background: Rectangle { implicitHeight: 6; radius: 3; color: Theme.fill }
                    contentItem: Item { Rectangle { width: parent.width * Togu.dictFraction; height: 6; radius: 3; color: Theme.purple } }
                }
                Repeater {
                    model: Togu.dicts
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 56
                        radius: 14
                        color: Theme.fill
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 8
                            spacing: 10
                            TSwitch { checked: modelData.enabled; onToggled: Togu.setDictEnabled(modelData.id, checked) }
                            Column {
                                Layout.fillWidth: true
                                Text { text: modelData.title; color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold; elide: Text.ElideRight; width: parent.width }
                                Text {
                                    text: ({ terms: qsTr("Definitions"), mixed: qsTr("Definitions"), freq: qsTr("Frequency"), pitch: qsTr("Pitch accent"), kanji: qsTr("Kanji") })[modelData.kind]
                                          + (modelData.terms ? " · " + modelData.terms.toLocaleString() + qsTr(" entries") : "")
                                    color: Theme.text3; font.pixelSize: 12
                                }
                            }
                            IconButton { iconName: "up"; size: 32; tip: qsTr("Move up"); enabled: index > 0; onClicked: Togu.moveDict(modelData.id, -1) }
                            IconButton { iconName: "down"; size: 32; tip: qsTr("Move down"); enabled: index < Togu.dicts.length - 1; onClicked: Togu.moveDict(modelData.id, 1) }
                            IconButton { iconName: "trash"; size: 32; tip: qsTr("Remove"); color: Theme.pink; onClicked: Togu.removeDict(modelData.id) }
                        }
                    }
                }
                Text {
                    visible: Togu.dicts.length === 0 && !Togu.dictBusy
                    text: qsTr("No dictionaries yet. JMdict or Jitendex for meanings, a frequency list (JPDB, BCCWJ) and a pitch dictionary (NHK, Kanjium) cover the essentials.")
                    color: Theme.text2; font.pixelSize: 13; wrapMode: Text.Wrap; Layout.fillWidth: true
                }
            }
            StarterDicts { visible: Togu.starterDicts.some(d => !d.installed) }
            FileDialog {
                id: dictDialog
                title: qsTr("Add dictionaries")
                currentFolder: StandardPaths.writableLocation(StandardPaths.DownloadLocation)
                fileMode: FileDialog.OpenFiles
                nameFilters: [qsTr("Yomitan dictionaries (*.zip)")]
                onAccepted: Togu.importDicts(selectedFiles)
            }
        }
    }

    Component { id: ankiTab; AnkiSettings {} }
    Component { id: mediaTab; MediaSettings {} }
    Component { id: onlineTab; OnlineSettings {} }
    Component { id: keysTab; KeysSettings {} }

    Component {
        id: generalTab
        ColumnLayout {
            spacing: 16
            Section {
                title: qsTr("General")
                ChoiceRow { label: qsTr("Theme"); key: "theme"; options: [{ text: qsTr("Dark"), value: "dark" }, { text: qsTr("Light"), value: "light" }] }
                SliderRow { label: qsTr("Daily immersion goal"); key: "dailyGoalMinutes"; from: 10; to: 300; step: 5; suffix: " min" }
            }
            Section {
                title: qsTr("Word list")
                subtitle: qsTr("One word per line, optionally followed by its reading after a tab or comma.")
                RowLayout {
                    spacing: 8
                    TCombo { id: listStatus; width: 160; model: [{ text: qsTr("Known"), value: "known" }, { text: qsTr("Learning"), value: "learning" }, { text: qsTr("Ignored"), value: "ignored" }, { text: qsTr("Tracked"), value: "tracked" }] }
                    TButton { small: true; quiet: true; tone: "plain"; iconName: "upload"; text: qsTr("Import…"); onClicked: importWords.open() }
                    TButton { small: true; quiet: true; tone: "plain"; iconName: "download"; text: qsTr("Export…"); onClicked: exportWords.open() }
                }
                FileDialog { id: importWords; title: qsTr("Import a word list"); currentFolder: StandardPaths.writableLocation(StandardPaths.DownloadLocation); nameFilters: [qsTr("Text (*.txt *.csv *.tsv)"), qsTr("All files (*)")]; onAccepted: Togu.importWordList(Togu.readTextFile(selectedFile), listStatus.currentValue) }
                FileDialog { id: exportWords; title: qsTr("Export the word list"); currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation); fileMode: FileDialog.SaveFile; defaultSuffix: "txt"; onAccepted: Togu.writeTextFile(selectedFile, Togu.exportWordList(listStatus.currentValue)) }
            }
            Section {
                title: qsTr("About")
                Text { text: "Togu " + Togu.version; color: Theme.text2; font.pixelSize: 13 }
            }
        }
    }
}
