import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMultimedia
import Togu

// Togu's Card Creator: the draft on the left, its media on the right.
Popup {
    id: cc
    property var d: ({})
    property int audioToken: -1
    property int imageToken: -1
    property var session: null
    property var mpv: null
    property string imageMode: Settings.imageMode
    property var wordAudios: []
    property bool confirmEmpty: false
    property double padStart: 0
    property double padEnd: 0

    function openFor(entry, line, surface, s, player) {
        if (!line || line.start === undefined) { Togu.toast(qsTr("No subtitle line here to make a card from."), "orange"); return }
        session = s
        mpv = player
        if (mpv) mpv.paused = true
        const draft = Cards.draft(entry || {}, line, surface || "", s ? s.itemId : 0, s ? s.offset : 0)
        d = draft
        wordAudios = draft.expression ? Cards.wordAudio(draft.expression, draft.reading) : []
        padStart = 0; padEnd = 0
        confirmEmpty = false
        open()
        recut()
    }
    function set(k, v) { const n = Object.assign({}, d); n[k] = v; d = n }
    function audioTrack() { return mpv && mpv.audioTrack > 0 ? mpv.audioTrack - 1 : -1 }
    function recutAudio() {
        if (!d.path) return
        set("sentenceAudio", "")
        audioToken = Cards.cutAudio(d.path, d.start + padStart, d.end + padEnd, audioTrack())
    }
    function recutImage() {
        if (!d.path || d.kind === "audio") return
        set("images", [])
        imageToken = Cards.cutImage(d.path, d.start + padStart, d.end + padEnd, imageMode, mpv && mpv.paused ? mpv.position : -1)
    }
    function recut() { recutAudio(); recutImage() }
    function isEmpty() { return !d.expression && !d.sentence }
    function submit(skipWarning) {
        if (!skipWarning && !d.expression) { confirmEmpty = true; return }
        Cards.add(d)
    }

    modal: true
    width: Math.min(1060, parent ? parent.width - 60 : 1060)
    height: Math.min(720, parent ? parent.height - 60 : 720)
    padding: 0
    background: Rectangle { radius: 24; color: Theme.raised; border.color: Theme.line }
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.55) }
    onClosed: if (mpv && session) mpv.forceActiveFocus()

    Connections {
        target: Cards
        function onMediaReady(token, m) {
            if (token === cc.audioToken && m.sentenceAudio !== undefined) cc.set("sentenceAudio", m.sentenceAudio)
            if (token === cc.imageToken && m.image !== undefined) cc.set("images", m.image ? [m.image] : [])
        }
        function onAdded(id, expr) { if (cc.visible) cc.close() }
        function onFailed(msg) { if (cc.visible) errorText.text = msg }
    }
    MediaPlayer { id: player; audioOutput: AudioOutput {} }

    Shortcut { sequences: ["Ctrl+Return", "Ctrl+Enter"]; enabled: cc.visible; onActivated: cc.submit(false) }
    Shortcut { sequences: ["Ctrl+Shift+Return", "Ctrl+Shift+Enter"]; enabled: cc.visible; onActivated: cc.submit(true) }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 22
        spacing: 14
        RowLayout {
            Text { text: qsTr("Card Creator"); color: Theme.text; font.pixelSize: 22; font.weight: Font.Bold; font.family: Theme.round; Layout.fillWidth: true }
            Chip { text: Settings.ankiDeck || qsTr("No deck"); tone: "blue" }
            Chip { text: Settings.ankiModel || qsTr("No note type"); tone: "purple" }
            Chip { visible: !Cards.connected; text: qsTr("Anki not reachable"); tone: "pink"; strong: true }
            IconButton { iconName: "close"; size: 34; onClicked: cc.close() }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 22
            // ------------------------------------------------ text
            Flickable {
                Layout.fillWidth: true
                Layout.minimumWidth: 360
                Layout.fillHeight: true
                contentHeight: left.implicitHeight
                clip: true
                ScrollBar.vertical: ScrollBar {}
                ColumnLayout {
                    id: left
                    width: parent.width - 12
                    spacing: 10
                    RowLayout {
                        spacing: 10
                        Column {
                            Layout.fillWidth: true
                            Label2 { text: qsTr("Word") }
                            TField { width: parent.width; text: cc.d.expression || ""; font.family: Theme.jp; font.pixelSize: 18; onTextEdited: cc.set("expression", text) }
                        }
                        Column {
                            Layout.preferredWidth: 200
                            Label2 { text: qsTr("Reading") }
                            TField { width: parent.width; text: cc.d.reading || ""; font.family: Theme.jp; font.pixelSize: 18; onTextEdited: cc.set("reading", text) }
                        }
                    }
                    Label2 { text: qsTr("Sentence") }
                    TArea { Layout.fillWidth: true; text: cc.d.sentence || ""; font.family: Theme.jp; font.pixelSize: 17; onTextChanged: if (text !== cc.d.sentence) cc.set("sentence", text) }
                    Label2 { text: qsTr("Translation") }
                    TArea { Layout.fillWidth: true; text: cc.d.translation || ""; onTextChanged: if (text !== cc.d.translation) cc.set("translation", text) }
                    Label2 { text: qsTr("Definitions"); visible: (cc.d.blocks || []).length > 0 }
                    Repeater {
                        model: cc.d.blocks || []
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: bc.implicitHeight + 16
                            radius: 12
                            color: modelData.on ? Theme.fill : "transparent"
                            border.color: Theme.line
                            opacity: modelData.on ? 1 : 0.5
                            ColumnLayout {
                                id: bc
                                x: 10; y: 8
                                width: parent.width - 20
                                RowLayout {
                                    TSwitch {
                                        checked: modelData.on
                                        onToggled: {
                                            const blocks = cc.d.blocks.map((b, i) => i === index ? Object.assign({}, b, { on: checked }) : b)
                                            cc.set("blocks", blocks)
                                        }
                                    }
                                    Text { text: modelData.dict; color: Theme.purple; font.pixelSize: 12; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.preview || modelData.html
                                    textFormat: Text.RichText
                                    wrapMode: Text.Wrap
                                    color: Theme.text
                                    font.pixelSize: 13
                                    maximumLineCount: modelData.on ? 12 : 2
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                    Label2 { text: qsTr("Notes") }
                    TArea { Layout.fillWidth: true; text: cc.d.notes || ""; onTextChanged: if (text !== cc.d.notes) cc.set("notes", text) }
                    Text { text: cc.d.source || ""; color: Theme.text3; font.pixelSize: 12 }
                }
            }
            // ------------------------------------------------ media
            ColumnLayout {
                Layout.preferredWidth: 380
                Layout.maximumWidth: 380
                Layout.fillWidth: false
                Layout.fillHeight: true
                spacing: 10
                Label2 { text: cc.d.kind === "audio" ? qsTr("Picture") : qsTr("Picture") }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 214
                    radius: 14
                    color: "black"
                    clip: true
                    AnimatedImage {
                        anchors.fill: parent
                        fillMode: Image.PreserveAspectFit
                        visible: (cc.d.images || []).length > 0 && !String(cc.d.images[0]).endsWith(".webm")
                        source: visible ? Togu.fileUrl(cc.d.images[0]) : ""
                    }
                    Column {
                        anchors.centerIn: parent
                        visible: (cc.d.images || []).length > 0 && String(cc.d.images[0]).endsWith(".webm")
                        spacing: 6
                        Icon { anchors.horizontalCenter: parent.horizontalCenter; name: "film"; size: 36; color: "white" }
                        Text { text: qsTr("Video clip ready"); color: "white"; font.pixelSize: 13 }
                    }
                    BusyIndicator { anchors.centerIn: parent; running: Cards.busy && (cc.d.images || []).length === 0; visible: running }
                    Text {
                        anchors.centerIn: parent
                        visible: !Cards.busy && (cc.d.images || []).length === 0
                        text: cc.d.kind === "audio" ? qsTr("Audio file: no picture") : qsTr("No picture")
                        color: Theme.text3; font.pixelSize: 13
                    }
                }
                RowLayout {
                    spacing: 6
                    visible: cc.d.kind !== "audio"
                    Repeater {
                        model: [{ v: "still", t: qsTr("Still") }, { v: "animated", t: qsTr("Animated") }, { v: "clip", t: qsTr("Clip") }]
                        TButton {
                            small: true
                            quiet: cc.imageMode !== modelData.v
                            tone: cc.imageMode === modelData.v ? "purple" : "plain"
                            text: modelData.t
                            onClicked: { cc.imageMode = modelData.v; cc.recutImage() }
                        }
                    }
                    Item { Layout.fillWidth: true }
                    IconButton { iconName: "refresh"; size: 32; tip: qsTr("Cut again"); onClicked: cc.recutImage() }
                    IconButton { iconName: "trash"; size: 32; tip: qsTr("No picture"); onClicked: cc.set("images", []) }
                }
                Label2 { text: qsTr("Sentence audio") }
                RowLayout {
                    spacing: 6
                    IconButton {
                        iconName: player.playbackState === MediaPlayer.PlayingState ? "pause" : "play"
                        enabled: !!cc.d.sentenceAudio
                        onClicked: {
                            if (player.playbackState === MediaPlayer.PlayingState) player.stop()
                            else { player.source = Togu.fileUrl(cc.d.sentenceAudio); player.play() }
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: cc.d.sentenceAudio ? Togu.formatTime(cc.d.start + cc.padStart) + " – " + Togu.formatTime(cc.d.end + cc.padEnd) : (Cards.busy ? qsTr("Cutting…") : qsTr("None"))
                        color: Theme.text2; font.pixelSize: 13
                    }
                    IconButton { iconName: "trash"; size: 32; tip: qsTr("No sentence audio"); onClicked: cc.set("sentenceAudio", "") }
                }
                RowLayout {
                    spacing: 4
                    Text { text: qsTr("Start"); color: Theme.text3; font.pixelSize: 12 }
                    TButton { small: true; quiet: true; tone: "plain"; text: "−0.25"; onClicked: { cc.padStart -= 0.25; cc.recutAudio() } }
                    TButton { small: true; quiet: true; tone: "plain"; text: "+0.25"; onClicked: { cc.padStart += 0.25; cc.recutAudio() } }
                    Item { width: 8 }
                    Text { text: qsTr("End"); color: Theme.text3; font.pixelSize: 12 }
                    TButton { small: true; quiet: true; tone: "plain"; text: "−0.25"; onClicked: { cc.padEnd -= 0.25; cc.recutAudio() } }
                    TButton { small: true; quiet: true; tone: "plain"; text: "+0.25"; onClicked: { cc.padEnd += 0.25; cc.recutAudio() } }
                }
                Label2 { text: qsTr("Word audio"); visible: !!cc.d.expression }
                Text {
                    visible: !!cc.d.expression && cc.wordAudios.length === 0
                    text: qsTr("None found. Point Settings → Card media at your local audio collection.")
                    color: Theme.text3; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: cc.wordAudios.slice(0, 6)
                        delegate: Rectangle {
                            height: 30
                            width: wa.implicitWidth + 40
                            radius: 15
                            color: cc.d.wordAudio === modelData.path ? Theme.purple : Theme.fill
                            Row {
                                anchors.centerIn: parent
                                spacing: 6
                                Icon { name: "volume"; size: 14; color: cc.d.wordAudio === modelData.path ? "white" : Theme.text2 }
                                Text { id: wa; text: modelData.source + (modelData.speaker ? " · " + modelData.speaker : ""); color: cc.d.wordAudio === modelData.path ? "white" : Theme.text; font.pixelSize: 12 }
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: { cc.set("wordAudio", modelData.path); player.source = Togu.fileUrl(modelData.path); player.play() }
                            }
                        }
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }
        // ------------------------------------------------ footer
        Text { id: errorText; visible: text !== ""; color: Theme.pink; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.Wrap }
        RowLayout {
            spacing: 10
            Text {
                visible: cc.confirmEmpty
                text: qsTr("This card has no word, only the sentence. Add it anyway?")
                color: Theme.orange; font.pixelSize: 13
            }
            Item { Layout.fillWidth: true }
            TButton {
                quiet: true; tone: "plain"; iconName: "layers"
                text: qsTr("Add media to last card")
                enabled: !Cards.busy
                onClicked: Cards.addMediaToLast(cc.d)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Puts this audio and picture on the newest note in Anki, e.g. one Yomitan just made")
            }
            TButton {
                iconName: "cardAdd"
                text: cc.confirmEmpty ? qsTr("Add anyway") : qsTr("Add card")
                enabled: !Cards.busy && !cc.isEmpty()
                onClicked: cc.submit(cc.confirmEmpty)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Ctrl+Enter")
            }
        }
    }

    component Label2: Text { color: Theme.text3; font.pixelSize: 11; font.weight: Font.DemiBold; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6 }
}
