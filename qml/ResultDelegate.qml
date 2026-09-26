import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LingmoUI.CompatibleModule 3.0 as LingmoUI

Item {
    id: control

    property bool selected: false
    signal clicked()
    signal hovered()

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onClicked: control.clicked()
        // Only follow the mouse when it actually moves, so a resting pointer
        // doesn't steal the selection from the keyboard
        onPositionChanged: control.hovered()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 16
        spacing: 12

        LingmoUI.IconItem {
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            source: iconName
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Label {
                Layout.fillWidth: true
                text: title
                elide: Text.ElideRight
                font.pixelSize: 15
                color: control.selected ? LingmoUI.Theme.highlightedTextColor : LingmoUI.Theme.textColor
            }

            Label {
                Layout.fillWidth: true
                text: subtitle
                visible: text !== ""
                elide: Text.ElideMiddle
                font.pixelSize: 12
                color: control.selected ? LingmoUI.Theme.highlightedTextColor : LingmoUI.Theme.disabledTextColor
                opacity: control.selected ? 0.85 : 1.0
            }
        }

        Label {
            text: "↵"
            visible: control.selected
            font.pixelSize: 15
            color: LingmoUI.Theme.highlightedTextColor
        }
    }
}
