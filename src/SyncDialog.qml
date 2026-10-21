import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Sync and version history for the notes folder: turn git on, connect a
// remote (or make a GitHub repository), sync, and resolve conflicts from git
// or from synced-folder tools.
Dialog {
    id: root
    objectName: "syncDialog"

    property bool darkMode: true
    property real textScale: 1
    property color textColor: "#d0d0d0"
    property color mutedColor: "#909191"
    property color accentColor: "#428bca"
    property color raisedColor: darkMode ? "#1a1a1a" : "#ffffff"
    property color fieldColor: darkMode ? "#202020" : "#f6f6f6"
    property color borderColor: darkMode ? "#343434" : "#d8d8d8"
    property int containerWidth: 900
    property int containerHeight: 700
    property string feedback: ""
    readonly property color errorColor: darkMode ? "#e8a0a0" : "#a03030"

    signal syncRequested()
    signal compareRequested(string path)
    signal editConflictRequested(string path)
    signal resolveRequested(string path, string choice)

    function scaled(pixels) { return Math.max(1, Math.round(pixels * textScale)); }

    function relativeTime(date) {
        if (!date || isNaN(date.getTime()))
            return "";
        var seconds = Math.round((Date.now() - date.getTime()) / 1000);
        if (seconds < 60) return "just now";
        if (seconds < 3600) return Math.round(seconds / 60) + " min ago";
        if (seconds < 86400) return Math.round(seconds / 3600) + " h ago";
        return date.toLocaleDateString();
    }

    onAboutToShow: {
        sync.refresh();
        feedback = "";
        remoteField.text = sync.remoteUrl;
        repoNameField.text = vault.rootName.replace(/\s+/g, "-");
    }

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    width: Math.min(scaled(640), containerWidth - 48)
    height: Math.min(implicitHeight, containerHeight - 48)
    x: Math.round((containerWidth - width) / 2)
    y: Math.max(24, Math.round((containerHeight - height) / 3))
    padding: scaled(20)

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }
    background: Rectangle { color: root.raisedColor; border.color: root.borderColor }

    component Small: Label {
        color: root.mutedColor
        wrapMode: Text.Wrap
        Layout.fillWidth: true
        font.family: Qt.application.font.family
        font.pixelSize: root.scaled(11)
    }

    component Heading: Label {
        color: root.textColor
        Layout.topMargin: root.scaled(8)
        font.family: Qt.application.font.family
        font.pixelSize: root.scaled(13)
        font.bold: true
    }

    component Action: Label {
        id: action
        signal clicked()
        color: !enabled ? root.mutedColor : (actionMouse.containsMouse ? root.textColor : root.accentColor)
        font.family: Qt.application.font.family
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

    component Field: Rectangle {
        property alias text: input.text
        Layout.fillWidth: true
        height: root.scaled(30)
        color: root.fieldColor
        border.color: input.activeFocus ? root.accentColor : root.borderColor
        TextInput {
            id: input
            anchors.fill: parent
            anchors.leftMargin: root.scaled(8)
            anchors.rightMargin: root.scaled(8)
            verticalAlignment: TextInput.AlignVCenter
            clip: true
            selectByMouse: true
            color: root.textColor
            font.family: Qt.application.font.family
            font.pixelSize: root.scaled(12)
        }
    }

    contentItem: Flickable {
        implicitHeight: content.implicitHeight
        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: content
            width: parent.width
            spacing: root.scaled(8)

            Label {
                text: "Sync & history"
                color: root.textColor
                font.family: Qt.application.font.family
                font.pixelSize: root.scaled(16)
                font.bold: true
            }

            Small {
                visible: !sync.gitAvailable
                text: "git is not installed. Install it with: sudo pacman -S git"
                color: root.errorColor
            }

            // ── Not yet a repository ────────────────────────────────────────
            Small {
                visible: sync.gitAvailable && !sync.isRepo
                text: "Version history keeps every saved version of your notes with git, so you can see "
                      + "what changed and go back. Add a remote (such as a private GitHub repository) to "
                      + "sync between devices and people."
            }
            Action {
                objectName: "enableHistory"
                visible: sync.gitAvailable && !sync.isRepo
                text: "Turn on version history"
                onClicked: {
                    if (!sync.enable())
                        root.feedback = sync.lastError();
                }
            }

            // ── Repository ──────────────────────────────────────────────────
            GridLayout {
                visible: sync.isRepo
                columns: 2
                columnSpacing: root.scaled(12)
                rowSpacing: root.scaled(4)
                Layout.fillWidth: true
                Small { text: "Folder"; Layout.fillWidth: false }
                Label {
                    Layout.fillWidth: true
                    text: vault.prettyPath(sync.repoRoot) + (sync.branch.length > 0 ? "  (" + sync.branch + ")" : "")
                    elide: Text.ElideMiddle
                    color: root.textColor
                    font.family: Qt.application.font.family
                    font.pixelSize: root.scaled(12)
                }
                Small { text: "Status"; Layout.fillWidth: false }
                Label {
                    objectName: "syncStatus"
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: {
                        var s = sync.state;
                        var base = s === "conflict" ? "Conflicts to resolve"
                                 : s === "error" ? "Last sync failed"
                                 : s === "local" ? "Version history on, no remote"
                                 : s === "changes" ? sync.changeCount + (sync.changeCount === 1 ? " change" : " changes") + " not synced yet"
                                 : "Up to date";
                        var when = root.relativeTime(sync.lastSync);
                        return base + (when.length > 0 ? " · synced " + when : "");
                    }
                    color: sync.state === "error" || sync.state === "conflict" ? root.errorColor : root.textColor
                    font.family: Qt.application.font.family
                    font.pixelSize: root.scaled(12)
                }
            }
            Small {
                visible: sync.isRepo && sync.message.length > 0
                text: sync.message
            }
            RowLayout {
                visible: sync.isRepo
                spacing: root.scaled(16)
                Action {
                    objectName: "syncNowAction"
                    text: sync.busy ? "Syncing…" : (sync.remoteUrl.length > 0 ? "Sync now" : "Save a version now")
                    enabled: !sync.busy && sync.state !== "conflict"
                    onClicked: root.syncRequested()
                }
                BusyIndicator {
                    visible: sync.busy
                    running: sync.busy
                    implicitWidth: root.scaled(20)
                    implicitHeight: root.scaled(20)
                }
            }

            // ── Remote ──────────────────────────────────────────────────────
            Heading { visible: sync.isRepo; text: "Remote" }
            Small {
                visible: sync.isRepo
                text: "Where versions are synced: a git address such as git@github.com:you/notes.git or "
                      + "https://github.com/you/notes.git. Uses your git login (gh auth setup-git or an SSH key)."
            }
            RowLayout {
                visible: sync.isRepo
                Layout.fillWidth: true
                spacing: root.scaled(12)
                Field { id: remoteField; objectName: "remoteField" }
                Action {
                    text: "Save"
                    onClicked: root.feedback = sync.setRemote(remoteField.text)
                               ? (remoteField.text.trim().length > 0 ? "Remote saved." : "Remote removed.")
                               : sync.lastError()
                }
            }
            RowLayout {
                visible: sync.isRepo && sync.remoteUrl.length === 0 && sync.ghAvailable
                Layout.fillWidth: true
                spacing: root.scaled(12)
                Small { text: "Or make a private GitHub repository:"; Layout.fillWidth: false }
                Field { id: repoNameField }
                Action {
                    text: "Create"
                    enabled: !sync.busy && repoNameField.text.trim().length > 0
                    onClicked: {
                        if (!sync.createGitHubRepo(repoNameField.text, true))
                            root.feedback = sync.lastError();
                    }
                }
            }

            // ── Automatic sync ──────────────────────────────────────────────
            Heading { visible: sync.isRepo && sync.remoteUrl.length > 0; text: "Automatic sync" }
            RowLayout {
                visible: sync.isRepo && sync.remoteUrl.length > 0
                spacing: root.scaled(10)
                Label {
                    text: sync.autoSync ? "☑" : "☐"
                    color: sync.autoSync ? root.accentColor : root.mutedColor
                    font.pixelSize: root.scaled(15)
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: sync.autoSync = !sync.autoSync }
                }
                Label {
                    text: "Sync when OmaZettel starts and every"
                    color: root.textColor
                    font.family: Qt.application.font.family
                    font.pixelSize: root.scaled(12)
                }
                Action { text: "−"; onClicked: sync.intervalMinutes = Math.max(1, sync.intervalMinutes - (sync.intervalMinutes > 10 ? 5 : 1)) }
                Label {
                    text: sync.intervalMinutes + " min"
                    color: root.textColor
                    font.family: Qt.application.font.family
                    font.pixelSize: root.scaled(12)
                }
                Action { text: "+"; onClicked: sync.intervalMinutes = sync.intervalMinutes + (sync.intervalMinutes >= 10 ? 5 : 1) }
            }

            // ── Conflicts ───────────────────────────────────────────────────
            Heading {
                visible: sync.gitConflicts.length > 0 || sync.conflictCopies.length > 0
                text: "Conflicts"
            }
            Small {
                visible: sync.gitConflicts.length > 0
                text: "These notes changed here and on another device since the last sync. Compare and "
                      + "choose a version, or edit the note to combine them and click Done editing."
            }
            Repeater {
                model: sync.gitConflicts
                delegate: RowLayout {
                    required property string modelData
                    Layout.fillWidth: true
                    spacing: root.scaled(14)
                    Label {
                        Layout.fillWidth: true
                        text: vault.prettyPath(modelData).replace(vault.prettyPath(vault.rootPath) + "/", "")
                        elide: Text.ElideMiddle
                        color: root.textColor
                        font.family: Qt.application.font.family
                        font.pixelSize: root.scaled(12)
                    }
                    Action { text: "Compare…"; onClicked: root.compareRequested(modelData) }
                    Action { text: "Edit"; onClicked: root.editConflictRequested(modelData) }
                    Action { text: "Done editing"; onClicked: root.resolveRequested(modelData, "edited") }
                }
            }
            Action {
                visible: sync.gitConflicts.length > 0
                text: "Undo the merge (keep everything as it was here)"
                onClicked: {
                    if (!sync.abortMerge())
                        root.feedback = sync.lastError();
                }
            }

            Small {
                visible: sync.conflictCopies.length > 0
                text: "Copies a sync tool (Syncthing, Nextcloud, Dropbox) left when a note changed on two devices at once:"
            }
            Repeater {
                model: sync.conflictCopies
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: root.scaled(14)
                    Label {
                        Layout.fillWidth: true
                        text: modelData.copy.substring(modelData.copy.lastIndexOf("/") + 1) + "  (" + modelData.tool + ")"
                        elide: Text.ElideMiddle
                        color: root.textColor
                        font.family: Qt.application.font.family
                        font.pixelSize: root.scaled(12)
                    }
                    Action { text: "Compare…"; onClicked: root.compareRequested(modelData.copy) }
                }
            }

            Small {
                visible: root.feedback.length > 0
                text: root.feedback
                color: root.textColor
            }

            Small {
                Layout.topMargin: root.scaled(8)
                text: "Prefer Syncthing, Nextcloud or Dropbox? Point it at this folder; OmaZettel lists any "
                      + "conflict copies it makes here. A note's own history is in its right-click menu."
            }
        }
    }

    footer: Item {
        implicitHeight: closeButton.implicitHeight + root.scaled(20)
        SquareDialogButton {
            id: closeButton
            anchors.right: parent.right
            anchors.rightMargin: root.scaled(20)
            anchors.verticalCenter: parent.verticalCenter
            text: "Close"
            darkMode: root.darkMode
            textScale: root.textScale
            labelColor: root.textColor
            onClicked: root.close()
        }
    }
}
