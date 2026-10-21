import QtQuick
import QtQuick.Controls

Button {
    id: control

    property bool primary: false
    property bool darkMode: true
    property color labelColor: primary
        ? ((0.2126 * activeColor.r + 0.7152 * activeColor.g + 0.0722 * activeColor.b) > 0.55 ? "#04121A" : "#ffffff")
        : (themeTokens["editor.foreground"] || "#d0d0d0")
    property color activeColor: "#428bca"
    property real textScale: 1
    property string formattedText: ""
    // OmaZettel: the theme's named colours (themed/omazettel.toml.tpl).
    readonly property var themeTokens: typeof backend !== "undefined" && backend !== null ? backend.themeTokens : ({})

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
        font.family: Qt.application.font.family
        font.pixelSize: Math.round(12 * control.textScale)
    }

    background: Rectangle {
        implicitWidth: 88
        implicitHeight: 34
        radius: 0
        color: control.primary
            ? (control.down ? Qt.darker(control.activeColor, 1.15) : control.hovered ? Qt.lighter(control.activeColor, 1.1) : control.activeColor)
            : control.down
                ? (control.themeTokens["ui.raised"] ? Qt.darker(control.themeTokens["ui.field"], 1.15) : control.darkMode ? "#2a2a2a" : "#dedede")
                : control.hovered
                    ? (control.themeTokens["ui.field"] ? Qt.lighter(control.themeTokens["ui.field"], 1.18) : control.darkMode ? "#242424" : "#eeeeee")
                    : (control.themeTokens["ui.field"] || (control.darkMode ? "#202020" : "#f6f6f6"))
        border.color: control.activeFocus
            ? (control.themeTokens["editor.foreground"] || (control.darkMode ? "#eeeeee" : "#222324"))
            : control.primary
                ? Qt.darker(control.activeColor, 1.1)
                : (control.themeTokens["ui.border"] || (control.darkMode ? "#424242" : "#c8c8c8"))
    }
}
