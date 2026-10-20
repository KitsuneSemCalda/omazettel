import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts

// Export a note with pandoc: pick a profile (a pandoc defaults file), where to
// save, and manage the templates and reference documents profiles use.
Dialog {
    id: root
    objectName: "exportDialog"

    property bool darkMode: true
    property real textScale: 1
    property color textColor: "#d0d0d0"
    property color mutedColor: "#909191"
    property color accentColor: "#428bca"
    property color selectionFill: "#428bca"
    property color selectionTextColor: "#ffffff"
    property int containerWidth: 900
    property int containerHeight: 700
    // Set by Main: asks for a name, then calls back with it.
    property var askName: null

    property string notePath: ""
    property string noteText: ""
    property bool useNoteText: false
    property string outputPath: ""
    property int profileIndex: 0
    readonly property var profileList: exporter.revision >= 0 ? exporter.profiles() : []
    readonly property var templateList: exporter.revision >= 0 ? exporter.templates() : []
    readonly property var profile: profileIndex >= 0 && profileIndex < profileList.length
                                   ? profileList[profileIndex] : null
    // "", "ok" or "error", with a message.
    property string resultState: ""
    property string message: ""
    property string exportedPath: ""

    signal editRequested(string path)

    function scaled(pixels) { return Math.max(1, Math.round(pixels * textScale)); }

    // Opens the dialog for a note. text is the editor's copy when it is the
    // open note, so unsaved changes are exported too.
    function openFor(path, text, isOpenNote) {
        notePath = path;
        noteText = text;
        useNoteText = isOpenNote;
        exporter.ensureDefaults();
        exporter.reload();
        // The profile used last, else PDF, else the first.
        var last = exporter.lastProfile;
        var index = -1;
        for (var i = 0; i < profileList.length; ++i) {
            if (profileList[i].path === last)
                index = i;
        }
        for (var j = 0; index < 0 && j < profileList.length; ++j) {
            if (profileList[j].name === "PDF")
                index = j;
        }
        profileIndex = Math.max(0, index);
        updateOutput();
        resultState = "";
        message = "";
        open();
    }

    function updateOutput() {
        if (profile && notePath.length > 0)
            outputPath = exporter.defaultOutputPath(notePath, profile.path);
    }

    function runExport() {
        if (!profile || outputPath.trim().length === 0)
            return;
        exporter.lastProfile = profile.path;
        resultState = "";
        message = "";
        var started = useNoteText
            ? exporter.exportText(notePath, profile.path, outputPath.trim(), noteText)
            : exporter.exportNote(notePath, profile.path, outputPath.trim());
        if (!started) {
            resultState = "error";
            message = exporter.error();
        }
    }

    function selectProfile(path) {
        for (var i = 0; i < profileList.length; ++i) {
            if (profileList[i].path === path) {
                profileIndex = i;
                updateOutput();
                return;
            }
        }
    }

    function newTemplate(kind, suggested) {
        if (!askName)
            return;
        askName("New " + suggested.label, suggested.name, function(name) {
            var path = exporter.newTemplate(kind, name);
            if (path.length === 0) {
                resultState = "error";
                message = exporter.error();
                return;
            }
            resultState = "ok";
            message = "Made " + path.substring(path.lastIndexOf("/") + 1)
                + ". Use it from a profile, or click “New profile with this”.";
            exportedPath = "";
        });
    }

    function newProfile(templatePath) {
        if (!askName || !profile)
            return;
        var base = templatePath.length > 0
            ? templatePath.substring(templatePath.lastIndexOf("/") + 1).replace(/\.[^.]+$/, "")
            : profile.name + " copy";
        askName("New export profile", base, function(name) {
            var path = exporter.newProfile(name, profile.path, templatePath);
            if (path.length === 0) {
                resultState = "error";
                message = exporter.error();
                return;
            }
            selectProfile(path);
            resultState = "ok";
            message = "Made the profile " + name + ". Edit it to change pandoc options.";
            exportedPath = "";
        });
    }

    Connections {
        target: exporter
        function onFinished(ok, outputPath, messages) {
            root.resultState = ok ? "ok" : "error";
            root.exportedPath = ok ? outputPath : "";
            root.message = ok
                ? "Exported to " + vault.prettyPath(outputPath) + (messages.length > 0 ? "\n\n" + messages : "")
                : (messages.length > 0 ? messages : "The export failed.");
        }
    }

    Dialogs.FileDialog {
        id: outputDialog
        title: "Export To"
        fileMode: Dialogs.FileDialog.SaveFile
        onAccepted: root.outputPath = vault.pathForUrl(selectedFile)
    }

    Menu {
        id: newTemplateMenu
        MenuItem { text: "LaTeX template (.tex, for PDF)"; onTriggered: root.newTemplate("latex", { label: "LaTeX template", name: "Article" }) }
        MenuItem { text: "HTML template (.html)"; onTriggered: root.newTemplate("html", { label: "HTML template", name: "Page" }) }
        MenuSeparator {}
        MenuItem { text: "Word reference document (.docx)"; onTriggered: root.newTemplate("docx", { label: "Word reference document", name: "Reference" }) }
        MenuItem { text: "OpenDocument reference (.odt)"; onTriggered: root.newTemplate("odt", { label: "OpenDocument reference", name: "Reference" }) }
        MenuItem { text: "PowerPoint reference (.pptx)"; onTriggered: root.newTemplate("pptx", { label: "PowerPoint reference", name: "Slides" }) }
    }

    modal: true
    focus: true
    closePolicy: exporter.busy ? Popup.NoAutoClose : Popup.CloseOnEscape
    width: Math.min(scaled(640), containerWidth - 48)
    height: Math.min(implicitHeight, containerHeight - 48)
    x: Math.round((containerWidth - width) / 2)
    y: Math.max(24, Math.round((containerHeight - height) / 3))
    padding: scaled(20)

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }

    background: Rectangle {
        color: root.darkMode ? "#1a1a1a" : "#ffffff"
        border.color: root.darkMode ? "#343434" : "#d8d8d8"
    }

    component SmallLabel: Label {
        color: root.mutedColor
        font.family: "IBM Plex Mono"
        font.pixelSize: root.scaled(11)
    }

    component TextButton: Label {
        id: textButton
        signal clicked()
        color: buttonMouse.containsMouse ? root.textColor : root.accentColor
        font.family: "IBM Plex Mono"
        font.pixelSize: root.scaled(12)
        MouseArea {
            id: buttonMouse
            anchors.fill: parent
            anchors.margins: -root.scaled(4)
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: textButton.clicked()
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
            spacing: root.scaled(10)

            Label {
                text: "Export " + root.notePath.substring(root.notePath.lastIndexOf("/") + 1).replace(/\.(md|markdown)$/i, "")
                color: root.textColor
                elide: Text.ElideRight
                Layout.fillWidth: true
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaled(16)
                font.bold: true
            }

            Label {
                visible: !exporter.pandocAvailable
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: "pandoc is not installed. Install it with: sudo pacman -S pandoc-cli"
                color: root.darkMode ? "#e8a0a0" : "#a03030"
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaled(12)
            }

            // ── Profile ─────────────────────────────────────────────────────
            SmallLabel { text: "Profile" }
            RowLayout {
                Layout.fillWidth: true
                spacing: root.scaled(12)
                ComboBox {
                    id: profileBox
                    objectName: "exportProfilePicker"
                    Layout.fillWidth: true
                    model: root.profileList
                    textRole: "name"
                    currentIndex: root.profileIndex
                    onActivated: function(index) {
                        root.profileIndex = index;
                        root.updateOutput();
                    }
                    font.family: "IBM Plex Mono"
                    font.pixelSize: root.scaled(13)
                }
                TextButton {
                    text: "Edit"
                    visible: root.profile !== null
                    onClicked: root.editRequested(root.profile.path)
                }
                TextButton {
                    text: "New…"
                    onClicked: root.newProfile("")
                }
            }
            SmallLabel {
                visible: root.profile !== null
                text: root.profile ? "pandoc -d " + vault.prettyPath(root.profile.path) : ""
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }

            // ── Output ──────────────────────────────────────────────────────
            SmallLabel { text: "Save to"; Layout.topMargin: root.scaled(6) }
            RowLayout {
                Layout.fillWidth: true
                spacing: root.scaled(12)
                Rectangle {
                    Layout.fillWidth: true
                    height: root.scaled(34)
                    color: root.darkMode ? "#202020" : "#f6f6f6"
                    border.color: outputField.activeFocus ? root.accentColor : (root.darkMode ? "#424242" : "#c8c8c8")
                    TextInput {
                        id: outputField
                        objectName: "exportOutputField"
                        anchors.fill: parent
                        anchors.leftMargin: root.scaled(10)
                        anchors.rightMargin: root.scaled(10)
                        verticalAlignment: TextInput.AlignVCenter
                        clip: true
                        selectByMouse: true
                        text: root.outputPath
                        onTextEdited: root.outputPath = text
                        color: root.textColor
                        selectionColor: root.selectionFill
                        selectedTextColor: root.selectionTextColor
                        font.family: "IBM Plex Mono"
                        font.pixelSize: root.scaled(12)
                        Keys.onReturnPressed: root.runExport()
                    }
                }
                TextButton {
                    text: "Choose…"
                    onClicked: {
                        outputDialog.selectedFile = vault.urlForPath(root.outputPath);
                        outputDialog.open();
                    }
                }
            }

            // ── Templates ───────────────────────────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: root.scaled(10)
                Label {
                    text: "Templates and reference documents"
                    color: root.textColor
                    Layout.fillWidth: true
                    font.family: "IBM Plex Mono"
                    font.pixelSize: root.scaled(13)
                    font.bold: true
                }
                TextButton {
                    id: newTemplateButton
                    text: "New ▾"
                    enabled: exporter.pandocAvailable
                    onClicked: newTemplateMenu.popup(newTemplateButton, 0, newTemplateButton.height + root.scaled(6))
                }
                TextButton {
                    text: "Folder"
                    onClicked: Qt.openUrlExternally(vault.urlForPath(exporter.templatesFolder))
                }
            }
            SmallLabel {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: root.templateList.length === 0
                      ? "None yet. A template (.tex, .html) lays out the page; a reference document (.docx, .odt, .pptx) gives Word, LibreOffice or PowerPoint exports their styles. Start from pandoc's default with New."
                      : "Templates lay out PDF and HTML exports; reference documents style Word, LibreOffice and PowerPoint exports."
            }

            Repeater {
                model: root.templateList
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: root.scaled(12)
                    Label {
                        Layout.fillWidth: true
                        text: modelData.name
                        elide: Text.ElideRight
                        color: root.textColor
                        font.family: "IBM Plex Mono"
                        font.pixelSize: root.scaled(12)
                    }
                    SmallLabel { text: modelData.kind === "reference" ? "reference" : "template" }
                    TextButton {
                        text: modelData.text ? "Edit" : "Open"
                        onClicked: modelData.text ? root.editRequested(modelData.path)
                                                  : Qt.openUrlExternally(vault.urlForPath(modelData.path))
                    }
                    TextButton {
                        text: "New profile with this"
                        onClicked: root.newProfile(modelData.path)
                    }
                }
            }

            // ── Result ──────────────────────────────────────────────────────
            RowLayout {
                visible: exporter.busy
                Layout.topMargin: root.scaled(8)
                spacing: root.scaled(10)
                BusyIndicator { running: exporter.busy; implicitWidth: root.scaled(24); implicitHeight: root.scaled(24) }
                Label {
                    text: "Exporting…"
                    color: root.textColor
                    font.family: "IBM Plex Mono"
                    font.pixelSize: root.scaled(12)
                }
            }

            TextEdit {
                objectName: "exportMessage"
                visible: !exporter.busy && root.message.length > 0
                Layout.fillWidth: true
                Layout.topMargin: root.scaled(8)
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                text: root.message
                color: root.resultState === "error" ? (root.darkMode ? "#e8a0a0" : "#a03030") : root.textColor
                selectionColor: root.selectionFill
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaled(11)
            }

            RowLayout {
                visible: !exporter.busy && root.resultState === "ok" && root.exportedPath.length > 0
                spacing: root.scaled(16)
                TextButton {
                    text: "Open file"
                    onClicked: Qt.openUrlExternally(vault.urlForPath(root.exportedPath))
                }
                TextButton {
                    text: "Show folder"
                    onClicked: Qt.openUrlExternally(vault.urlForPath(
                        root.exportedPath.substring(0, root.exportedPath.lastIndexOf("/"))))
                }
            }
        }
    }

    footer: Item {
        implicitHeight: footerButtons.implicitHeight + root.scaled(20)
        Row {
            id: footerButtons
            anchors.right: parent.right
            anchors.rightMargin: root.scaled(20)
            anchors.verticalCenter: parent.verticalCenter
            spacing: root.scaled(8)
            SquareDialogButton {
                text: exporter.busy ? "Stop" : "Close"
                darkMode: root.darkMode
                textScale: root.textScale
                labelColor: root.textColor
                onClicked: exporter.busy ? exporter.cancel() : root.close()
            }
            SquareDialogButton {
                objectName: "exportButton"
                text: "Export"
                primary: true
                darkMode: root.darkMode
                textScale: root.textScale
                activeColor: root.accentColor
                enabled: !exporter.busy && exporter.pandocAvailable && root.profile !== null
                         && root.outputPath.trim().length > 0
                onClicked: root.runExport()
            }
        }
    }
}
