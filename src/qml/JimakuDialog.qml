import QtQuick
import QtQuick.Controls.Basic

Popup {
    id: d
    function openFor(session) { open() }
    width: 300; height: 120
    background: Rectangle { radius: 20; color: Theme.raised }
}
