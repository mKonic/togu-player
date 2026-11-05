import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Togu

// YouTube in Togu's skin. youtube.com only signs in and carries the requests (YtBridge).
Item {
    id: yp
    property var bridge
    signal play(var video)
    property string tab: "home"        // home, subscriptions, search, channel
    property string query: ""
    property var channel: null         // {id, name} for the channel tab
    property var videos: []
    property string next: ""
    property bool loading: false
    property string failed: ""
    property bool showHidden: false
    property int generation: 0
    property var signInWindow: null

    // Japanese if the title has kana, or kanji with a kana channel name; hangul or none of it is not.
    function isJapanese(v) {
        const kana = /[぀-ヿ]/
        const hangul = /[가-힯]/
        if (hangul.test(v.title)) return false
        if (kana.test(v.title)) return true
        return /[一-鿿]/.test(v.title) && kana.test(v.channel)
    }
    readonly property var shown: showHidden ? videos : videos.filter(isJapanese)
    readonly property int hiddenCount: videos.length - videos.filter(isJapanese).length

    function load(more) {
        if (!bridge || !bridge.signedIn) return
        if (more && (!next || loading)) return
        const gen = more ? generation : ++generation
        if (!more) { videos = []; next = ""; failed = "" }
        loading = true
        const fn = tab === "search" ? "search" : tab === "channel" ? "channel" : tab
        const args = tab === "search" ? [query, more ? next : ""] : tab === "channel" ? [channel.id, more ? next : ""] : [more ? next : ""]
        bridge.call(fn, args, r => {
            if (gen !== generation) return
            const have = new Set(videos.map(v => v.id))
            let got = r.videos.filter(v => !have.has(v.id))
            if (tab === "channel") got = got.map(v => Object.assign(v, { channel: v.channel || channel.name, channelId: v.channelId || channel.id }))
            videos = videos.concat(got)
            next = r.next || ""
            loading = false
            // Keep going while a page is mostly filtered out.
            if (!showHidden && shown.length < 12 && next) load(true)
        }, e => {
            if (gen !== generation) return
            loading = false
            failed = e
        })
    }
    function go(t) { tab = t; load(false) }
    function openChannel(v) { channel = { id: v.channelId, name: v.channel }; go("channel") }
    function signIn() {
        if (signInWindow) { signInWindow.raise(); return }
        const c = Qt.createComponent("YtSignIn.qml")
        signInWindow = c.createObject(null, { bridge: bridge })
        signInWindow.finished.connect(() => {
            signInWindow.destroy()
            signInWindow = null
            bridge.reload()
        })
        signInWindow.show()
    }

    Connections {
        target: yp.bridge
        function onSessionChanged() { if (yp.bridge.signedIn && yp.videos.length === 0 && !yp.loading) yp.load(false) }
    }
    Component.onCompleted: if (bridge && bridge.signedIn) load(false)

    // ------------------------------------------------------------ not ready / signed out
    Column {
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 60)
        spacing: 16
        visible: !yp.bridge || !yp.bridge.ready || !yp.bridge.signedIn
        BusyIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            running: visible
            visible: yp.bridge && !yp.bridge.ready && !yp.bridge.error
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            visible: !!yp.bridge && yp.bridge.error !== "" && !yp.bridge.ready
            text: qsTr("Couldn't reach YouTube: %1").arg(yp.bridge ? yp.bridge.error : "")
            color: Theme.text2; font.pixelSize: 14; wrapMode: Text.Wrap
        }
        TButton {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: !!yp.bridge && yp.bridge.error !== "" && !yp.bridge.ready
            text: qsTr("Try again"); onClicked: yp.bridge.reload()
        }
        TButton {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: !!yp.bridge && yp.bridge.ready && !yp.bridge.signedIn
            iconName: "user"
            text: qsTr("Sign in to YouTube")
            onClicked: yp.signIn()
        }
    }

    // ------------------------------------------------------------ signed in
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        anchors.bottomMargin: 0
        spacing: 14
        visible: !!yp.bridge && yp.bridge.signedIn

        RowLayout {
            spacing: 10
            IconButton { visible: yp.tab === "channel"; iconName: "back"; tip: qsTr("Back"); onClicked: yp.go("home") }
            Text {
                text: yp.tab === "channel" ? yp.channel.name : qsTr("YouTube")
                color: Theme.text; font.pixelSize: 28; font.weight: Font.Bold; font.family: yp.tab === "channel" ? Theme.jp : Theme.round
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            TField {
                id: searchField
                Layout.preferredWidth: 300
                placeholderText: qsTr("Search YouTube")
                onAccepted: if (text.trim()) { yp.query = text.trim(); yp.go("search") }
            }
            IconButton {
                id: accountBtn
                iconName: "user"
                tip: yp.bridge ? yp.bridge.accountName : ""
                onClicked: accountMenu.popup(accountBtn, 0, accountBtn.height)
                TMenu {
                    id: accountMenu
                    MenuItem { enabled: false; text: yp.bridge ? yp.bridge.accountName : "" }
                    MenuItem { text: qsTr("Sign out of YouTube"); onTriggered: { yp.videos = []; yp.bridge.signOut() } }
                }
            }
        }
        RowLayout {
            spacing: 6
            visible: yp.tab !== "channel"
            Repeater {
                model: [{ id: "home", t: qsTr("Home") }, { id: "subscriptions", t: qsTr("Subscriptions") }]
                    .concat(yp.tab === "search" ? [{ id: "search", t: qsTr("“%1”").arg(yp.query) }] : [])
                TButton {
                    small: true
                    quiet: yp.tab !== modelData.id
                    tone: yp.tab === modelData.id ? "purple" : "plain"
                    text: modelData.t
                    onClicked: if (modelData.id !== yp.tab) yp.go(modelData.id)
                }
            }
            Item { Layout.fillWidth: true }
            TButton {
                visible: yp.hiddenCount > 0
                small: true; quiet: true; tone: "plain"
                text: yp.showHidden ? qsTr("Hide non-Japanese") : qsTr("Show %n hidden", "", yp.hiddenCount)
                onClicked: yp.showHidden = !yp.showHidden
            }
        }
        Text {
            visible: yp.failed !== ""
            text: qsTr("YouTube didn't answer: %1").arg(yp.failed)
            color: Theme.pink; font.pixelSize: 13; wrapMode: Text.Wrap; Layout.fillWidth: true
        }
        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            readonly property int cols: Math.max(1, Math.floor(width / 300))
            cellWidth: Math.floor(width / cols)
            cellHeight: cellWidth * 9 / 16 + 110
            model: yp.shown
            ScrollBar.vertical: ScrollBar {}
            onAtYEndChanged: if (atYEnd) yp.load(true)
            delegate: Item {
                width: grid.cellWidth
                height: grid.cellHeight
                VideoCard {
                    x: 8; width: parent.width - 16
                    video: modelData
                    onClicked: yp.play(modelData)
                    onChannelClicked: yp.openChannel(modelData)
                }
            }
            footer: Item {
                width: grid.width
                height: 60
                BusyIndicator { anchors.centerIn: parent; running: yp.loading; visible: running }
                Text {
                    anchors.centerIn: parent
                    visible: !yp.loading && yp.shown.length === 0 && yp.failed === ""
                    text: qsTr("Nothing in Japanese here yet.")
                    color: Theme.text3; font.pixelSize: 13
                }
            }
        }
    }
}
