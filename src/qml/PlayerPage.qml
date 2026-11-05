import QtQuick
import QtCore
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Effects
import Togu

// The player: video (or cover art for audio), Togu's subtitle layer, controls,
// the line list and the subtitle menu.
FocusScope {
    id: page
    property alias session: session
    property alias mpv: mpv
    property bool sidePanel: false
    property bool controlsShown: true
    property bool primaryHidden: false
    property bool secondaryHidden: Settings.sub2Mode === "off"
    property int hoverIndex: -1
    property Item hoverItem: null
    property bool pausedByHover: false
    property bool fullscreen: false
    signal closeRequested()
    signal cardRequested(var entry, var line, string surface)
    signal fullscreenToggled()

    function open(path, itemId) {
        session.open(path, itemId || 0)
        forceActiveFocus()
    }

    Rectangle { anchors.fill: parent; color: "black" }

    MpvItem {
        id: mpv
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: parent.width - (page.sidePanel ? side.width : 0)
        Behavior on width { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
    }

    PlaybackSession {
        id: session
        player: mpv
        onToast: (text, tone) => toasts.show(text, tone)
        onSyncStateChanged: if (syncState === "applied" || syncState === "inSync") syncHide.restart()
        // Straight on to the next episode.
        onFinished: {
            const next = Library.nextItem(itemId)
            if (next) { const it = Library.item(next); session.openAt(it.path, next, 0) }
        }
    }
    Timer { id: syncHide; interval: 4000; onTriggered: if (session.syncState !== "offered" && session.syncState !== "running") session.dismissSync() }

    // Cover art when playing audio.
    Item {
        anchors.fill: mpv
        visible: session.kind === "audio"
        Rectangle { anchors.fill: parent; gradient: Gradient { GradientStop { position: 0; color: Theme.bgApp } GradientStop { position: 1; color: "#05001a" } } }
        Image {
            id: cover
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -parent.height * 0.12
            width: Math.min(parent.width, parent.height) * 0.42
            height: width
            fillMode: Image.PreserveAspectCrop
            source: session.coverUrl
            visible: status === Image.Ready
            layer.enabled: true
            layer.effect: MultiEffect { shadowEnabled: true; shadowBlur: 1; shadowOpacity: 0.6 }
        }
        Icon { anchors.centerIn: cover; visible: !cover.visible; name: "music"; size: 96; color: Theme.text3 }
    }

    // ---------------------------------------------------------- mouse: show controls
    MouseArea {
        id: stage
        anchors.fill: mpv
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPositionChanged: page.poke()
        onClicked: mouse => {
            if (mouse.button === Qt.RightButton) { subMenu.open(); return }
            if (popup.visible) { page.closePopup(); return }
            mpv.paused = !mpv.paused
        }
        onDoubleClicked: page.fullscreenToggled()
        cursorShape: page.controlsShown ? Qt.ArrowCursor : Qt.BlankCursor
        onWheel: wheel => mpv.volume = mpv.volume + (wheel.angleDelta.y > 0 ? 5 : -5)
    }
    function poke() {
        // Keys go to the player whenever the pointer is over it and nothing else wants them.
        if (!page.activeFocus && !subMenu.visible) page.forceActiveFocus()
        controlsShown = true
        hideTimer.restart()
    }
    Timer {
        id: hideTimer
        interval: 2600
        onTriggered: if (!mpv.paused && !controls.hovered && !topBar.hovered && !subMenu.visible) page.controlsShown = false
    }

    // ---------------------------------------------------------- subtitles
    readonly property real videoScale: Math.max(0.55, Math.min(mpv.width / 1280, mpv.height / 720))
    readonly property real subSize: Settings.subSize * videoScale

    Item {
        id: subLayer
        anchors.fill: mpv
        // secondary line
        Text {
            id: sub2
            visible: !page.secondaryHidden && session.currentText2 !== "" && !(Settings.sub2Mode === "hover" && !subHover.hovered && !sub2Hover.hovered)
            text: session.currentText2
            width: Math.min(implicitWidth, parent.width * 0.86)
            anchors.horizontalCenter: parent.horizontalCenter
            y: Settings.sub2Position === "top" ? parent.height * 0.05 : primary.y + primary.height + 6
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font.family: Theme.sans
            font.pixelSize: Settings.sub2Size * page.videoScale
            font.weight: Font.Medium
            color: "#f0f0ff"
            style: Text.Outline
            styleColor: "black"
            layer.enabled: Settings.sub2Mode === "blur" && !sub2Hover.hovered
            layer.effect: MultiEffect { blurEnabled: true; blur: 0.8; blurMax: 24 }
            HoverHandler { id: sub2Hover }
        }
        Rectangle {
            visible: primary.visible && Settings.subBackground > 0
            anchors.fill: primary
            anchors.margins: -12
            radius: 10
            color: Qt.rgba(0, 0, 0, Settings.subBackground)
        }
        TokenLine {
            id: primary
            visible: !page.primaryHidden && session.currentTokens.length > 0
            tokens: session.currentTokens
            fontSize: page.subSize
            fontFamily: Settings.subFont
            fontWeight: Settings.subWeight
            outline: Settings.subOutline
            maxWidth: parent.width * 0.9
            anchors.horizontalCenter: parent.horizontalCenter
            y: parent.height * (1 - Settings.subBottom) - height - (page.controlsShown ? 70 : 0)
            Behavior on y { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
            activeIndex: popup.visible && popup.lineIndex === session.currentIndex ? popup.tokenIndex : -1
            layer.enabled: Settings.subBlur && !subHover.hovered
            layer.effect: MultiEffect { blurEnabled: true; blur: 0.9; blurMax: 32 }
            HoverHandler {
                id: subHover
                onHoveredChanged: {
                    if (hovered && Settings.pauseOnHover && !mpv.paused) { mpv.paused = true; page.pausedByHover = true }
                    else if (!hovered && page.pausedByHover && !popup.hovered) resumeTimer.restart()
                }
            }
            onWordEntered: (i, item) => { if (Settings.lookupMode === "hover") page.lookupToken(session.currentIndex, i, item, primary) }
            onWordClicked: (i, item, mouse) => {
                if (mouse.button === Qt.MiddleButton) { page.cycleStatus(session.currentTokens[i].k) ; return }
                page.lookupToken(session.currentIndex, i, item, primary)
            }
        }
    }
    Timer {
        id: resumeTimer
        interval: 250
        onTriggered: {
            if (page.pausedByHover && !subHover.hovered && !popup.hovered) {
                page.pausedByHover = false
                mpv.paused = false
                page.closePopup()
            }
        }
    }

    // ---------------------------------------------------------- word popup
    WordPopup {
        id: popup
        property int lineIndex: -1
        property int tokenIndex: -1
        property var line: null
        property real anchorX: 0
        property real anchorTop: 0
        property real anchorBottom: 0
        visible: false
        x: Math.max(8, Math.min(page.width - width - 8, anchorX - width / 2))
        // Above the word when it fits, else below it.
        y: anchorTop - height - 6 >= 8 ? anchorTop - height - 6 : Math.min(page.height - height - 8, anchorBottom + 6)
        z: 20
        maxHeight: Math.min(560, page.height * 0.7)
        onStatusChosen: (key, st) => Togu.setStatus(key, st)
        onCardRequested: entry => page.requestCard(entry)
        onQueryRequested: text => { entries = Togu.lookup(text); current = 0 }
        onCloseRequested: page.closePopup()
        onLineRequested: hit => { page.closePopup(); session.openAt(hit.path, hit.item, hit.start) }
        onHoveredChanged: if (!hovered && page.pausedByHover) resumeTimer.restart()
    }
    function lookupToken(lineIndex, tokenIndex, item, container) {
        const toks = session.tokensForLine(lineIndex)
        const tok = toks[tokenIndex]
        if (!tok || !tok.w) return
        if (popup.visible && popup.lineIndex === lineIndex && popup.tokenIndex === tokenIndex) return
        popup.lineIndex = lineIndex
        popup.tokenIndex = tokenIndex
        popup.line = session.line(lineIndex)
        popup.tokenKey = tok.k
        popup.tokenSurface = tok.s
        popup.entries = Togu.lookup(session.textFrom(lineIndex, tokenIndex))
        popup.kanjiList = Togu.kanji(popup.entries.length ? popup.entries[0].expression : tok.s)
        popup.libraryLines = Library.linesWithWord(popup.entries.length ? popup.entries[0].key : tok.k, 8)
            .filter(h => !(h.item === session.itemId && Math.abs(h.start - (popup.line ? popup.line.start - session.offset : -1)) < 0.05))
        popup.current = 0
        const p = item.mapToItem(page, 0, 0)
        popup.anchorX = p.x + item.width / 2
        popup.anchorTop = p.y
        popup.anchorBottom = p.y + item.height
        popup.visible = true
        hoverIndex = tokenIndex
    }
    function closePopup() {
        popup.visible = false
        popup.lineIndex = -1
        popup.tokenIndex = -1
    }
    function requestCard(entry) {
        const line = popup.line || session.line(session.currentIndex >= 0 ? session.currentIndex : session.lastIndex)
        page.cardRequested(entry, line, popup.tokenSurface)
    }
    function cycleStatus(key) {
        if (!key) return
        const order = ["unknown", "learning", "known"]
        const cur = Togu.status(key)
        Togu.setStatus(key, order[(order.indexOf(cur) + 1) % order.length])
    }
    // The word keys act on the hovered word, else on the line's only unknown word.
    function keyTarget() {
        if (popup.visible) return popup.targetKey
        const unknown = session.currentTokens.filter(t => t.w && (t.st === "unknown" || t.st === "tracked"))
        return unknown.length === 1 ? unknown[0].k : ""
    }
    property int cycleAt: -1
    function cycleWords(all) {
        const idx = []
        session.currentTokens.forEach((t, i) => { if (t.w && (all || t.st === "unknown" || t.st === "tracked")) idx.push(i) })
        if (!idx.length) { toasts.show(all ? qsTr("No words in this subtitle") : qsTr("No unknown words in this subtitle"), "grey"); return }
        cycleAt = (cycleAt + 1) % idx.length
        const item = primary.itemAt(idx[cycleAt])
        if (item) lookupToken(session.currentIndex, idx[cycleAt], item, primary)
    }

    // ---------------------------------------------------------- top bar
    Rectangle {
        id: topBar
        readonly property bool hovered: topHover.hovered
        anchors.top: parent.top
        anchors.left: mpv.left
        width: mpv.width
        height: 64
        opacity: page.controlsShown ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 180 } }
        gradient: Gradient { GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0.65) } GradientStop { position: 1; color: "transparent" } }
        HoverHandler { id: topHover }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10
            IconButton { iconName: "back"; color: "white"; tip: qsTr("Back to library (Esc)"); onClicked: page.closeRequested() }
            Text {
                Layout.fillWidth: true
                text: session.title
                color: "white"
                font.pixelSize: 16
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Rectangle {
                visible: session.stats.comprehension !== undefined
                height: 28
                width: compText.implicitWidth + 22
                radius: 14
                color: Theme.toneLow(session.stats.tone || "purple")
                Text {
                    id: compText
                    anchors.centerIn: parent
                    text: (session.stats.comprehension || 0) + "% · " + (session.stats.label || "")
                    color: "white"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
                ToolTip.visible: compHover.hovered
                ToolTip.text: qsTr("%1 known · %2 learning · %3 unknown words in this episode · %4 recommended sentences")
                              .arg(session.stats.known).arg(session.stats.learning).arg(session.stats.unknown).arg(session.stats.recommended)
                HoverHandler { id: compHover }
            }
        }
    }

    // ---------------------------------------------------------- sync banner
    Rectangle {
        id: syncBanner
        visible: session.syncState !== ""
        anchors.horizontalCenter: mpv.horizontalCenter
        y: 72
        z: 5
        height: 44
        width: syncRow.implicitWidth + 28
        radius: 22
        color: Theme.raised
        border.color: Theme.line
        RowLayout {
            id: syncRow
            anchors.centerIn: parent
            spacing: 10
            BusyIndicator { visible: session.syncState === "running"; running: visible; implicitWidth: 22; implicitHeight: 22 }
            Icon {
                visible: session.syncState !== "running"
                name: session.syncState === "failed" ? "warning" : session.syncState === "offered" ? "sync" : "check"
                size: 18
                color: session.syncState === "failed" ? Theme.orange : Theme.teal
            }
            Text { text: session.syncMessage; color: Theme.text; font.pixelSize: 14 }
            TButton { visible: session.syncState === "offered"; small: true; text: qsTr("Apply"); onClicked: session.acceptSync() }
            TButton { visible: session.syncState === "offered" || session.syncState === "failed"; small: true; quiet: true; tone: "plain"; text: qsTr("Dismiss"); onClicked: session.dismissSync() }
        }
    }

    // ---------------------------------------------------------- bottom controls
    Rectangle {
        id: controls
        readonly property bool hovered: ctrlHover.hovered || seek.pressed
        anchors.bottom: parent.bottom
        anchors.left: mpv.left
        width: mpv.width
        height: 96
        opacity: page.controlsShown ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 180 } }
        gradient: Gradient { GradientStop { position: 0; color: "transparent" } GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0.75) } }
        HoverHandler { id: ctrlHover }

        TSlider {
            id: seek
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            y: 30
            from: 0
            to: Math.max(1, mpv.duration)
            value: pressed ? value : mpv.position
            onMoved: mpv.seek(value, false)
            onPressedChanged: if (!pressed) mpv.seek(value, true)
        }
        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.bottomMargin: 8
            spacing: 2
            IconButton { iconName: mpv.paused ? "play" : "pause"; color: "white"; size: 44; tip: qsTr("Play / pause") + KeyMap.hint("pause"); onClicked: mpv.paused = !mpv.paused }
            IconButton { iconName: "prevLine"; color: "white"; tip: qsTr("Previous line") + KeyMap.hint("prevLine"); onClicked: session.prevLine() }
            IconButton { iconName: "replay"; color: "white"; tip: qsTr("Replay line") + KeyMap.hint("replayLine"); onClicked: session.replayLine() }
            IconButton { iconName: "nextLine"; color: "white"; tip: qsTr("Next line") + KeyMap.hint("nextLine"); onClicked: session.nextLine() }
            IconButton { iconName: mpv.muted || mpv.volume === 0 ? "mute" : "volume"; color: "white"; tip: qsTr("Mute") + KeyMap.hint("mute"); onClicked: mpv.muted = !mpv.muted }
            TSlider { Layout.preferredWidth: 90; from: 0; to: 130; value: mpv.volume; onMoved: mpv.volume = value; accent: "white" }
            Text {
                text: Togu.formatTime(mpv.position) + "  /  " + Togu.formatTime(mpv.duration)
                color: "white"
                font.pixelSize: 13
                font.features: { "tnum": 1 }
                Layout.leftMargin: 8
            }
            Item { Layout.fillWidth: true }
            Text {
                visible: session.offset !== 0
                text: (session.offset > 0 ? "+" : "") + session.offset.toFixed(2) + " s" + (session.pinned ? " · " + qsTr("pinned") : "")
                color: Theme.text2
                font.pixelSize: 12
                Layout.rightMargin: 6
            }
            IconButton {
                id: chapterBtn
                visible: mpv.chapters.length > 1
                iconName: "list"
                color: "white"
                tip: qsTr("Chapters")
                onClicked: chapterMenu.popup(chapterBtn, 0, -chapterMenu.implicitHeight)
                TMenu {
                    id: chapterMenu
                    Repeater {
                        model: mpv.chapters
                        MenuItem {
                            text: (modelData.title || qsTr("Chapter %1").arg(index + 1)) + "  ·  " + Togu.formatTime(modelData.time)
                            checkable: true
                            checked: mpv.position >= modelData.time && (index + 1 >= mpv.chapters.length || mpv.position < mpv.chapters[index + 1].time)
                            onTriggered: mpv.seek(modelData.time)
                        }
                    }
                }
            }
            IconButton {
                id: speedBtn
                iconName: "speed"; color: "white"; tip: qsTr("Speed: %1×").arg(mpv.speed.toFixed(2))
                onClicked: speedMenu.popup(speedBtn, 0, -speedMenu.implicitHeight)
                TMenu {
                    id: speedMenu
                    Repeater {
                        model: [0.5, 0.75, 0.85, 0.9, 1.0, 1.1, 1.25, 1.5]
                        MenuItem { text: modelData + "×"; checkable: true; checked: Math.abs(mpv.speed - modelData) < 0.001; onTriggered: mpv.speed = modelData }
                    }
                }
            }
            IconButton {
                iconName: "subtitles"; color: "white"; tip: qsTr("Subtitles and timing"); active: subMenu.visible
                onClicked: subMenu.visible ? subMenu.close() : subMenu.open()
                BusyIndicator { anchors.fill: parent; running: session.loadingSubs; visible: running }
            }
            IconButton { iconName: "list"; color: "white"; tip: qsTr("Line list") + KeyMap.hint("sidePanel"); active: page.sidePanel; onClicked: page.sidePanel = !page.sidePanel }
            IconButton { iconName: "cardAdd"; color: "white"; tip: qsTr("Card from this line") + KeyMap.hint("cardLine"); onClicked: page.cardRequested(null, session.line(session.currentIndex >= 0 ? session.currentIndex : session.lastIndex), "") }
            IconButton { iconName: page.fullscreen ? "unfullscreen" : "fullscreen"; color: "white"; tip: qsTr("Fullscreen") + KeyMap.hint("fullscreen"); onClicked: page.fullscreenToggled() }
        }
    }

    // ---------------------------------------------------------- side panel: line list
    Rectangle {
        id: side
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: Math.min(420, page.width * 0.34)
        color: Theme.bg
        visible: page.sidePanel
        border.color: Theme.line
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 12
                Text { text: qsTr("Lines"); color: Theme.text; font.pixelSize: 16; font.weight: Font.Bold; Layout.fillWidth: true }
                Text { text: session.lineCount; color: Theme.text3; font.pixelSize: 13 }
                IconButton { iconName: "close"; size: 32; onClicked: page.sidePanel = false }
            }
            ListView {
                id: lineList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: session.lines
                spacing: 2
                currentIndex: session.lastIndex
                highlightFollowsCurrentItem: false
                property bool userScrolling: false
                onCurrentIndexChanged: if (!userScrolling && currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Center)
                onMovementStarted: userScrolling = true
                onMovementEnded: scrollReset.restart()
                Timer { id: scrollReset; interval: 3000; onTriggered: lineList.userScrolling = false }
                ScrollBar.vertical: ScrollBar {}
                delegate: Rectangle {
                    id: row
                    required property int index
                    required property double start
                    required property var tokens
                    required property string text2
                    required property bool bookmarked
                    width: lineList.width
                    height: rowCol.implicitHeight + 16
                    color: index === session.currentIndex ? Theme.tint : rowHover.hovered ? Theme.fill : "transparent"
                    HoverHandler { id: rowHover }
                    TapHandler { onTapped: session.seekToLine(row.index) }
                    Row {
                        visible: rowHover.hovered
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 4
                        z: 2
                        IconButton { size: 26; iconName: "cardAdd"; tip: qsTr("Card from this line"); onClicked: page.cardRequested(null, session.line(row.index), "") }
                        IconButton { size: 26; iconName: "queue"; tip: qsTr("Put in the card queue"); onClicked: session.queueLine(row.index, "") }
                        IconButton { size: 26; iconName: row.bookmarked ? "bookmarkOn" : "bookmark"; tip: qsTr("Bookmark"); onClicked: session.toggleBookmark(row.index) }
                    }
                    Column {
                        id: rowCol
                        x: 14
                        y: 8
                        width: parent.width - 28
                        spacing: 2
                        Row {
                            spacing: 8
                            Text { text: Togu.formatTime(row.start); color: Theme.text3; font.pixelSize: 11; font.features: { "tnum": 1 } }
                            Icon { visible: row.bookmarked; name: "bookmarkOn"; size: 12; color: Theme.blue }
                        }

                        TokenLine {
                            tokens: row.tokens
                            fontSize: 18
                            outline: false
                            textColor: Theme.text
                            maxWidth: rowCol.width
                            align: Text.AlignLeft
                            onWordClicked: (i, item, mouse) => page.lookupToken(row.index, i, item, this)
                        }
                        Text {
                            visible: row.text2 !== ""
                            width: parent.width
                            text: row.text2
                            color: Theme.text2
                            font.pixelSize: 13
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }
    }

    // ---------------------------------------------------------- subtitle menu
    Popup {
        id: subMenu
        x: Math.max(8, mpv.width - width - 16)
        y: page.height - height - 104
        width: 400
        height: Math.min(page.height - 140, subCol.implicitHeight + 32)
        padding: 16
        background: Rectangle { radius: 20; color: Theme.raised; border.color: Theme.line }
        onClosed: page.forceActiveFocus()
        Flickable {
            anchors.fill: parent
            contentHeight: subCol.implicitHeight
            clip: true
            ScrollBar.vertical: ScrollBar {}
            ColumnLayout {
                id: subCol
                width: parent.width
                spacing: 10
                Text { text: qsTr("Japanese subtitles"); color: Theme.text; font.pixelSize: 15; font.weight: Font.Bold }
                Repeater {
                    model: [{ id: "none", label: qsTr("None"), usable: true }].concat(session.subtitleSources)
                    delegate: SourceRow { src: modelData; selected: (session.primaryId || "none") === modelData.id; onPicked: session.selectPrimary(modelData.id) }
                }
                RowLayout {
                    spacing: 8
                    TButton { small: true; quiet: true; tone: "plain"; iconName: "upload"; text: qsTr("Import…"); onClicked: { importDialog.secondary = false; importDialog.open() } }
                    TButton { visible: Settings.jimakuKey !== ""; small: true; quiet: true; tone: "plain"; iconName: "search"; text: qsTr("Search Jimaku…"); onClicked: { subMenu.close(); jimaku.openFor(session) } }
                }
                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.line }
                Text { text: qsTr("Timing"); color: Theme.text; font.pixelSize: 15; font.weight: Font.Bold }
                RowLayout {
                    spacing: 6
                    IconButton { iconName: "minus"; size: 34; tip: qsTr("Earlier by %1 ms").arg(Math.round(Settings.offsetStep * 1000)) + KeyMap.hint("offsetBack"); onClicked: session.adjustOffset(-Settings.offsetStep) }
                    Text {
                        text: (session.offset >= 0 ? "+" : "") + Math.round(session.offset * 1000) + " ms"
                        color: Theme.text; font.pixelSize: 16; font.features: { "tnum": 1 }
                        Layout.preferredWidth: 90
                        horizontalAlignment: Text.AlignHCenter
                    }
                    IconButton { iconName: "plus"; size: 34; tip: qsTr("Later by %1 ms").arg(Math.round(Settings.offsetStep * 1000)) + KeyMap.hint("offsetForward"); onClicked: session.adjustOffset(Settings.offsetStep) }
                    TButton { small: true; quiet: true; tone: "plain"; text: "−" + Settings.offsetStepBig + " s"; onClicked: session.adjustOffset(-Settings.offsetStepBig) }
                    TButton { small: true; quiet: true; tone: "plain"; text: "+" + Settings.offsetStepBig + " s"; onClicked: session.adjustOffset(Settings.offsetStepBig) }
                }
                RowLayout {
                    spacing: 8
                    TButton { small: true; iconName: "target"; text: qsTr("Line starts now"); onClicked: session.lineStartsNow()
                        ToolTip.visible: hovered; ToolTip.text: qsTr("Press as the next line is spoken") + KeyMap.hint("lineStartsNow") }
                    TButton { small: true; tone: "blue"; iconName: "sync"; text: qsTr("Sync now"); enabled: session.lineCount > 0; onClicked: session.runSync() }
                    TButton { small: true; quiet: true; tone: "plain"; text: qsTr("Reset"); onClicked: session.setOffset(0) }
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: Theme.text3
                    font.pixelSize: 12
                    text: session.pinned ? qsTr("Set by hand, so auto-sync leaves this video alone until you press Sync now.")
                                         : qsTr("Auto-sync checks the timing once when subtitles are first paired with a video.")
                }
                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.line }
                Text { text: qsTr("Second subtitles"); color: Theme.text; font.pixelSize: 15; font.weight: Font.Bold }
                Repeater {
                    model: [{ id: "none", label: qsTr("None"), usable: true }].concat(session.subtitleSources)
                    delegate: SourceRow { src: modelData; selected: (session.secondaryId || "none") === modelData.id; onPicked: session.selectSecondary(modelData.id) }
                }
                RowLayout {
                    spacing: 8
                    TButton { small: true; quiet: true; tone: "plain"; iconName: "upload"; text: qsTr("Import…"); onClicked: { importDialog.secondary = true; importDialog.open() } }
                    TCombo {
                        Layout.fillWidth: true
                        model: [{ text: qsTr("Show on hover"), value: "hover" }, { text: qsTr("Always show"), value: "always" },
                                { text: qsTr("Blurred until hovered"), value: "blur" }, { text: qsTr("Hidden"), value: "off" }]
                        Component.onCompleted: currentIndex = indexOfValue(Settings.sub2Mode)
                        onActivated: Settings.set("sub2Mode", currentValue)
                    }
                }
                RowLayout {
                    visible: session.secondaryId !== ""
                    spacing: 6
                    Text { text: qsTr("Offset"); color: Theme.text2; font.pixelSize: 13 }
                    IconButton { iconName: "minus"; size: 30; onClicked: session.adjustOffset(-Settings.offsetStep, true) }
                    Text { text: Math.round(session.offset2 * 1000) + " ms"; color: Theme.text; font.pixelSize: 13 }
                    IconButton { iconName: "plus"; size: 30; onClicked: session.adjustOffset(Settings.offsetStep, true) }
                }
            }
        }
    }
    component SourceRow: Rectangle {
        id: sr
        property var src
        property bool selected: false
        signal picked()
        Layout.fillWidth: true
        implicitHeight: 38
        radius: 10
        color: selected ? Theme.tint : srHover.hovered ? Theme.fill : "transparent"
        opacity: src.usable === false ? 0.45 : 1
        HoverHandler { id: srHover }
        TapHandler { enabled: sr.src.usable !== false; onTapped: sr.picked() }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            Icon { name: sr.src.kind === "track" ? "film" : sr.src.id === "none" ? "close" : "subtitles"; size: 16; color: sr.selected ? Theme.purple : Theme.text2 }
            Text { text: sr.src.label; color: Theme.text; font.pixelSize: 13; elide: Text.ElideMiddle; Layout.fillWidth: true }
            Chip { visible: !!sr.src.japanese && sr.src.id !== "none"; text: "JA"; tone: "teal"; fontSize: 10 }
            Icon { visible: sr.selected; name: "check"; size: 16; color: Theme.purple }
        }
    }

    FileDialog {
        id: importDialog
        property bool secondary: false
        title: qsTr("Import subtitles")
        // Next to the video, where subtitle files usually are.
        currentFolder: session.path && !session.path.includes("://") ? Togu.fileUrl(session.path.substring(0, session.path.lastIndexOf("/"))) : StandardPaths.writableLocation(StandardPaths.DownloadLocation)
        nameFilters: [qsTr("Subtitles (*.ass *.ssa *.srt *.vtt *.lrc *.zip *.rar *.7z)"), qsTr("All files (*)")]
        onAccepted: session.importSubtitle(selectedFile, secondary)
    }

    JimakuDialog { id: jimaku; anchors.centerIn: parent }

    Toasts { id: toasts; anchors.horizontalCenter: mpv.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 120; z: 30 }

    // ---------------------------------------------------------- keys
    Keys.onPressed: event => {
        let hit = true
        if (event.key === Qt.Key_Escape) {
            if (popup.visible) closePopup()
            else if (page.fullscreen) page.fullscreenToggled()
            else page.closeRequested()
            event.accepted = true
            return
        }
        const cur = session.currentIndex >= 0 ? session.currentIndex : session.lastIndex
        switch (KeyMap.action(event)) {
        case "pause": mpv.paused = !mpv.paused; break
        case "prevLine": session.prevLine(); break
        case "nextLine": session.nextLine(); break
        case "replayLine": session.replayLine(); break
        case "seekBack": mpv.seekRelative(-Settings.seekStep); break
        case "seekForward": mpv.seekRelative(Settings.seekStep); break
        case "slower": mpv.speed = mpv.speed - Settings.speedStep; break
        case "faster": mpv.speed = mpv.speed + Settings.speedStep; break
        case "normalSpeed": mpv.speed = 1.0; break
        case "frameBack": mpv.frameStep(true); break
        case "frameForward": mpv.frameStep(false); break
        case "mute": mpv.muted = !mpv.muted; break
        case "fullscreen": page.fullscreenToggled(); break
        case "togglePrimary":
            primaryHidden = !primaryHidden
            toasts.show(primaryHidden ? qsTr("Japanese subtitles hidden") : qsTr("Japanese subtitles visible"), "blue"); break
        case "toggleSecondary":
            secondaryHidden = !secondaryHidden
            toasts.show(secondaryHidden ? qsTr("Second subtitles hidden") : qsTr("Second subtitles visible"), "blue"); break
        case "toggleAll":
            const anyShown = !primaryHidden || !secondaryHidden
            primaryHidden = anyShown; secondaryHidden = anyShown
            toasts.show(anyShown ? qsTr("Subtitles hidden") : qsTr("Subtitles visible"), "blue"); break
        case "copyLine": Togu.copyText(session.currentText); toasts.show(qsTr("Line copied"), "blue"); break
        case "sidePanel": sidePanel = !sidePanel; break
        case "bookmark": session.toggleBookmark(cur); break
        case "nextWord": cycleWords(false); break
        case "prevWord": cycleWords(true); break
        case "subSmaller": Settings.set("subSize", Math.max(16, Settings.subSize - 2)); break
        case "subLarger": Settings.set("subSize", Math.min(120, Settings.subSize + 2)); break
        case "offsetBack": session.adjustOffset(-Settings.offsetStep, false); break
        case "offsetForward": session.adjustOffset(Settings.offsetStep, false); break
        case "offset2Back": session.adjustOffset(-Settings.offsetStep, true); break
        case "offset2Forward": session.adjustOffset(Settings.offsetStep, true); break
        case "lineStartsNow": session.lineStartsNow(); break
        case "autoSync": session.runSync(); break
        case "unknown": if (keyTarget()) Togu.setStatus(keyTarget(), "unknown"); break
        case "tracked": if (keyTarget()) Togu.setStatus(keyTarget(), Togu.status(keyTarget()) === "tracked" ? "unknown" : "tracked"); break
        case "learning": if (keyTarget()) Togu.setStatus(keyTarget(), "learning"); break
        case "known": if (keyTarget()) Togu.setStatus(keyTarget(), "known"); break
        case "ignored": if (keyTarget()) Togu.setStatus(keyTarget(), "ignored"); break
        case "cardWord": if (popup.visible) requestCard(popup.entry); break
        case "cardLine": page.cardRequested(popup.visible ? popup.entry : null, session.line(cur), popup.visible ? popup.tokenSurface : ""); break
        case "queueLine": session.queueLine(cur, popup.visible ? popup.targetKey : ""); break
        default: hit = false
        }
        if (hit) { event.accepted = true; poke() }
    }
}
