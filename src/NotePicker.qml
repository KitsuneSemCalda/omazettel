import QtQuick
import QtQuick.Controls

// A filtered list of notes. Used two ways: as the quick switcher (a dialog
// with its own search field, Ctrl+O) and as the [[ link completer (a small
// popup at the caret, fed the query being typed).
Popup {
    id: root

    property bool darkMode: true
    property color raisedColor: darkMode ? "#1a1a1a" : "#ffffff"
    property color borderColor: darkMode ? "#343434" : "#d8d8d8"
    property real textScale: 1
    property color textColor: "#d0d0d0"
    property color mutedColor: "#909191"
    property color accentColor: "#428bca"
    property color selectionFill: "#428bca"
    property color selectionTextColor: "#ffffff"
    // With a search field (quick switcher) or without (completer).
    property bool hasField: true
    property string query: hasField ? field.text : ""
    property int maxResults: hasField ? 50 : 8
    property int currentIndex: 0
    readonly property var results: filter(noteIndex.revision >= 0 ? noteIndex.notes() : [], query)
    readonly property var currentResult: currentIndex >= 0 && currentIndex < results.length
                                         ? results[currentIndex] : null

    // A note was picked; name is what was typed when no note matched.
    signal picked(var note, string typed)

    function scaled(pixels) { return Math.max(1, Math.round(pixels * textScale)); }

    // Matches the name, title or folder: whole-word starts rank first, then
    // other substrings, then letters in order (fuzzy).
    function filter(notes, text) {
        var q = text.trim().toLowerCase();
        if (q.length === 0)
            return notes.slice(0, maxResults);
        var scored = [];
        for (var i = 0; i < notes.length; ++i) {
            var note = notes[i];
            var name = note.name.toLowerCase();
            var title = note.title.toLowerCase();
            var full = (note.folder.length > 0 ? note.folder.toLowerCase() + "/" : "") + name;
            var score = -1;
            if (name === q || title === q) score = 100;
            else if (name.indexOf(q) === 0 || title.indexOf(q) === 0) score = 80;
            else if (name.indexOf(" " + q) >= 0 || title.indexOf(" " + q) >= 0) score = 60;
            else if (name.indexOf(q) >= 0 || title.indexOf(q) >= 0) score = 50;
            else if (full.indexOf(q) >= 0) score = 40;
            else if (fuzzy(full, q)) score = 10;
            if (score >= 0)
                scored.push({ note: note, score: score });
        }
        scored.sort(function(a, b) {
            return b.score - a.score || a.note.name.length - b.note.name.length
                || a.note.name.localeCompare(b.note.name);
        });
        return scored.slice(0, maxResults).map(function(entry) { return entry.note; });
    }

    function fuzzy(haystack, needle) {
        var at = 0;
        for (var i = 0; i < needle.length; ++i) {
            if (needle[i] === " ")
                continue;
            at = haystack.indexOf(needle[i], at);
            if (at < 0)
                return false;
            ++at;
        }
        return true;
    }

    function move(step) {
        if (results.length === 0)
            return;
        currentIndex = (currentIndex + step + results.length) % results.length;
        list.positionViewAtIndex(currentIndex, ListView.Contain);
    }

    function accept() {
        var typed = query.trim();
        var note = currentResult;
        close();
        picked(note, typed);
    }

    // For the completer: the editor forwards navigation keys here.
    function handleKey(event) {
        if (!opened)
            return false;
        if (event.key === Qt.Key_Down) { move(1); return true; }
        if (event.key === Qt.Key_Up) { move(-1); return true; }
        if (event.key === Qt.Key_Escape) { close(); return true; }
        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Tab)
                && results.length > 0) {
            accept();
            return true;
        }
        return false;
    }

    onQueryChanged: currentIndex = 0
    onOpened: {
        currentIndex = 0;
        if (hasField) {
            field.text = "";
            field.forceActiveFocus();
        }
    }

    modal: hasField
    focus: hasField
    closePolicy: hasField ? (Popup.CloseOnEscape | Popup.CloseOnPressOutside)
                          : Popup.CloseOnPressOutsideParent
    padding: scaled(6)
    width: scaled(hasField ? 520 : 340)
    height: Math.min(implicitHeight, scaled(hasField ? 460 : 280))

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.45) }

    background: Rectangle {
        color: root.raisedColor
        border.color: root.borderColor
        radius: root.scaled(6)
    }

    contentItem: Column {
        spacing: root.scaled(4)

        Rectangle {
            visible: root.hasField
            width: parent.width
            height: root.scaled(38)
            color: "transparent"
            TextInput {
                id: field
                objectName: "quickSwitcherField"
                anchors.fill: parent
                anchors.leftMargin: root.scaled(10)
                anchors.rightMargin: root.scaled(10)
                verticalAlignment: TextInput.AlignVCenter
                clip: true
                color: root.textColor
                selectionColor: root.selectionFill
                selectedTextColor: root.selectionTextColor
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaled(15)
                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Down) { root.move(1); event.accepted = true; }
                    else if (event.key === Qt.Key_Up) { root.move(-1); event.accepted = true; }
                    else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        root.accept();
                        event.accepted = true;
                    }
                }
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                x: root.scaled(10)
                visible: field.text.length === 0
                text: "Go to note… (Enter on no match creates it)"
                color: root.mutedColor
                font.family: "IBM Plex Mono"
                font.pixelSize: root.scaled(15)
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: root.borderColor
            }
        }

        Label {
            visible: root.results.length === 0
            width: parent.width
            padding: root.scaled(8)
            text: root.query.trim().length > 0 ? "No note named “" + root.query.trim() + "”. Enter creates it."
                                               : "No notes yet."
            wrapMode: Text.Wrap
            color: root.mutedColor
            font.family: "IBM Plex Mono"
            font.pixelSize: root.scaled(12)
        }

        ListView {
            id: list
            objectName: "notePickerList"
            width: parent.width
            height: Math.min(contentHeight, root.scaled(root.hasField ? 400 : 260))
            clip: true
            model: root.results
            currentIndex: root.currentIndex
            boundsBehavior: Flickable.StopAtBounds
            delegate: Rectangle {
                required property var modelData
                required property int index
                width: list.width
                height: root.scaled(root.hasField ? 44 : 32)
                radius: root.scaled(4)
                color: index === root.currentIndex
                       ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.18)
                       : "transparent"
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    x: root.scaled(10)
                    width: parent.width - root.scaled(20)
                    Label {
                        width: parent.width
                        text: modelData.name
                        elide: Text.ElideRight
                        color: index === root.currentIndex ? root.accentColor : root.textColor
                        font.family: "IBM Plex Mono"
                        font.pixelSize: root.scaled(13)
                    }
                    Label {
                        visible: root.hasField && (modelData.folder.length > 0 || modelData.title !== modelData.name)
                        width: parent.width
                        text: (modelData.title !== modelData.name ? modelData.title : "")
                              + (modelData.folder.length > 0
                                 ? (modelData.title !== modelData.name ? "  ·  " : "") + modelData.folder : "")
                        elide: Text.ElideRight
                        color: root.mutedColor
                        font.family: "IBM Plex Mono"
                        font.pixelSize: root.scaled(10)
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: root.currentIndex = index
                    onClicked: {
                        root.currentIndex = index;
                        root.accept();
                    }
                }
            }
        }
    }
}
