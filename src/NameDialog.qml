import QtQuick
import QtQuick.Controls

// Asks for a name (new note, new folder, rename) or for a yes or no
// (move to trash), styled like the unsaved-changes dialog.
Dialog {
    id: root
    objectName: "nameDialog"

    property bool darkMode: true
    property color textColor: darkMode ? "#d0d0d0" : "#42464c"
    property color strongTextColor: darkMode ? "#eeeeee" : "#222324"
    property color activeButtonColor: "#428bca"
    property color selectionFill: "#428bca"
    property color selectionTextColor: "#ffffff"
    property int containerWidth: 420
    property int containerHeight: 320
    property real textScale: 1

    property string heading: ""
    property string message: ""
    property string acceptText: "OK"
    property bool askName: true
    property var pendingCallback: null

    function askForName(headingText, initialName, buttonText, callback) {
        heading = headingText;
        message = "";
        acceptText = buttonText;
        askName = true;
        nameField.text = initialName;
        pendingCallback = callback;
        open();
    }

    function confirm(headingText, messageText, buttonText, callback) {
        heading = headingText;
        message = messageText;
        acceptText = buttonText;
        askName = false;
        pendingCallback = callback;
        open();
    }

    function finish() {
        var callback = pendingCallback;
        var name = nameField.text;
        pendingCallback = null;
        close();
        if (callback && (!askName || name.trim().length > 0))
            callback(name);
    }

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    width: Math.min(Math.round(420 * textScale), containerWidth - 48)
    x: Math.round((containerWidth - width) / 2)
    y: Math.round((containerHeight - height) / 3)
    padding: 20

    onOpened: {
        if (askName) {
            nameField.forceActiveFocus();
            nameField.selectAll();
        } else {
            acceptButton.forceActiveFocus();
        }
    }
    onClosed: pendingCallback = null

    background: Rectangle {
        color: root.darkMode ? "#1a1a1a" : "#ffffff"
        border.color: root.darkMode ? "#343434" : "#d8d8d8"
        radius: 0
    }

    contentItem: Column {
        spacing: 12

        Label {
            text: root.heading
            color: root.strongTextColor
            font.family: "IBM Plex Mono"
            font.pixelSize: Math.round(16 * root.textScale)
            font.bold: true
        }

        Label {
            width: parent.width
            visible: root.message.length > 0
            text: root.message
            color: root.textColor
            wrapMode: Text.Wrap
            font.family: "IBM Plex Mono"
            font.pixelSize: Math.round(13 * root.textScale)
        }

        Rectangle {
            width: parent.width
            height: Math.round(36 * root.textScale)
            visible: root.askName
            color: root.darkMode ? "#202020" : "#f6f6f6"
            border.color: nameField.activeFocus ? root.activeButtonColor
                                                : (root.darkMode ? "#424242" : "#c8c8c8")

            TextInput {
                id: nameField
                objectName: "nameField"
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                verticalAlignment: TextInput.AlignVCenter
                clip: true
                selectByMouse: true
                color: root.strongTextColor
                selectionColor: root.selectionFill
                selectedTextColor: root.selectionTextColor
                font.family: "IBM Plex Mono"
                font.pixelSize: Math.round(14 * root.textScale)
                Keys.onReturnPressed: root.finish()
                Keys.onEnterPressed: root.finish()
            }
        }
    }

    footer: Item {
        implicitHeight: buttons.implicitHeight + 20

        Row {
            id: buttons
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            SquareDialogButton {
                text: "Cancel"
                darkMode: root.darkMode
                textScale: root.textScale
                labelColor: root.textColor
                onClicked: root.close()
            }

            SquareDialogButton {
                id: acceptButton
                objectName: "nameDialogAccept"
                text: root.acceptText
                primary: true
                darkMode: root.darkMode
                textScale: root.textScale
                activeColor: root.activeButtonColor
                enabled: !root.askName || nameField.text.trim().length > 0
                onClicked: root.finish()
            }
        }
    }
}
