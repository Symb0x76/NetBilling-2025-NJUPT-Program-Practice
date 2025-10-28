import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15

ApplicationWindow {
    id: win
    width: 1100; height: 720; visible: true
    title: "上网计费系统 (Fluent)"
    
    Material.theme: Material.Light
    Material.primary: Material.Blue
    Material.accent: Material.Blue

    header: ToolBar {
        contentHeight: 48
        RowLayout {
            anchors.fill: parent; anchors.margins: 10
            Label { text: win.title; font.pointSize: 14; font.bold: true }
            Item { Layout.fillWidth: true }
            Button { 
                text: "明/暗"
                onClicked: {
                    win.Material.theme = (win.Material.theme === Material.Light) 
                        ? Material.Dark 
                        : Material.Light
                }
            }
        }
    }

    Drawer {
        id: nav; width: 240; edge: Qt.LeftEdge; modal: false; visible: true
        Column {
            spacing: 8; anchors.margins: 12; anchors.fill: parent
            ToolButton { text: "用户";   onClicked: stackView.push("pages/UsersPage.qml") }
            ToolButton { text: "上网记录"; onClicked: stackView.push("pages/SessionsPage.qml") }
            ToolButton { text: "生成账单"; onClicked: stackView.push("pages/BillingPage.qml") }
            ToolButton { text: "报表(选做)"; onClicked: stackView.push("pages/ReportsPage.qml") }
        }
    }

    StackView {
        id: stackView
        initialItem: "pages/BillingPage.qml"
        anchors.fill: parent
    }
}
