import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

Item {
    id: lib
    property string kind: "video"
    signal play(string path, int itemId)
    TButton { anchors.centerIn: parent; text: qsTr("Open file…"); iconName: "folder"; onClicked: fd.open() }
    FileDialog { id: fd; onAccepted: lib.play(Togu.localPath(selectedFile), 0) }
}
