import QtQuick
import QtQuick.Controls

// Two versions of a note compared line by line: lines only in the first
// marked −, only in the second +, in red and green tints.
ListView {
    id: root
    objectName: "diffView"

    property var lines: []
    property real textScale: 1
    property color textColor: "#d0d0d0"
    property color mutedColor: "#909191"
    property bool onlyChanges: false
    readonly property color addedFill: Qt.rgba(0.20, 0.83, 0.60, 0.16)
    readonly property color removedFill: Qt.rgba(0.94, 0.27, 0.27, 0.16)

    function scaled(pixels) { return Math.max(1, Math.round(pixels * textScale)); }

    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
    model: onlyChanges ? lines.filter(function(line) { return line.kind !== "same"; }) : lines

    delegate: Rectangle {
        required property var modelData
        width: root.width
        height: lineText.implicitHeight + root.scaled(2)
        color: modelData.kind === "added" ? root.addedFill
               : modelData.kind === "removed" ? root.removedFill : "transparent"
        Label {
            id: marker
            x: root.scaled(6)
            text: modelData.kind === "added" ? "+" : modelData.kind === "removed" ? "−" : " "
            color: root.mutedColor
            font.family: Qt.application.font.family
            font.pixelSize: root.scaled(12)
        }
        Label {
            id: lineText
            x: root.scaled(24)
            width: parent.width - x - root.scaled(6)
            text: modelData.text.length > 0 ? modelData.text : " "
            textFormat: Text.PlainText
            wrapMode: Text.WrapAnywhere
            color: modelData.kind === "same" ? root.mutedColor : root.textColor
            font.family: Qt.application.font.family
            font.pixelSize: root.scaled(12)
        }
    }
}
