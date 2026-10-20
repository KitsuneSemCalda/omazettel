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
    // The row last clicked: a folder, or a note. New notes and folders go
    // in it (or in the folder holding it).
    property string selectedPath: ""
    // Where new notes and folders go right now.
    readonly property string targetFolder: vault.folderFor(selectedPath.length > 0 ? selectedPath : currentPath)

    // Every sidebar colour comes from here, derived from the page colour so
    // the sidebar follows the Omarchy theme.
    readonly property color panelColor: darkMode ? Qt.lighter(pageColor, 1.25) : Qt.darker(pageColor, 1.04)
    readonly property color hairlineColor: Qt.rgba(mutedColor.r, mutedColor.g, mutedColor.b, 0.25)
    readonly property color hoverFill: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.06)
    readonly property color currentFill: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.16)
    readonly property color selectedFill: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.10)
    readonly property color dropFill: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.28)

    signal openRequested(url fileUrl)
    signal newNoteRequested(string folder)
    signal newFolderRequested(string folder)
    signal renameRequested(string path)
    signal trashRequested(string path)
    signal chooseFolderRequested()
    signal moveRequested(string path, string folder)

    // "files" or "tags".
    property string mode: "files"
    property string selectedTag: ""

    function showTag(tag) {
        mode = "tags";
        selectedTag = tag;
    }

    onCurrentPathChanged: selectedPath = currentPath

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
            onClicked: root.newNoteRequested(root.targetFolder)
        }

        IconButton {
            objectName: "newFolderButton"
            visible: vault.isOpen
            iconName: "folder-plus"
            iconColor: root.mutedColor
            hoverColor: root.textColor
            textScale: root.textScale
            tooltip: "New folder"
            onClicked: root.newFolderRequested(root.targetFolder)
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

    // Files | Tags
    Row {
        id: tabs
        objectName: "sidebarTabs"
        visible: vault.isOpen
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.topMargin: root.scaled(10)
        anchors.leftMargin: root.scaled(14)
        height: visible ? root.scaled(22) : 0
        spacing: root.scaled(16)

        Repeater {
            model: [{ id: "files", label: "Files" }, { id: "tags", label: "Tags" }]
            delegate: Label {
                required property var modelData
                text: modelData.label
                color: root.mode === modelData.id ? root.textColor : root.mutedColor
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaled(12)
                font.bold: root.mode === modelData.id
                Rectangle {
                    anchors.top: parent.bottom
                    anchors.topMargin: root.scaled(3)
                    width: parent.width
                    height: 2
                    color: root.accentColor
                    visible: root.mode === parent.modelData.id
                }
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -root.scaled(4)
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.mode = parent.modelData.id
                }
            }
        }
    }

    // ── Tags ────────────────────────────────────────────────────────────────
    ListView {
        id: tagList
        objectName: "tagList"
        visible: vault.isOpen && root.mode === "tags"
        anchors.top: tabs.bottom
        anchors.bottom: errorLabel.visible ? errorLabel.top : parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: root.scaled(12)
        anchors.rightMargin: 1
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        model: noteIndex.revision >= 0 ? noteIndex.tags() : []

        header: Label {
            visible: tagList.count === 0
            height: visible ? implicitHeight : 0
            width: tagList.width
            leftPadding: root.scaled(14)
            rightPadding: root.scaled(14)
            wrapMode: Text.Wrap
            text: "No tags yet. Write #tag in a note, or tags: [a, b] in its front matter."
            color: root.mutedColor
            font.family: "IBM Plex Mono"
            font.pixelSize: root.scaled(11)
        }

        delegate: Column {
            id: tagRow
            required property var modelData
            readonly property bool open: root.selectedTag.toLowerCase() === modelData.tag.toLowerCase()
            width: tagList.width

            Rectangle {
                width: parent.width
                height: root.scaled(28)
                color: "transparent"
                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: root.scaled(6)
                    anchors.rightMargin: root.scaled(6)
                    radius: root.scaled(4)
                    color: tagRow.open ? root.selectedFill : (tagMouse.containsMouse ? root.hoverFill : "transparent")
                }
                IconButton {
                    id: tagChevron
                    x: root.scaled(10)
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: tagRow.open ? "chevron-down" : "chevron-right"
                    iconColor: root.mutedColor
                    textScale: root.textScale * 0.8
                }
                Label {
                    anchors.left: tagChevron.right
                    anchors.leftMargin: root.scaled(6)
                    anchors.right: tagCount.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "#" + tagRow.modelData.tag
                    elide: Text.ElideRight
                    color: root.accentColor
                    font.family: "IBM Plex Mono"
                    font.pixelSize: root.scaled(12)
                }
                Label {
                    id: tagCount
                    anchors.right: parent.right
                    anchors.rightMargin: root.scaled(14)
                    anchors.verticalCenter: parent.verticalCenter
                    text: tagRow.modelData.count
                    color: root.mutedColor
                    font.family: "IBM Plex Mono"
                    font.pixelSize: root.scaled(11)
                }
                MouseArea {
                    id: tagMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.selectedTag = tagRow.open ? "" : tagRow.modelData.tag
                }
            }

            Repeater {
                model: tagRow.open && noteIndex.revision >= 0 ? noteIndex.notesWithTag(tagRow.modelData.tag) : []
                delegate: Rectangle {
                    required property var modelData
                    width: tagRow.width
                    height: root.scaled(26)
                    color: "transparent"
                    Rectangle {
                        anchors.fill: parent
                        anchors.leftMargin: root.scaled(6)
                        anchors.rightMargin: root.scaled(6)
                        radius: root.scaled(4)
                        color: modelData.path === root.currentPath ? root.currentFill
                               : (taggedMouse.containsMouse ? root.hoverFill : "transparent")
                    }
                    Label {
                        x: root.scaled(38)
                        width: parent.width - root.scaled(50)
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.title
                        elide: Text.ElideRight
                        color: modelData.path === root.currentPath ? root.accentColor : root.textColor
                        font.family: "IBM Plex Mono"
                        font.pixelSize: root.scaled(12)
                    }
                    MouseArea {
                        id: taggedMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.openRequested(vault.urlForPath(parent.modelData.path))
                    }
                }
            }
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

    // Dropping below the rows, or on the folder name at the top, moves the
    // dragged note or folder to the top level of the notes folder.
    DropArea {
        id: rootDrop
        objectName: "rootDropArea"
        anchors.fill: treeView
        enabled: treeView.visible
        keys: ["omazettel/path"]
        onDropped: function(drop) {
            root.moveRequested(drop.source.path, vault.rootPath);
            drop.accept();
        }
        Rectangle {
            anchors.fill: parent
            color: root.dropFill
            opacity: 0.35
            visible: rootDrop.containsDrag && !treeView.rowHasDrag
        }
    }

    // What follows the pointer while dragging a row.
    Rectangle {
        id: dragGhost
        objectName: "dragGhost"
        property string path: ""
        property string label: ""
        visible: Drag.active
        z: 100
        width: ghostLabel.implicitWidth + root.scaled(20)
        height: root.scaled(26)
        radius: root.scaled(4)
        color: root.panelColor
        border.color: root.accentColor
        Drag.keys: ["omazettel/path"]
        Drag.source: dragGhost
        Drag.hotSpot.x: root.scaled(10)
        Drag.hotSpot.y: height / 2
        Label {
            id: ghostLabel
            anchors.centerIn: parent
            text: dragGhost.label
            color: root.textColor
            font.family: "IBM Plex Mono"
            font.pixelSize: root.scaled(12)
        }
    }

    TreeView {
        id: treeView
        objectName: "notesTree"
        visible: vault.isOpen && root.mode === "files"
        anchors.top: tabs.bottom
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
        // Set while a row (rather than the space below the rows) is the
        // drop target, so only one highlight shows.
        property bool rowHasDrag: false

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
            readonly property bool isSelected: path.length > 0 && path === root.selectedPath
            readonly property string label: isFolder ? fileName : fileName.replace(/\.(md|markdown)$/i, "")
            // A drop on a folder goes into it; on a note, into the note's folder.
            readonly property string dropFolder: isFolder ? path : vault.folderFor(path)

            implicitWidth: treeView.width
            implicitHeight: root.scaled(28)

            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: root.scaled(6)
                anchors.rightMargin: root.scaled(6)
                radius: root.scaled(4)
                color: rowDrop.containsDrag ? root.dropFill
                       : rowItem.isCurrent ? root.currentFill
                       : rowItem.isSelected ? root.selectedFill
                       : (rowMouse.containsMouse ? root.hoverFill : "transparent")
            }

            DropArea {
                id: rowDrop
                anchors.fill: parent
                keys: ["omazettel/path"]
                onContainsDragChanged: rowItem.treeView.rowHasDrag = containsDrag
                onEntered: function(drag) {
                    // A folder cannot go inside itself.
                    var from = drag.source.path;
                    drag.accepted = from !== rowItem.dropFolder
                        && rowItem.dropFolder.indexOf(from + "/") !== 0
                        && from !== rowItem.path;
                }
                onDropped: function(drop) {
                    root.moveRequested(drop.source.path, rowItem.dropFolder);
                    drop.accept();
                }
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
                text: rowItem.label
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
                cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.PointingHandCursor
                preventStealing: true
                drag.target: dragGhost
                drag.threshold: 6

                onPressed: function(mouse) {
                    if (mouse.button !== Qt.LeftButton)
                        return;
                    var start = mapToItem(root, mouse.x, mouse.y);
                    dragGhost.x = start.x - root.scaled(10);
                    dragGhost.y = start.y - dragGhost.height / 2;
                    dragGhost.path = rowItem.path;
                    dragGhost.label = rowItem.label;
                }
                drag.onActiveChanged: {
                    if (drag.active)
                        dragGhost.Drag.active = true;
                }
                onReleased: {
                    if (dragGhost.Drag.active) {
                        dragGhost.Drag.drop();
                        dragGhost.Drag.active = false;
                        rowItem.treeView.rowHasDrag = false;
                    }
                }
                onCanceled: {
                    dragGhost.Drag.active = false;
                    rowItem.treeView.rowHasDrag = false;
                }

                onClicked: function(mouse) {
                    root.selectedPath = rowItem.path;
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
