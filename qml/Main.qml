import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import LingmoUI.CompatibleModule 3.0 as LingmoUI

Window {
    id: root

    readonly property int rowHeight: 52
    readonly property int sectionHeight: 28
    readonly property int searchHeight: 64
    readonly property int maxListHeight: 440
    readonly property bool hasResults: resultsView.count > 0

    width: 700
    height: searchHeight + (hasResults ? Math.min(resultsView.contentHeight + 12, maxListHeight) + 1 : 0)
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    Behavior on height {
        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
    }

    // Called by Spotlight::show() before the window appears
    function reset() {
        searchField.text = ""
        resultsView.currentIndex = 0
        searchField.forceActiveFocus()
    }

    function activateCurrent() {
        if (resultsView.currentIndex >= 0 && searchEngine.activate(resultsView.currentIndex))
            spotlight.hide()
    }

    // Clicking anywhere else closes it, like macOS
    onActiveChanged: if (!active && visible) spotlight.hide()

    LingmoUI.WindowHelper { id: windowHelper }

    LingmoUI.WindowBlur {
        view: root
        geometry: Qt.rect(root.x, root.y, root.width, root.height)
        windowRadius: background.radius
        enabled: windowHelper.compositing
    }

    LingmoUI.WindowShadow {
        view: root
        geometry: Qt.rect(root.x, root.y, root.width, root.height)
        radius: background.radius
    }

    Rectangle {
        id: background
        anchors.fill: parent
        radius: 16
        color: LingmoUI.Theme.darkMode ? "#2A2A2E" : "#F6F6F8"
        opacity: windowHelper.compositing ? (LingmoUI.Theme.darkMode ? 0.78 : 0.82) : 1.0
        border.width: 1
        border.color: LingmoUI.Theme.darkMode ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(0, 0, 0, 0.12)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Search field
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.searchHeight
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            spacing: 12

            LingmoUI.IconItem {
                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                source: "system-search"
                opacity: 0.7
            }

            TextField {
                id: searchField
                Layout.fillWidth: true
                font.pixelSize: 24
                placeholderText: qsTr("Spotlight Search")
                color: LingmoUI.Theme.textColor
                placeholderTextColor: LingmoUI.Theme.disabledTextColor
                selectByMouse: true
                background: Item {}

                onTextChanged: {
                    searchEngine.query = text
                    resultsView.currentIndex = 0
                }

                Keys.onDownPressed: resultsView.incrementCurrentIndex()
                Keys.onUpPressed: resultsView.decrementCurrentIndex()
                Keys.onReturnPressed: root.activateCurrent()
                Keys.onEnterPressed: root.activateCurrent()
                Keys.onEscapePressed: spotlight.hide()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            visible: root.hasResults
            color: LingmoUI.Theme.darkMode ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.08)
        }

        ListView {
            id: resultsView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 6
            visible: root.hasResults
            clip: true
            model: searchEngine.results
            currentIndex: 0
            highlightMoveDuration: 0
            boundsBehavior: Flickable.StopAtBounds
            keyNavigationWraps: true

            section.property: "category"
            section.delegate: Label {
                width: ListView.view.width
                height: root.sectionHeight
                leftPadding: 14
                verticalAlignment: Text.AlignBottom
                bottomPadding: 4
                text: section
                font.pixelSize: 12
                font.bold: true
                color: LingmoUI.Theme.disabledTextColor
            }

            highlight: Rectangle {
                radius: 10
                color: LingmoUI.Theme.highlightColor
            }

            delegate: ResultDelegate {
                width: ListView.view.width
                height: root.rowHeight
                selected: ListView.isCurrentItem
                onClicked: {
                    resultsView.currentIndex = index
                    root.activateCurrent()
                }
                onHovered: resultsView.currentIndex = index
            }

            ScrollBar.vertical: ScrollBar {}
        }
    }
}
