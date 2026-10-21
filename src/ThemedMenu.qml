import QtQuick
import QtQuick.Controls

// A menu in OmaZettel's look: the palette's raised surface, the app font,
// wide enough for its items.
Menu {
    id: menu

    readonly property var themeTokens: typeof backend !== "undefined" && backend !== null ? backend.themeTokens : ({})
    readonly property bool dark: typeof backend === "undefined" || backend === null || backend.darkMode
    readonly property real textScale: typeof backend !== "undefined" && backend !== null ? backend.textScale : 1

    padding: Math.round(4 * textScale)
    width: Math.round(290 * textScale)

    background: Rectangle {
        implicitWidth: menu.width
        color: menu.themeTokens["ui.raised"] || (menu.dark ? "#1a1a1a" : "#ffffff")
        border.color: menu.themeTokens["ui.border"] || (menu.dark ? "#343434" : "#d8d8d8")
        radius: Math.round(4 * menu.textScale)
    }

    delegate: ThemedMenuItem { }
}
