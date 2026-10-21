import QtQuick
import QtQuick.Controls

Button {
    id: control

    property bool primary: false
    property bool darkMode: true
    property color labelColor: primary
        ? ((0.2126 * activeColor.r + 0.7152 * activeColor.g + 0.0722 * activeColor.b) > 0.55 ? "#04121A" : "#ffffff")
        : (omaPalette ? "#E7EDF7" : "#d0d0d0")
    property color activeColor: "#428bca"
    property real textScale: 1
    property string formattedText: ""
    // OmaZettel: the Oma palette's surfaces when it is on.
    readonly property bool omaPalette: typeof backend !== "undefined" && backend !== null && backend.omaPalette

    leftPadding: 16
    rightPadding: 16
    topPadding: 7
    bottomPadding: 7

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    contentItem: Label {
        text: control.formattedText.length > 0 ? control.formattedText : control.text
        textFormat: control.formattedText.length > 0 ? Text.StyledText : Text.PlainText
        color: control.labelColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.family: "IBM Plex Mono"
        font.pixelSize: Math.round(12 * control.textScale)
    }

    background: Rectangle {
        implicitWidth: 88
        implicitHeight: 34
        radius: 0
        color: control.primary
            ? (control.down ? Qt.darker(control.activeColor, 1.15) : control.hovered ? Qt.lighter(control.activeColor, 1.1) : control.activeColor)
            : control.down
                ? (control.omaPalette ? "#171E2A" : control.darkMode ? "#2a2a2a" : "#dedede")
                : control.hovered
                    ? (control.omaPalette ? "#222D3D" : control.darkMode ? "#242424" : "#eeeeee")
                    : (control.omaPalette ? "#1B2431" : control.darkMode ? "#202020" : "#f6f6f6")
        border.color: control.activeFocus
            ? (control.omaPalette ? "#E7EDF7" : control.darkMode ? "#eeeeee" : "#222324")
            : control.primary
                ? Qt.darker(control.activeColor, 1.1)
                : (control.omaPalette ? "#3A4658" : control.darkMode ? "#424242" : "#c8c8c8")
    }
}
