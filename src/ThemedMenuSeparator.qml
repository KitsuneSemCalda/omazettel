import QtQuick
import QtQuick.Controls

MenuSeparator {
    readonly property bool omaPalette: typeof backend !== "undefined" && backend !== null && backend.omaPalette
    readonly property bool dark: typeof backend === "undefined" || backend === null || backend.darkMode
    topPadding: 4
    bottomPadding: 4
    implicitHeight: visible ? 9 : 0
    contentItem: Rectangle {
        implicitHeight: 1
        color: parent.omaPalette ? "#293241" : (parent.dark ? "#343434" : "#d8d8d8")
    }
}
