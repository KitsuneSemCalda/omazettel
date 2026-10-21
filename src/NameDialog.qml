import QtQuick
import QtQuick.Controls

// Asks for a name (new note, new folder, rename) or for a yes or no
// (move to trash), styled like the unsaved-changes dialog.
Dialog {
    id: root
    objectName: "nameDialog"

    property bool darkMode: true
    property color raisedColor: darkMode ? "#1a1a1a" : "#ffffff"
    property color borderColor: darkMode ? "#343434" : "#d8d8d8"
    property color fieldColor: darkMode ? "#202020" : "#f6f6f6"
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
    // New notes: [{name, needsTitle}] to pick from; empty hides the picker.
    property var templates: []
    property int templateIndex: 0
    readonly property var chosenTemplate: templates.length > 0 && templateIndex >= 0
                                          && templateIndex < templates.length ? templates[templateIndex] : null
    readonly property bool wantsName: askName && (!chosenTemplate || chosenTemplate.needsTitle)

    // New note: pick a template (or "Blank note") and, if it needs one, a title.
    function askForNote(templateList, defaultName, callback) {
        heading = "New note";
        message = "";
        acceptText = "Create";
        askName = true;
        nameField.text = "";
        var choices = templateList.slice();
        choices.push({ name: "Blank note", needsTitle: true, blank: true });
        templates = choices;
        var index = 0;
        for (var i = 0; i < choices.length; ++i) {
            if (choices[i].name === defaultName)
                index = i;
        }
        templateIndex = index;
        pendingCallback = callback;
        open();
    }

    function askForName(headingText, initialName, buttonText, callback) {
        templates = [];
        heading = headingText;
        message = "";
        acceptText = buttonText;
        askName = true;
        nameField.text = initialName;
        pendingCallback = callback;
        open();
    }

    function confirm(headingText, messageText, buttonText, callback) {
        templates = [];
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
        var template = chosenTemplate;
        pendingCallback = null;
        if (wantsName && name.trim().length === 0)
            return;
        close();
        if (!callback)
            return;
        if (templates.length > 0)
            callback(name, template.blank ? "" : template.name);
        else
            callback(name);
    }

    modal: true
    focus: true
    // Above other dialogs (export) that ask for a name.
    z: 100
    closePolicy: Popup.CloseOnEscape
    width: Math.min(Math.round(420 * textScale), containerWidth - 48)
    x: Math.round((containerWidth - width) / 2)
    y: Math.round((containerHeight - height) / 3)
    padding: 20

    onOpened: {
        if (wantsName) {
            nameField.forceActiveFocus();
            nameField.selectAll();
        } else {
            acceptButton.forceActiveFocus();
        }
    }
    onClosed: pendingCallback = null

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }

    background: Rectangle {
        color: root.raisedColor
        border.color: root.borderColor
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

        ComboBox {
            id: templateBox
            objectName: "templatePicker"
            width: parent.width
            visible: root.templates.length > 0
            model: root.templates
            textRole: "name"
            currentIndex: root.templateIndex
            onActivated: function(index) {
                root.templateIndex = index;
                if (root.wantsName)
                    nameField.forceActiveFocus();
            }
            font.family: "IBM Plex Mono"
            font.pixelSize: Math.round(13 * root.textScale)
        }

        Label {
            width: parent.width
            visible: root.templates.length > 0 && !root.wantsName
            text: "This template names the note itself."
            color: root.textColor
            opacity: 0.7
            font.family: "IBM Plex Mono"
            font.pixelSize: Math.round(12 * root.textScale)
        }

        Rectangle {
            width: parent.width
            height: Math.round(36 * root.textScale)
            visible: root.wantsName
            color: root.fieldColor
            border.color: nameField.activeFocus ? root.activeButtonColor
                                                : root.borderColor

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
                enabled: !root.wantsName || nameField.text.trim().length > 0
                onClicked: root.finish()
            }
        }
    }
}
