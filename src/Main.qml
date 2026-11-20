import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import Mread

ApplicationWindow {
    id: win
    width: 1000
    height: 820
    minimumWidth: 420
    minimumHeight: 320
    visible: true
    title: document.isOpen ? document.title + " - Mread" : "Mread"
    color: theme.background

    Material.theme: theme.dark ? Material.Dark : Material.Light
    Material.accent: theme.accent
    Material.background: theme.background
    Material.foreground: theme.foreground
    Material.roundedScale: theme.radius

    // Vim state: a pending prefix (g, [, ]) and a numeric count.
    property string pending: ""
    property string countText: ""
    property string searchText: ""
    property string message: ""
    property int savedPosition: 0
    property real savedOffset: 0

    readonly property real lineStep: bodyMetrics.height * 1.4
    readonly property real maxY: Math.max(0, view.contentHeight - view.height)

    FontMetrics {
        id: bodyMetrics
        font.family: settings.bodyFont
        font.pixelSize: settings.fontSize
    }

    Component.onCompleted: {
        if (initialFile !== "")
            document.open(initialFile)
        reader.forceActiveFocus()
    }

    // ---- motions ---------------------------------------------------------

    function scrollTo(y) { view.contentY = Math.max(0, Math.min(maxY, y)) }
    function scrollBy(dy) { scrollTo(view.contentY + dy) }
    function topPosition() { return view.positionAtY(view.contentY + 1) }
    function yOf(position) { return view.yOf(position) }

    function showPosition(position, atTop) {
        const y = yOf(position)
        if (atTop)
            scrollTo(y - lineStep)
        else if (y < view.contentY || y > view.contentY + view.height - lineStep * 2)
            scrollTo(y - view.height / 3)
    }

    // A heading counts as current once it sits where a jump would put it, so
    // ]] always moves on to the next one.
    function jumpHeading(forward, times) {
        const headings = document.headings
        let anchor = view.contentY
        let target = -1
        for (let n = 0; n < times; ++n) {
            let next = -1
            for (let i = 0; i < headings.length; ++i) {
                const top = yOf(headings[i]) - lineStep
                if (forward && top > anchor + lineStep * 1.5) { next = headings[i]; break }
                if (!forward && top < anchor - 2) next = headings[i]
            }
            if (next < 0) break
            target = next
            anchor = yOf(next) - lineStep
        }
        if (target >= 0)
            showPosition(target, true)
        else
            scrollTo(forward ? maxY : 0)
    }

    function search(forward) {
        if (searchText === "")
            return
        view.highlight = searchText
        const from = view.hasSelection
                ? (forward ? view.selectionEnd() : view.selectionStart())
                : topPosition()
        const match = document.find(searchText, from, !forward)
        if (match[0] < 0) {
            message = "Pattern not found: " + searchText
            view.deselect()
            return
        }
        message = ""
        view.select(match[0], match[1], true)
        showPosition(match[0], false)
    }

    // ---- actions ---------------------------------------------------------

    function openFinder() {
        fileIndex.refresh()
        filePicker.open()
    }

    function closePopups() {
        help.visible = false
        reader.forceActiveFocus()
    }

    function handleKey(event) {
        const ctrl = event.modifiers & Qt.ControlModifier
        const key = event.key
        const text = event.text
        const count = countText === "" ? 1 : parseInt(countText)

        if (help.visible && (key === Qt.Key_Escape || text === "?" || text === "q")) {
            help.visible = false
            return true
        }

        if (ctrl) {
            countText = ""; pending = ""
            const half = view.height / 2, page = view.height - lineStep * 2
            if (key === Qt.Key_D) scrollBy(half * count)
            else if (key === Qt.Key_U) scrollBy(-half * count)
            else if (key === Qt.Key_F) scrollBy(page * count)
            else if (key === Qt.Key_B) scrollBy(-page * count)
            else if (key === Qt.Key_E) scrollBy(lineStep * count)
            else if (key === Qt.Key_Y) scrollBy(-lineStep * count)
            else if (key === Qt.Key_C) view.copy()
            else if (key === Qt.Key_O) openFinder()
            else if (key === Qt.Key_Q) Qt.quit()
            else return false
            return true
        }

        // Counts: digits before a motion; a lone 0 resets the font size.
        if (text >= "1" && text <= "9" || (text === "0" && countText !== "")) {
            countText += text
            return true
        }

        if (pending !== "") {
            const prefix = pending
            pending = ""
            countText = ""
            if (prefix === "g" && text === "g") scrollTo(0)
            else if (prefix === "]" && text === "]") jumpHeading(true, count)
            else if (prefix === "[" && text === "[") jumpHeading(false, count)
            return true
        }
        if (text === "g" || text === "[" || text === "]") {
            pending = text
            return true
        }

        countText = ""
        const page = view.height - lineStep * 2
        switch (text) {
        case "j": scrollBy(lineStep * count); return true
        case "k": scrollBy(-lineStep * count); return true
        case "d": scrollBy(view.height / 2 * count); return true
        case "u": scrollBy(-view.height / 2 * count); return true
        case " ": scrollBy(page * count); return true
        case "b": scrollBy(-page * count); return true
        case "G": scrollTo(maxY); return true
        case "/": searchField.open(); return true
        case "n": search(true); return true
        case "N": search(false); return true
        case "o": openFinder(); return true
        case "x": document.close(); return true
        case "f": fontPicker.target = "body"; fontPicker.open(); return true
        case "F": fontPicker.target = "code"; fontPicker.open(); return true
        case "+": case "=": settings.fontSize += count; return true
        case "-": settings.fontSize -= count; return true
        case "0": settings.resetFontSize(); return true
        case ">": settings.columns += 5 * count; return true
        case "<": settings.columns -= 5 * count; return true
        case "y":
            if (view.hasSelection) { view.copy(); message = "Copied" }
            return true
        case "?": help.visible = !help.visible; return true
        case "q": Qt.quit(); return true
        }

        switch (key) {
        case Qt.Key_Down: scrollBy(lineStep * count); return true
        case Qt.Key_Up: scrollBy(-lineStep * count); return true
        case Qt.Key_PageDown: scrollBy(page * count); return true
        case Qt.Key_PageUp: scrollBy(-page * count); return true
        case Qt.Key_Home: scrollTo(0); return true
        case Qt.Key_End: scrollTo(maxY); return true
        case Qt.Key_Escape:
            view.deselect(); view.highlight = ""; message = ""; help.visible = false
            return true
        }
        return false
    }

    // ---- document --------------------------------------------------------

    Connections {
        target: document
        // Keep the text at the top of the window in place across re-renders.
        function onAboutToRender() {
            win.savedPosition = win.topPosition()
            win.savedOffset = win.yOf(win.savedPosition) - view.contentY
        }
        function onRendered(reload) {
            if (reload)
                win.scrollTo(win.yOf(win.savedPosition) - win.savedOffset)
            else
                win.scrollTo(0)
        }
        function onErrorChanged() { if (document.error !== "") win.message = document.error }
        function onPathChanged() { win.message = "" }
    }

    Item {
        id: reader
        anchors.fill: parent
        focus: true
        Keys.onPressed: event => { event.accepted = win.handleKey(event) }

        MarkdownView {
            id: view
            anchors { left: parent.left; right: parent.right; top: parent.top; bottom: statusBar.top }
            visible: document.isOpen
            source: document
            textWidth: Math.min(width - 48, settings.columns * bodyMetrics.averageCharacterWidth)
            wheelStep: win.lineStep * 3
            textColor: theme.foreground
            selectionColor: theme.selection
            matchColor: theme.lighterBackground
            currentMatchColor: theme.accent
            currentMatchTextColor: theme.background
            onLinkActivated: link => {
                const position = document.followLink(link)
                if (position >= 0)
                    win.showPosition(position, true)
            }
        }

        ScrollBar {
            id: scrollBar
            anchors { right: view.right; top: view.top; bottom: view.bottom }
            visible: document.isOpen && view.contentHeight > view.height
            orientation: Qt.Vertical
            size: view.height / Math.max(1, view.contentHeight)
            position: view.contentY / Math.max(1, view.contentHeight)
            onPositionChanged: if (pressed) view.contentY = position * view.contentHeight
        }

        // Shown when no file is open.
        Column {
            anchors.centerIn: parent
            spacing: 14
            visible: !document.isOpen
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Mread"
                color: theme.accent
                font.family: settings.codeFont
                font.pixelSize: 32
                font.bold: true
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "o  open file     ?  keys     q  quit"
                color: theme.muted
                font.family: settings.codeFont
                font.pixelSize: 15
            }
        }

        DropArea {
            anchors.fill: parent
            onDropped: drop => {
                if (drop.hasUrls)
                    document.open(decodeURIComponent(drop.urls[0].toString().replace(/^file:\/\//, "")))
            }
        }

        // ---- status line ---------------------------------------------------

        Rectangle {
            id: statusBar
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 26
            color: theme.darkBackground

            Text {
                anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                width: parent.width * 0.6
                elide: Text.ElideMiddle
                text: win.message !== "" ? win.message
                    : document.isOpen ? document.path.replace(/^\/home\/[^\/]+/, "~") : "No file"
                color: document.error !== "" && win.message === document.error ? theme.color("red") : theme.muted
                font.family: settings.codeFont
                font.pixelSize: 12
            }

            Text {
                anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                text: {
                    const keys = win.countText + win.pending
                    let where = ""
                    if (document.isOpen)
                        where = win.maxY <= 0 ? "All"
                              : view.contentY <= 0 ? "Top"
                              : view.contentY >= win.maxY - 1 ? "Bot"
                              : Math.round(view.contentY / win.maxY * 100) + "%"
                    return (keys !== "" ? keys + "   " : "")
                         + settings.fontSize + "px  " + settings.columns + "col   " + where
                }
                color: theme.muted
                font.family: settings.codeFont
                font.pixelSize: 12
            }
        }

        // Search prompt, vim style, over the status line.
        TextInput {
            id: searchField
            visible: false
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 12; rightMargin: 12 }
            height: statusBar.height
            verticalAlignment: TextInput.AlignVCenter
            color: theme.foreground
            selectionColor: theme.selection
            font.family: settings.codeFont
            font.pixelSize: 13
            z: 2

            property int origin: 0

            function open() {
                text = ""
                origin = win.topPosition()
                visible = true
                forceActiveFocus()
            }

            function finish(keep) {
                visible = false
                if (!keep) {
                    view.deselect()
                    view.highlight = win.searchText
                    win.scrollTo(win.yOf(origin) - 1)
                }
                reader.forceActiveFocus()
            }

            // Incremental search from where the prompt was opened.
            onTextChanged: {
                if (!visible || text === "")
                    return
                view.highlight = text
                const match = document.find(text, origin, false)
                if (match[0] >= 0) {
                    view.select(match[0], match[1], true)
                    win.showPosition(match[0], false)
                } else {
                    view.deselect()
                }
            }

            Keys.onPressed: event => {
                if (event.key === Qt.Key_Escape) {
                    finish(false)
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    win.searchText = text
                    finish(true)
                    if (!view.hasSelection && text !== "")
                        win.message = "Pattern not found: " + text
                } else {
                    return
                }
                event.accepted = true
            }

            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: -12
                anchors.rightMargin: -12
                color: theme.darkBackground
                z: -1
            }
            Text {
                anchors { right: parent.left; verticalCenter: parent.verticalCenter }
                text: "/"
                color: theme.accent
                font: searchField.font
            }
        }
    }

    // ---- overlays --------------------------------------------------------

    FuzzyPicker {
        id: filePicker
        model: fileIndex.model
        placeholder: "Open Markdown file"
        status: fileIndex.error !== "" ? fileIndex.error
              : fileIndex.scanning ? "Scanning..." : ""
        onPicked: value => { document.open(value); win.closePopups() }
        onDismissed: win.closePopups()
    }

    FuzzyPicker {
        id: fontPicker
        property string target: "body"
        model: fontModel
        previewFonts: true
        placeholder: target === "body" ? "Text font (now " + settings.bodyFont + ")"
                                       : "Code font (now " + settings.codeFont + ")"
        onPicked: value => {
            if (target === "body") settings.bodyFont = value
            else settings.codeFont = value
            win.closePopups()
        }
        onDismissed: win.closePopups()
    }

    KeyHelp { id: help }
}
