import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The notes sidebar: the open notes folder as a tree of folders and
// Markdown notes, or a prompt to choose a folder when none is open.
// It only reports what the writer asked for. Main.qml decides how to open,
// rename or trash a note, since that can involve the note being edited.
Rectangle {
    id: root
    objectName: "sidebar"

    property bool darkMode: true
    property real textScale: 1
    property color pageColor: "#101010"
    property color textColor: "#d0d0d0"
    property color mutedColor: "#909191"
    property color accentColor: "#428bca"
    property string currentPath: ""

    // Every sidebar colour comes from here, derived from the page colour so
    // the sidebar follows the Omarchy theme.
    readonly property color panelColor: darkMode ? Qt.lighter(pageColor, 1.25) : Qt.darker(pageColor, 1.04)
    readonly property color hairlineColor: Qt.rgba(mutedColor.r, mutedColor.g, mutedColor.b, 0.25)
    readonly property color hoverFill: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.06)
    readonly property color currentFill: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.16)

    signal openRequested(url fileUrl)
    signal newNoteRequested(string folder)
    signal newFolderRequested(string folder)
    signal renameRequested(string path)
    signal trashRequested(string path)
    signal chooseFolderRequested()

    function scaled(pixels) { return Math.max(1, Math.round(pixels * textScale)); }

    // Expands the folders above a path and selects it, so a note opened or
    // created elsewhere shows up in the tree.
    function reveal(path) {
        var index = vault.indexForPath(path);
        if (!index.valid)
            return;
        treeView.expandToIndex(index);
        Qt.callLater(function() {
            var row = treeView.rowAtIndex(index);
            if (row >= 0)
                treeView.positionViewAtRow(row, TableView.Contain);
        });
    }

    color: panelColor

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: root.hairlineColor
    }

    // Drag the right edge to resize.
    MouseArea {
        id: resizeHandle
        anchors.right: parent.right
        anchors.rightMargin: -3
        width: 6
        height: parent.height
        z: 5
        cursorShape: Qt.SplitHCursor
        property real pressX: 0
        property int startWidth: 0
        onPressed: function(mouse) {
            pressX = mapToItem(null, mouse.x, 0).x;
            startWidth = vault.sidebarWidth;
        }
        onPositionChanged: function(mouse) {
            if (pressed)
                vault.sidebarWidth = startWidth + Math.round(mapToItem(null, mouse.x, 0).x - pressX);
        }
    }

    // ── Header ──────────────────────────────────────────────────────────────
    RowLayout {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: root.scaled(12)
        anchors.leftMargin: root.scaled(14)
        anchors.rightMargin: root.scaled(12)
        height: root.scaled(24)
        spacing: root.scaled(12)

        Label {
            Layout.fillWidth: true
            text: vault.isOpen ? vault.rootName : "Notes"
            color: root.textColor
            elide: Text.ElideRight
            font.family: "IBM Plex Mono"
            font.pixelSize: root.scaled(13)
            font.bold: true
        }

        IconButton {
            objectName: "newNoteButton"
            visible: vault.isOpen
            iconName: "note-plus"
            iconColor: root.mutedColor
            hoverColor: root.textColor
            textScale: root.textScale
            tooltip: "New note (Ctrl+N)"
            onClicked: root.newNoteRequested(vault.folderFor(root.currentPath))
        }

        IconButton {
            objectName: "newFolderButton"
            visible: vault.isOpen
            iconName: "folder-plus"
            iconColor: root.mutedColor
            hoverColor: root.textColor
            textScale: root.textScale
            tooltip: "New folder"
            onClicked: root.newFolderRequested(vault.folderFor(root.currentPath))
        }

        IconButton {
            id: moreButton
            iconName: "more"
            iconColor: root.mutedColor
            hoverColor: root.textColor
            textScale: root.textScale
            tooltip: "Notes folder"
            onClicked: folderMenu.popup(moreButton, 0, moreButton.height + root.scaled(8))
        }
    }

    Menu {
        id: folderMenu
        MenuItem {
            text: vault.isOpen ? "Change notes folder…" : "Choose notes folder…"
            onTriggered: root.chooseFolderRequested()
        }
        Menu {
            id: recentMenu
            title: "Recent folders"
            enabled: vault.recentFolders.length > 0
            Repeater {
                model: vault.recentFolders
                MenuItem {
                    required property string modelData
                    text: vault.prettyPath(modelData)
                    onTriggered: vault.openFolder(modelData)
                }
            }
        }
        MenuSeparator { visible: vault.isOpen }
        MenuItem {
            text: "Close notes folder"
            enabled: vault.isOpen
            onTriggered: vault.closeFolder()
        }
    }

    // ── No folder yet ───────────────────────────────────────────────────────
    Column {
        objectName: "sidebarEmptyState"
        visible: !vault.isOpen
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: root.scaled(14)
        anchors.topMargin: root.scaled(24)
        spacing: root.scaled(12)

        Label {
            width: parent.width
            text: "Pick a folder of Markdown notes. OmaZettel works on the files where they are."
            wrapMode: Text.Wrap
            color: root.mutedColor
            font.family: "IBM Plex Mono"
            font.pixelSize: root.scaled(12)
        }

        SquareDialogButton {
            objectName: "chooseFolderButton"
            text: "Choose folder…"
            primary: true
            darkMode: root.darkMode
            textScale: root.textScale
            activeColor: root.accentColor
            onClicked: root.chooseFolderRequested()
        }

        Label {
            visible: vault.recentFolders.length > 0
            topPadding: root.scaled(8)
            text: "Recent"
            color: root.mutedColor
            font.family: "IBM Plex Mono"
            font.pixelSize: root.scaled(11)
            font.capitalization: Font.AllUppercase
        }

        Repeater {
            model: vault.recentFolders
            delegate: Label {
                required property string modelData
                width: parent.width
                text: vault.prettyPath(modelData)
                elide: Text.ElideMiddle
                color: recentMouse.containsMouse ? root.accentColor : root.textColor
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaled(12)
                MouseArea {
                    id: recentMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: vault.openFolder(parent.modelData)
                }
            }
        }
    }

    // ── The notes tree ──────────────────────────────────────────────────────
    TreeView {
        id: treeView
        objectName: "notesTree"
        visible: vault.isOpen
        anchors.top: header.bottom
        anchors.bottom: errorLabel.visible ? errorLabel.top : parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: root.scaled(10)
        anchors.rightMargin: 1
        clip: true
        model: vault.model
        rootIndex: vault.rootIndex
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        delegate: Item {
            id: rowItem

            required property TreeView treeView
            required property bool isTreeNode
            required property bool expanded
            required property bool hasChildren
            required property int depth
            required property int row
            required property int column
            // Model roles, so a renamed note's row updates at once.
            required property string filePath
            required property string fileName

            readonly property string path: filePath
            readonly property bool isFolder: vault.isFolderPath(filePath)
            readonly property bool isCurrent: path.length > 0 && path === root.currentPath

            implicitWidth: treeView.width
            implicitHeight: root.scaled(28)

            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: root.scaled(6)
                anchors.rightMargin: root.scaled(6)
                radius: root.scaled(4)
                color: rowItem.isCurrent ? root.currentFill
                                     : (rowMouse.containsMouse ? root.hoverFill : "transparent")
            }

            IconButton {
                id: chevron
                x: root.scaled(10) + rowItem.depth * root.scaled(14)
                anchors.verticalCenter: parent.verticalCenter
                visible: rowItem.isFolder
                iconName: rowItem.expanded ? "chevron-down" : "chevron-right"
                iconColor: root.mutedColor
                textScale: root.textScale * 0.8
                onClicked: rowItem.treeView.toggleExpanded(rowItem.row)
            }

            Label {
                anchors.left: chevron.right
                anchors.leftMargin: root.scaled(6)
                anchors.right: parent.right
                anchors.rightMargin: root.scaled(12)
                anchors.verticalCenter: parent.verticalCenter
                text: rowItem.isFolder ? rowItem.fileName : rowItem.fileName.replace(/\.(md|markdown)$/i, "")
                elide: Text.ElideRight
                color: rowItem.isCurrent ? root.accentColor
                                     : (rowItem.isFolder ? root.textColor : Qt.rgba(root.textColor.r, root.textColor.g, root.textColor.b, 0.85))
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaled(12)
                font.bold: rowItem.isFolder
            }

            MouseArea {
                id: rowMouse
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                cursorShape: Qt.PointingHandCursor
                onClicked: function(mouse) {
                    if (mouse.button === Qt.RightButton) {
                        itemMenu.path = rowItem.path;
                        itemMenu.isFolder = rowItem.isFolder;
                        itemMenu.popup();
                        return;
                    }
                    if (rowItem.isFolder)
                        rowItem.treeView.toggleExpanded(rowItem.row);
                    else
                        root.openRequested(vault.urlForPath(rowItem.path));
                }
            }
        }
    }

    Menu {
        id: itemMenu
        property string path: ""
        property bool isFolder: false
        MenuItem {
            text: "New note here"
            onTriggered: root.newNoteRequested(vault.folderFor(itemMenu.path))
        }
        MenuItem {
            text: "New folder here"
            onTriggered: root.newFolderRequested(vault.folderFor(itemMenu.path))
        }
        MenuSeparator {}
        MenuItem {
            text: "Rename…"
            onTriggered: root.renameRequested(itemMenu.path)
        }
        MenuItem {
            text: "Move to trash"
            onTriggered: root.trashRequested(itemMenu.path)
        }
    }

    Label {
        id: errorLabel
        objectName: "sidebarError"
        visible: vault.lastError.length > 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        padding: root.scaled(12)
        text: vault.lastError
        wrapMode: Text.Wrap
        color: root.darkMode ? "#e8a0a0" : "#a03030"
        font.family: "IBM Plex Mono"
        font.pixelSize: root.scaled(11)
        MouseArea {
            anchors.fill: parent
            onClicked: vault.clearError()
        }
    }
}
