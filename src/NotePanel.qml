import QtQuick
import QtQuick.Controls

// The right-hand panel for the open note: its tags and the notes that link
// to it (backlinks).
Rectangle {
    id: root
    objectName: "notePanel"

    property bool darkMode: true
    property real textScale: 1
    property color pageColor: "#101010"
    property color textColor: "#d0d0d0"
    property color mutedColor: "#909191"
    property color accentColor: "#428bca"
    property string currentPath: ""
    // The open note's headings ([{level, text, position}]) and the caret,
    // to mark the section it is in.
    property var headings: []
    property int cursorPosition: 0
    readonly property int currentHeading: {
        var found = -1;
        for (var i = 0; i < headings.length; ++i) {
            if (headings[i].position <= cursorPosition)
                found = i;
        }
        return found;
    }

    readonly property color panelColor: darkMode ? Qt.lighter(pageColor, 1.25) : Qt.darker(pageColor, 1.04)
    readonly property color hairlineColor: Qt.rgba(mutedColor.r, mutedColor.g, mutedColor.b, 0.25)
    readonly property color hoverFill: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.06)
    readonly property color chipFill: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.16)

    // Re-read whenever the index changes.
    readonly property var backlinks: noteIndex.revision >= 0 && currentPath.length > 0
                                     ? noteIndex.backlinks(currentPath) : []
    readonly property var noteTags: noteIndex.revision >= 0 && currentPath.length > 0
                                    ? noteIndex.tagsOf(currentPath) : []

    signal openRequested(url fileUrl)
    signal tagRequested(string tag)
    signal headingRequested(int position)

    function scaled(pixels) { return Math.max(1, Math.round(pixels * textScale)); }

    color: panelColor

    Rectangle {
        anchors.left: parent.left
        width: 1
        height: parent.height
        color: root.hairlineColor
    }

    Flickable {
        anchors.fill: parent
        anchors.leftMargin: 1
        contentHeight: content.implicitHeight + root.scaled(24)
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            id: content
            x: root.scaled(14)
            y: root.scaled(12)
            width: parent.width - root.scaled(28)
            spacing: root.scaled(10)

            // ── Outline ─────────────────────────────────────────────────────
            Label {
                text: "Outline"
                color: root.textColor
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaled(13)
                font.bold: true
            }

            Label {
                visible: root.headings.length === 0
                width: parent.width
                wrapMode: Text.Wrap
                text: "No headings. Lines starting with # appear here."
                color: root.mutedColor
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaled(11)
            }

            Column {
                objectName: "outline"
                width: parent.width
                visible: root.headings.length > 0
                Repeater {
                    model: root.headings
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        readonly property int minLevel: {
                            var level = 6;
                            for (var i = 0; i < root.headings.length; ++i)
                                level = Math.min(level, root.headings[i].level);
                            return level;
                        }
                        width: parent.width
                        height: root.scaled(24)
                        radius: root.scaled(4)
                        color: index === root.currentHeading
                               ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.14)
                               : (headingMouse.containsMouse ? root.hoverFill : "transparent")
                        Label {
                            x: root.scaled(6) + (modelData.level - parent.minLevel) * root.scaled(12)
                            width: parent.width - x - root.scaled(6)
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.text
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            color: index === root.currentHeading ? root.accentColor : root.textColor
                            font.family: "IBM Plex Mono"
                            font.pixelSize: root.scaled(modelData.level <= parent.minLevel ? 12 : 11)
                            font.bold: modelData.level <= parent.minLevel
                        }
                        MouseArea {
                            id: headingMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.headingRequested(parent.modelData.position)
                        }
                    }
                }
            }

            Item { width: 1; height: root.scaled(6) }

            // ── Tags ────────────────────────────────────────────────────────
            Label {
                text: "Tags"
                color: root.textColor
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaled(13)
                font.bold: true
            }

            Label {
                visible: root.noteTags.length === 0
                width: parent.width
                wrapMode: Text.Wrap
                text: root.currentPath.length > 0 ? "No tags. Type #tag in the note to add one."
                                                  : "Open a note to see its tags."
                color: root.mutedColor
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaled(11)
            }

            Flow {
                objectName: "noteTags"
                width: parent.width
                spacing: root.scaled(6)
                visible: root.noteTags.length > 0
                Repeater {
                    model: root.noteTags
                    delegate: Rectangle {
                        required property string modelData
                        width: tagLabel.implicitWidth + root.scaled(14)
                        height: root.scaled(22)
                        radius: root.scaled(11)
                        color: tagMouse.containsMouse ? Qt.darker(root.chipFill, 0.8) : root.chipFill
                        Label {
                            id: tagLabel
                            anchors.centerIn: parent
                            text: "#" + parent.modelData
                            color: root.accentColor
                            font.family: "IBM Plex Mono"
                            font.pixelSize: root.scaled(11)
                        }
                        MouseArea {
                            id: tagMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.tagRequested(parent.modelData)
                        }
                    }
                }
            }

            Item { width: 1; height: root.scaled(6) }

            // ── Backlinks ───────────────────────────────────────────────────
            Label {
                text: "Backlinks" + (root.backlinks.length > 0 ? "  " + root.backlinks.length : "")
                color: root.textColor
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaled(13)
                font.bold: true
            }

            Label {
                visible: root.backlinks.length === 0
                width: parent.width
                wrapMode: Text.Wrap
                text: root.currentPath.length > 0 ? "No notes link here yet."
                                                  : "Open a note to see what links to it."
                color: root.mutedColor
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaled(11)
            }

            Repeater {
                objectName: "backlinksList"
                model: root.backlinks
                delegate: Rectangle {
                    required property var modelData
                    width: content.width
                    height: linkColumn.implicitHeight + root.scaled(12)
                    radius: root.scaled(4)
                    color: linkMouse.containsMouse ? root.hoverFill : "transparent"

                    Column {
                        id: linkColumn
                        x: root.scaled(6)
                        y: root.scaled(6)
                        width: parent.width - root.scaled(12)
                        spacing: root.scaled(3)
                        Label {
                            width: parent.width
                            text: modelData.title
                            elide: Text.ElideRight
                            color: root.accentColor
                            font.family: "IBM Plex Mono"
                            font.pixelSize: root.scaled(12)
                        }
                        Label {
                            width: parent.width
                            text: modelData.context
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            maximumLineCount: 3
                            elide: Text.ElideRight
                            color: root.mutedColor
                            font.family: "IBM Plex Mono"
                            font.pixelSize: root.scaled(11)
                        }
                    }

                    MouseArea {
                        id: linkMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.openRequested(vault.urlForPath(parent.modelData.path))
                    }
                }
            }
        }
    }
}
