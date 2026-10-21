import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One conflict, compared line by line: this device's version (or the
// original) against the other one (or the sync tool's copy), and a choice.
Dialog {
    id: root
    objectName: "conflictDialog"

    property bool darkMode: true
    property real textScale: 1
    property color textColor: "#d0d0d0"
    property color mutedColor: "#909191"
    property color accentColor: "#428bca"
    property color raisedColor: darkMode ? "#1a1a1a" : "#ffffff"
    property color borderColor: darkMode ? "#343434" : "#d8d8d8"
    property int containerWidth: 900
    property int containerHeight: 700

    property var item: ({})
    readonly property bool isCopy: item.kind === "copy"
    readonly property var lines: item.kind ? sync.diff(item.mine, item.theirs) : []

    signal resolveRequested(string path, string choice)

    function scaled(pixels) { return Math.max(1, Math.round(pixels * textScale)); }

    function openFor(path) {
        item = sync.conflict(path);
        if (item.kind)
            open();
    }

    modal: true
    focus: true
    z: 50
    closePolicy: Popup.CloseOnEscape
    width: Math.min(scaled(820), containerWidth - 48)
    height: Math.min(scaled(640), containerHeight - 48)
    x: Math.round((containerWidth - width) / 2)
    y: Math.round((containerHeight - height) / 2)
    padding: scaled(20)

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }
    background: Rectangle { color: root.raisedColor; border.color: root.borderColor }

    contentItem: ColumnLayout {
        spacing: root.scaled(8)

        Label {
            text: "Compare " + (root.item.label || "")
            elide: Text.ElideMiddle
            Layout.fillWidth: true
            color: root.textColor
            font.family: "IBM Plex Mono"
            font.pixelSize: root.scaled(16)
            font.bold: true
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: root.isCopy
                  ? "− lines only in the original   + lines only in the copy " + (root.item.tool || "") + " made"
                  : "− lines only in this device's version   + lines only in the other version"
                    + (root.item.mineDeleted ? "\nThis device deleted the note." : "")
                    + (root.item.theirsDeleted ? "\nThe other side deleted the note." : "")
            color: root.mutedColor
            font.family: "IBM Plex Mono"
            font.pixelSize: root.scaled(11)
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"
            border.color: root.borderColor
            DiffView {
                anchors.fill: parent
                anchors.margins: 1
                lines: root.lines
                textScale: root.textScale
                textColor: root.textColor
                mutedColor: root.mutedColor
            }
        }
    }

    footer: Item {
        implicitHeight: buttons.implicitHeight + root.scaled(20)
        Row {
            id: buttons
            anchors.right: parent.right
            anchors.rightMargin: root.scaled(20)
            anchors.verticalCenter: parent.verticalCenter
            spacing: root.scaled(8)
            SquareDialogButton {
                text: "Cancel"
                darkMode: root.darkMode
                textScale: root.textScale
                labelColor: root.textColor
                onClicked: root.close()
            }
            SquareDialogButton {
                text: "Keep both"
                darkMode: root.darkMode
                textScale: root.textScale
                labelColor: root.textColor
                visible: !root.item.mineDeleted && !root.item.theirsDeleted
                onClicked: { root.close(); root.resolveRequested(root.item.path, "both"); }
            }
            SquareDialogButton {
                objectName: "keepTheirs"
                text: root.isCopy ? "Keep the copy" : "Keep theirs"
                darkMode: root.darkMode
                textScale: root.textScale
                labelColor: root.textColor
                onClicked: { root.close(); root.resolveRequested(root.item.path, "theirs"); }
            }
            SquareDialogButton {
                objectName: "keepMine"
                text: root.isCopy ? "Keep the original" : "Keep mine"
                primary: true
                darkMode: root.darkMode
                textScale: root.textScale
                activeColor: root.accentColor
                onClicked: { root.close(); root.resolveRequested(root.item.path, "mine"); }
            }
        }
    }
}
