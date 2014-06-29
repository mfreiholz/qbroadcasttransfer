import QtQuick 2.0
import QtQuick.Controls 1.2
import QtQuick.Dialogs 1.2

Item {
    id: main
    visible: true
    width: 500
    height: 250

    function updateUi() {
        if (clientsView.count > 0) {
            selectFilesButton.enabled = true;
        } else {
            selectFilesButton.enabled = false;
        }
    }

    ListView {
        id: clientsView
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: findClientsButton.top
        anchors.margins: 3
        model: serverLogic.serverModel
        header: Component {
            id: clientsViewHeader
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                height: headerText.paintedHeight + 12
                color: "#000000"
                border.width: 1
                border.color: "#000000"
                radius: 3
                Text {
                    id: headerText
                    anchors.fill: parent
                    anchors.margins: 6
                    text: "Connected clients"
                    color: "#ffffff"
                }
            }
        }
        delegate: Component {
            id: clientsViewDelegate
            Item {
                anchors.left: parent.left
                anchors.right: parent.right
                height: clientsViewDelegateText.paintedHeight + 6
                Text {
                    id: clientsViewDelegateText
                    anchors.fill: parent
                    anchors.margins: 3
                    text: address + ":" + tcpport
                    MouseArea {
                        anchors.fill: parent
                        onClicked: clientsView.currentIndex = index
                    }
                }
            }
        }
        highlight: Component {
            Rectangle {
                color: "lightsteelblue"
                border.color: "steelblue"
                border.width: 1
            }
        }
    }

    Button {
        id: findClientsButton
        anchors.bottom: main.bottom
        anchors.left: parent.left
        anchors.margins: 3
        text: "Find clients"
        onClicked: serverLogic.server.broadcastHello()
    }

    Button {
        id: disconnectAllButton
        anchors.bottom: main.bottom
        anchors.left: findClientsButton.right
        anchors.margins: 3
        text: "Disconnect clients"
        onClicked: serverLogic.server.disconnectFromClients()
    }

    Button {
        id: selectFilesButton
        anchors.bottom: main.bottom
        anchors.left: disconnectAllButton.right
        anchors.margins: 3
        text: "Select files..."
        onClicked: fileDialog.open()
        FileDialog {
            id: fileDialog
            selectExisting: true
            selectFolder: false
            selectMultiple: true
            title: "Choose files to transfer..."
            onAccepted: serverLogic.setFiles(fileDialog.fileUrls)
            onRejected: serverLogic.setFiles(fileDialog.fileUrls)
        }
    }
}
