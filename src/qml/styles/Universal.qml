import QtQuick 2.15
import QtQuick.Controls 2.15

// 统一外观：圆角、内边距、字号
QtObject {
    property int radius: 14
    property int padding: 14
    property font h1: Qt.font({pointSize: 20, weight: Font.DemiBold})
    property font body: Qt.font({pointSize: 12})
}
