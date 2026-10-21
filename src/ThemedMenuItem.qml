import QtQuick
import QtQuick.Controls

// A menu item in OmaZettel's look. Text is shown as written ("&" is not a
// mnemonic), checkable items get a check mark, submenus an arrow.
MenuItem {
    id: item

    readonly property var themeTokens: typeof backend !== "undefined" && backend !== null ? backend.themeTokens : ({})
    readonly property bool dark: typeof backend === "undefined" || backend === null || backend.darkMode
    readonly property real textScale: typeof backend !== "undefined" && backend !== null ? backend.textScale : 1
    readonly property color accent: typeof backend !== "undefined" && backend !== null ? backend.themeAccent : "#428bca"
    readonly property color textColor: themeTokens["editor.foreground"] || (dark ? "#d0d0d0" : "#42464c")
    readonly property color mutedColor: themeTokens["ui.muted"] || (dark ? "#909191" : "#aeb1b5")

    implicitWidth: Math.round(280 * textScale)
    implicitHeight: visible ? Math.round(30 * textScale) : 0
    leftPadding: Math.round(12 * textScale)
    rightPadding: Math.round(12 * textScale)

    indicator: Label {
        x: item.leftPadding
        anchors.verticalCenter: parent.verticalCenter
        visible: item.checkable
        text: item.checked ? "✓" : ""
        color: item.accent
        font.pixelSize: Math.round(13 * item.textScale)
    }

    arrow: Label {
        x: item.width - width - item.rightPadding
        anchors.verticalCenter: parent.verticalCenter
        visible: item.subMenu !== null
        text: "›"
        color: item.mutedColor
        font.pixelSize: Math.round(15 * item.textScale)
    }

    contentItem: Label {
        leftPadding: item.checkable ? Math.round(20 * item.textScale) : 0
        rightPadding: item.subMenu ? Math.round(16 * item.textScale) : 0
        text: item.text
        textFormat: Text.PlainText
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
        color: !item.enabled ? item.mutedColor : (item.highlighted ? item.accent : item.textColor)
        font.family: "IBM Plex Mono"
        font.pixelSize: Math.round(12 * item.textScale)
    }

    background: Rectangle {
        color: item.highlighted && item.enabled
               ? Qt.rgba(item.textColor.r, item.textColor.g, item.textColor.b, 0.07) : "transparent"
        radius: Math.round(3 * item.textScale)
    }
}
