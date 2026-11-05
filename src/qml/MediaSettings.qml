import QtQuick
import QtCore
import QtQuick.Dialogs
import QtQuick.Layouts
import Togu

ColumnLayout {
    spacing: 16
    Section {
        title: qsTr("Sentence audio")
        ChoiceRow { label: qsTr("Format"); hint: qsTr("MP3 plays everywhere, AnkiMobile included"); key: "audioFormat"
            options: [{ text: "MP3", value: "mp3" }, { text: "Opus", value: "opus" }] }
        SliderRow { label: qsTr("Extra before the line"); key: "audioPadBefore"; from: 0; to: 1.5; step: 0.05; decimals: 2; suffix: " s" }
        SliderRow { label: qsTr("Extra after the line"); key: "audioPadAfter"; from: 0; to: 1.5; step: 0.05; decimals: 2; suffix: " s" }
    }
    Section {
        title: qsTr("Picture")
        ChoiceRow { label: qsTr("Default"); key: "imageMode"
            options: [{ text: qsTr("Still frame"), value: "still" }, { text: qsTr("Animated image"), value: "animated" }, { text: qsTr("Video clip"), value: "clip" }] }
        ChoiceRow { label: qsTr("Still format"); key: "imageFormat"; options: [{ text: "WebP", value: "webp" }, { text: "JPEG", value: "jpg" }] }
        SliderRow { label: qsTr("Height"); key: "imageHeight"; from: 240; to: 1080; step: 20; suffix: " px" }
        SliderRow { label: qsTr("Longest animation or clip"); key: "clipMaxSeconds"; from: 2; to: 20; step: 1; suffix: " s" }
    }
    Section {
        title: qsTr("Word audio")
        subtitle: qsTr("The local audio collection used by Yomitan's local audio add-on (the folder with entries.db). Found on its own when that add-on is installed in Anki.")
        RowLayout {
            spacing: 8
            TField { Layout.fillWidth: true; text: Settings.wordAudioDir; placeholderText: qsTr("Found automatically"); onEditingFinished: Settings.set("wordAudioDir", text.trim()) }
            TButton { small: true; quiet: true; tone: "plain"; iconName: "folder"; text: qsTr("Choose…"); onClicked: audioDir.open() }
        }
        FilePicker { folder: true; id: audioDir; title: qsTr("Word audio folder"); currentFolder: StandardPaths.writableLocation(StandardPaths.HomeLocation); onAccepted: Settings.set("wordAudioDir", Togu.localPath(selectedFolder)) }
    }
}
