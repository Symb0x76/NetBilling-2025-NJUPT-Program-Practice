import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    anchors.fill: parent
    ColumnLayout {
        anchors.margins: 24; anchors.fill: parent; spacing: 14

        GroupBox {
            title: "按月份生成账单"
            Layout.fillWidth: true
            RowLayout {
                spacing: 12
                SpinBox { id: y; from: 2000; to: 2100; value: new Date().getFullYear(); width: 120 }
                SpinBox { id: m; from: 1; to: 12; value: (new Date().getMonth()+1); width: 80 }
                TextField { id: outDir; text: Qt.resolvedUrl("../../out"); Layout.fillWidth: true }
                Button {
                    text: "生成"
                    onClicked: {
                        // 这里建议通过注册的 QObject 接口调用 C++：repoWrapper.computeAndWrite(y.value,m.value)
                        // 为了展示思路，省略直接调用，按你的封装接入即可。
                    }
                }
            }
        }

        TableView {
            id: table; Layout.fillWidth: true; Layout.fillHeight: true
            // 使用 ListModel 绑定 C++ 返回的账单列表
            // columns: 账号 / 姓名 / 套餐 / 分钟 / 金额
        }
    }
}
