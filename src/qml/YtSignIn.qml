import QtQuick
import QtQuick.Window
import QtWebEngine
import Togu

// Google's own sign-in page in YouTube's profile. Closes by itself once YouTube is reached.
Window {
    id: w
    property var bridge
    title: qsTr("Sign in to YouTube — Togu")
    width: 520
    height: 720
    color: "white"
    signal finished()
    onClosing: finished()

    WebEngineView {
        id: view
        anchors.fill: parent
        profile: w.bridge.profile
        url: "https://accounts.google.com/ServiceLogin?service=youtube&continue=https%3A%2F%2Fwww.youtube.com%2Fsignin%3Faction_handle_signin%3Dtrue%26next%3D%252F"
        onUrlChanged: {
            const u = String(url)
            if (u.startsWith("https://www.youtube.com/") && !u.includes("/signin")) w.close()
        }
        onNewWindowRequested: request => view.url = request.requestedUrl
    }
}
