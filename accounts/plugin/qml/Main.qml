import QtQuick 2.9
import Ubuntu.OnlineAccounts.Plugin 1.0

Flickable {
    id: root

    property int keyboardSize: Qt.inputMethod.visible ? Qt.inputMethod.keyboardRectangle.height : 0
    contentHeight: loader.item.height + keyboardSize

    signal finished

    Loader {
        id: loader
        anchors.fill: parent
        sourceComponent: newAccountComponent

        Connections {
            target: loader.item
            onFinished: root.finished()
        }
    }

    Component {
        id: newAccountComponent
        NewAccount {}
    }
}
