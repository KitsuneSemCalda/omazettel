import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts

// Where citations come from: a bibliography (BibTeX or CSL JSON) and an
// optional citation style, saved in the notes folder.
Dialog {
    id: root
    objectName: "citationsDialog"

    property bool darkMode: true
    property real textScale: 1
    property color textColor: "#d0d0d0"
    property color mutedColor: "#909191"
    property color accentColor: "#428bca"
    property color raisedColor: darkMode ? "#1a1a1a" : "#ffffff"
    property color borderColor: darkMode ? "#343434" : "#d8d8d8"
    property int containerWidth: 900
    property int containerHeight: 700

    function scaled(pixels) { return Math.max(1, Math.round(pixels * textScale)); }

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    width: Math.min(scaled(600), containerWidth - 48)
    x: Math.round((containerWidth - width) / 2)
    y: Math.max(24, Math.round((containerHeight - height) / 3))
    padding: scaled(20)

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }
    background: Rectangle { color: root.raisedColor; border.color: root.borderColor }

    Dialogs.FileDialog {
        id: bibliographyDialog
        title: "Choose Bibliography"
        nameFilters: ["Bibliographies (*.bib *.json)", "BibTeX (*.bib)", "CSL JSON (*.json)", "All files (*)"]
        onAccepted: citations.setBibliography(vault.pathForUrl(selectedFile))
    }

    Dialogs.FileDialog {
        id: styleDialog
        title: "Choose Citation Style"
        nameFilters: ["Citation styles (*.csl)", "All files (*)"]
        onAccepted: citations.setStyle(vault.pathForUrl(selectedFile))
    }

    component Small: Label {
        color: root.mutedColor
        wrapMode: Text.Wrap
        Layout.fillWidth: true
        font.family: "IBM Plex Mono"
        font.pixelSize: root.scaled(11)
    }

    component Action: Label {
        id: action
        signal clicked()
        color: actionMouse.containsMouse ? root.textColor : root.accentColor
        font.family: "IBM Plex Mono"
        font.pixelSize: root.scaled(12)
        MouseArea {
            id: actionMouse
            anchors.fill: parent
            anchors.margins: -root.scaled(4)
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: action.clicked()
        }
    }

    contentItem: ColumnLayout {
        spacing: root.scaled(10)

        Label {
            text: "Citations"
            color: root.textColor
            font.family: "IBM Plex Mono"
            font.pixelSize: root.scaled(16)
            font.bold: true
        }

        Small {
            text: "Cite with [@key] or [@key, p. 4; @other]. Typing [@ suggests references. "
                  + "Exports turn citations into a formatted reference list."
        }

        Small { text: "Bibliography"; Layout.topMargin: root.scaled(6) }
        RowLayout {
            Layout.fillWidth: true
            spacing: root.scaled(12)
            Label {
                objectName: "bibliographyPath"
                Layout.fillWidth: true
                text: citations.bibliography.length > 0 ? vault.prettyPath(citations.bibliography) : "None"
                elide: Text.ElideMiddle
                color: root.textColor
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaled(12)
            }
            Action { text: "Choose…"; onClicked: bibliographyDialog.open() }
            Action {
                text: "Clear"
                visible: citations.bibliography.length > 0
                onClicked: citations.setBibliography("")
            }
        }
        Small {
            text: citations.error.length > 0 ? citations.error
                  : (citations.bibliography.length > 0 ? citations.count + (citations.count === 1 ? " reference" : " references")
                     : "BibTeX (.bib) or CSL JSON. In Zotero, Better BibTeX can keep one exported for you; OmaZettel reloads it when it changes.")
            color: citations.error.length > 0 ? (root.darkMode ? "#e8a0a0" : "#a03030") : root.mutedColor
        }

        Small { text: "Citation style"; Layout.topMargin: root.scaled(6) }
        RowLayout {
            Layout.fillWidth: true
            spacing: root.scaled(12)
            Label {
                Layout.fillWidth: true
                text: citations.style.length > 0 ? vault.prettyPath(citations.style) : "Chicago author-date (pandoc's default)"
                elide: Text.ElideMiddle
                color: root.textColor
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaled(12)
            }
            Action { text: "Choose…"; onClicked: styleDialog.open() }
            Action {
                text: "Clear"
                visible: citations.style.length > 0
                onClicked: citations.setStyle("")
            }
        }
        Small {
            text: "A .csl file, such as APA or IEEE, from the Zotero Style Repository."
        }
        Action {
            text: "Open the Zotero Style Repository"
            onClicked: Qt.openUrlExternally("https://www.zotero.org/styles")
        }
    }

    footer: Item {
        implicitHeight: closeButton.implicitHeight + root.scaled(20)
        SquareDialogButton {
            id: closeButton
            anchors.right: parent.right
            anchors.rightMargin: root.scaled(20)
            anchors.verticalCenter: parent.verticalCenter
            text: "Done"
            primary: true
            darkMode: root.darkMode
            textScale: root.textScale
            activeColor: root.accentColor
            onClicked: root.close()
        }
    }
}
