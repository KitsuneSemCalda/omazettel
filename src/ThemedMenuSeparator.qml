import QtQuick
import QtQuick.Controls

MenuSeparator {
    readonly property var themeTokens: typeof backend !== "undefined" && backend !== null ? backend.themeTokens : ({})
    readonly property bool dark: typeof backend === "undefined" || backend === null || backend.darkMode
    topPadding: 4
    bottomPadding: 4
    implicitHeight: visible ? 9 : 0
    contentItem: Rectangle {
        implicitHeight: 1
        color: parent.themeTokens["ui.border"] || (parent.dark ? "#343434" : "#d8d8d8")
    }
}
