import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15

Item {
    id: root
    property color tint: Qt.rgba(1,1,1,0.4)
    anchors.fill: parent

    Rectangle { anchors.fill: parent; color: "transparent" }
    FastBlur {
        anchors.fill: parent
        source: ShaderEffectSource { sourceItem: parent; hideSource: true; recursive: true }
        radius: 32
    }
    Rectangle { anchors.fill: parent; color: root.tint; border.color: "#40FFFFFF"; radius: 16 }
}
