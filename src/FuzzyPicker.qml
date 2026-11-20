import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A centered fuzzy finder over a FuzzyModel: type to filter, Up/Down or
// Ctrl+J/K (Ctrl+N/P) to move, Enter to pick, Escape to dismiss.
Rectangle {
    id: picker

    property var model
    property string placeholder
    property string status
    property bool previewFonts: false
    signal picked(string value)
    signal dismissed()

    visible: false
    width: Math.min(760, parent.width - 80)
    height: Math.min(parent.height * 0.7, input.height + list.contentHeight + footer.height + 60)
    anchors.horizontalCenter: parent.horizontalCenter
    y: parent.height * 0.12
    z: 10
    color: theme.darkBackground
    border.color: theme.accent
    border.width: 1
    radius: theme.radius

    function open() {
        model.query = ""
        input.text = ""
        list.currentIndex = 0
        visible = true
        input.forceActiveFocus()
    }

    function close() {
        visible = false
        dismissed()
    }

    function move(delta) {
        if (list.count > 0)
            list.currentIndex = Math.max(0, Math.min(list.count - 1, list.currentIndex + delta))
    }

    function accept() {
        const value = model.valueAt(list.currentIndex)
        visible = false
        if (value !== "")
            picked(value)
        else
            dismissed()
    }

    TextField {
        id: input
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
        placeholderText: picker.placeholder
        font.family: settings.codeFont
        font.pixelSize: 15
        color: theme.foreground
        Material.accent: theme.accent
        onTextChanged: {
            picker.model.query = text
            list.currentIndex = 0
        }
        Keys.onPressed: event => {
            const ctrl = event.modifiers & Qt.ControlModifier
            if (event.key === Qt.Key_Escape) picker.close()
            else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) picker.accept()
            else if (event.key === Qt.Key_Down || (ctrl && (event.key === Qt.Key_J || event.key === Qt.Key_N))) picker.move(1)
            else if (event.key === Qt.Key_Up || (ctrl && (event.key === Qt.Key_K || event.key === Qt.Key_P))) picker.move(-1)
            else if (event.key === Qt.Key_PageDown) picker.move(10)
            else if (event.key === Qt.Key_PageUp) picker.move(-10)
            else return
            event.accepted = true
        }
    }

    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: input.bottom; bottom: footer.top; margins: 6 }
        clip: true
        model: picker.model
        boundsBehavior: Flickable.StopAtBounds
        highlightMoveDuration: 0
        ScrollBar.vertical: ScrollBar {}
        delegate: Rectangle {
            required property int index
            required property string highlighted
            required property string value
            width: ListView.view.width
            height: label.implicitHeight + 10
            radius: Math.min(4, theme.radius)
            color: ListView.isCurrentItem ? theme.selection : "transparent"
            Text {
                id: label
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 10; rightMargin: 10 }
                text: highlighted
                textFormat: Text.StyledText
                elide: Text.ElideLeft
                color: theme.foreground
                font.family: picker.previewFonts ? value : settings.codeFont
                font.pixelSize: 14
            }
            MouseArea {
                anchors.fill: parent
                onClicked: { list.currentIndex = index; picker.accept() }
            }
        }
    }

    Text {
        id: footer
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 12 }
        text: picker.status !== "" ? picker.status : picker.model ? picker.model.count + " / " + picker.model.total : ""
        color: theme.muted
        font.family: settings.codeFont
        font.pixelSize: 12
        elide: Text.ElideRight
    }
}
