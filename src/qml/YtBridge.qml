import QtQuick
import QtWebEngine
import Togu

// YouTube's session: a profile of its own (kept between runs) and a hidden youtube.com page that every
// request goes through. Google refuses sign-in when the user agent hides "QtWebEngine", so it stays.
Item {
    id: yb
    visible: false
    property bool ready: false
    property bool signedIn: false
    property string accountName: ""
    property string accountAvatar: ""
    property string error: ""
    property int nextId: 1
    property var pending: ({})
    signal sessionChanged()

    readonly property alias profile: ytProfile
    WebEngineProfile {
        id: ytProfile
        storageName: "youtube"
        offTheRecord: false
        persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies
        httpCacheType: WebEngineProfile.DiskHttpCache
    }

    // Calls fn(...args) in the page; done(data) on success, fail(message) otherwise.
    function call(fn, args, done, fail) {
        if (!ready) { if (fail) fail(qsTr("YouTube isn't ready yet")); return }
        const id = String(nextId++)
        const p = Object.assign({}, pending)
        p[id] = { done: done, fail: fail }
        pending = p
        page.runJavaScript("window.toguYt.run(" + JSON.stringify(id) + "," + JSON.stringify(fn) + "," + JSON.stringify(args || []) + ")")
    }
    function reload() {
        ready = false
        page.url = "https://www.youtube.com/feed/library?hl=ja&gl=JP"
        page.reload()
    }
    function refreshState() {
        call("state", [], s => {
            signedIn = s.signedIn
            accountName = s.name || ""
            accountAvatar = s.avatar || ""
            sessionChanged()
        }, e => { error = e; sessionChanged() })
    }
    function signOut() {
        ytProfile.cookieStore.deleteAllCookies()
        ytProfile.clearHttpCache()
        signedIn = false
        accountName = ""
        accountAvatar = ""
        sessionChanged()
        reload()
    }

    WebEngineView {
        id: page
        width: 800; height: 600
        profile: ytProfile
        audioMuted: true
        settings.playbackRequiresUserGesture: true
        // A light youtube.com page: it carries ytcfg without starting a video.
        url: "https://www.youtube.com/feed/library?hl=ja&gl=JP"
        onLoadingChanged: info => {
            if (info.status === WebEngineView.LoadSucceededStatus) runJavaScript(Togu.resourceText(":/qt/qml/Togu/assets/yt.js"))
            else if (info.status === WebEngineView.LoadFailedStatus) { yb.error = info.errorString; yb.sessionChanged() }
        }
        onJavaScriptConsoleMessage: (level, message) => {
            if (!message.startsWith("togu-yt:")) return
            const rest = message.slice(8)
            const colon = rest.indexOf(":")
            const id = rest.slice(0, colon)
            const r = JSON.parse(rest.slice(colon + 1))
            if (id === "ready") { yb.ready = true; yb.error = ""; yb.refreshState(); return }
            const cb = yb.pending[id]
            if (!cb) return
            const p = Object.assign({}, yb.pending)
            delete p[id]
            yb.pending = p
            if (r.ok) { if (cb.done) cb.done(r.data) }
            else if (cb.fail) cb.fail(r.error)
        }
    }
}
