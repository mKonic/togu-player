import QtQuick

Image {
    id: root
    property string name: "info"
    property color color: Theme.text
    property real size: 20
    property real weight: 2
    width: size
    height: size
    sourceSize: Qt.size(size * 2, size * 2)
    smooth: true
    source: Icons.svg(name, color, weight)
}
