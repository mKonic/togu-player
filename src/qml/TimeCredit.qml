import QtQuick
import Togu

// Credits reading time to Stats while the page is in use: the app is in front and
// something happened (a page turn, a hover, a key) in the last two minutes.
Timer {
    id: tc
    property string kind
    property string path
    property bool active: true
    property real lastPoke: 0
    function poke() { lastPoke = Date.now() }
    interval: 15000
    repeat: true
    running: active && path !== "" && Qt.application.state === Qt.ApplicationActive
    onTriggered: if (Date.now() - lastPoke < 120000) Stats.addTime(kind, path, interval / 1000)
}
