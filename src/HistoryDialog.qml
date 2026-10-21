import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A note's saved versions (from git), each compared with the note as it is
// now, and restored on request.
Dialog {
    id: root
    objectName: "historyDialog"

    property bool darkMode: true
    property real textScale: 1
    property color textColor: "#d0d0d0"
    property color mutedColor: "#909191"
    property color accentColor: "#428bca"
    property color raisedColor: darkMode ? "#1a1a1a" : "#ffffff"
    property color borderColor: darkMode ? "#343434" : "#d8d8d8"
    property int containerWidth: 900
    property int containerHeight: 700

    property string notePath: ""
    property string currentText: ""
    property var versions: []
    property int versionIndex: 0
    readonly property var version: versionIndex >= 0 && versionIndex < versions.length ? versions[versionIndex] : null
    readonly property string versionText: version ? sync.versionText(version.hash, version.path) : ""
    readonly property var lines: version ? sync.diff(versionText, currentText) : []

    signal restoreRequested(string text)

    function scaled(pixels) { return Math.max(1, Math.round(pixels * textScale)); }

    function openFor(path, text) {
        notePath = path;
        currentText = text;
        versions = sync.isRepo ? sync.history(path) : [];
        versionIndex = 0;
        open();
    }

    function when(iso) {
        var date = new Date(iso);
        return isNaN(date.getTime()) ? iso : date.toLocaleString(Qt.locale(), Locale.ShortFormat);
    }

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    width: Math.min(scaled(900), containerWidth - 48)
    height: Math.min(scaled(640), containerHeight - 48)
    x: Math.round((containerWidth - width) / 2)
    y: Math.round((containerHeight - height) / 2)
    padding: scaled(20)

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }
    background: Rectangle { color: root.raisedColor; border.color: root.borderColor }

    contentItem: ColumnLayout {
        spacing: root.scaled(8)

        Label {
            text: "History of " + root.notePath.substring(root.notePath.lastIndexOf("/") + 1).replace(/\.(md|markdown)$/i, "")
            elide: Text.ElideMiddle
            Layout.fillWidth: true
            color: root.textColor
            font.family: Qt.application.font.family
            font.pixelSize: root.scaled(16)
            font.bold: true
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: !sync.isRepo ? "Version history is off. Turn it on in … menu → Sync & history."
                  : root.versions.length === 0 ? "No saved versions of this note yet. Versions are saved on each sync."
                  : "− lines in the chosen version but not now   + lines added since"
            color: root.mutedColor
            font.family: Qt.application.font.family
            font.pixelSize: root.scaled(11)
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: root.scaled(12)
            visible: root.versions.length > 0

            ListView {
                id: versionList
                objectName: "versionList"
                Layout.preferredWidth: root.scaled(260)
                Layout.fillHeight: true
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                model: root.versions
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: versionList.width
                    height: versionColumn.implicitHeight + root.scaled(10)
                    radius: root.scaled(4)
                    color: index === root.versionIndex
                           ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.18)
                           : (versionMouse.containsMouse ? Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.06) : "transparent")
                    Column {
                        id: versionColumn
                        x: root.scaled(8)
                        y: root.scaled(5)
                        width: parent.width - root.scaled(16)
                        Label {
                            width: parent.width
                            text: root.when(modelData.date)
                            color: index === root.versionIndex ? root.accentColor : root.textColor
                            font.family: Qt.application.font.family
                            font.pixelSize: root.scaled(12)
                        }
                        Label {
                            width: parent.width
                            text: modelData.author + " · " + modelData.message
                            elide: Text.ElideRight
                            color: root.mutedColor
                            font.family: Qt.application.font.family
                            font.pixelSize: root.scaled(10)
                        }
                    }
                    MouseArea {
                        id: versionMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.versionIndex = index
                    }
                }
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
                text: "Close"
                darkMode: root.darkMode
                textScale: root.textScale
                labelColor: root.textColor
                onClicked: root.close()
            }
            SquareDialogButton {
                objectName: "restoreVersion"
                text: "Restore this version"
                primary: true
                visible: root.version !== null
                darkMode: root.darkMode
                textScale: root.textScale
                activeColor: root.accentColor
                onClicked: {
                    var text = root.versionText;
                    root.close();
                    root.restoreRequested(text);
                }
            }
        }
    }
}
