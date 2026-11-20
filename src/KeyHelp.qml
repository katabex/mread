import QtQuick

// Every shortcut, shown with "?".
Rectangle {
    id: help
    visible: false
    z: 20
    anchors.centerIn: parent
    width: grid.implicitWidth + 64
    height: grid.implicitHeight + heading.height + 56
    color: theme.darkBackground
    border.color: theme.accent
    border.width: 1
    radius: theme.radius

    readonly property var keys: [
        ["j / k", "Line down / up"],
        ["Ctrl+d / Ctrl+u", "Half page down / up"],
        ["Ctrl+f / Ctrl+b, Space", "Page down / up"],
        ["gg / G", "Top / bottom"],
        ["]] / [[", "Next / previous heading"],
        ["/  then  n / N", "Search, next / previous match"],
        ["5j, 3]] ...", "Counts repeat a motion"],
        ["o", "Open file (fuzzy finder)"],
        ["x", "Close file"],
        ["f / F", "Text font / code font"],
        ["+ / - / 0", "Font size up / down / reset"],
        ["> / <", "Column wider / narrower"],
        ["y", "Copy selection"],
        ["Esc", "Clear search and selection"],
        ["?", "This help"],
        ["q", "Quit"]
    ]

    Text {
        id: heading
        anchors { top: parent.top; left: parent.left; topMargin: 20; leftMargin: 32 }
        text: "Mread"
        color: theme.accent
        font.family: settings.codeFont
        font.pixelSize: 18
        font.bold: true
    }

    Grid {
        id: grid
        anchors { top: heading.bottom; left: parent.left; topMargin: 14; leftMargin: 32 }
        columns: 2
        columnSpacing: 28
        rowSpacing: 6
        Repeater {
            model: help.keys.length * 2
            Text {
                required property int index
                text: help.keys[Math.floor(index / 2)][index % 2]
                color: index % 2 === 0 ? theme.accent : theme.foreground
                font.family: settings.codeFont
                font.pixelSize: 14
            }
        }
    }
}
