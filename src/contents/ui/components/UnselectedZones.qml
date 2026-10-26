import QtQuick

Item {
    property var config
    property var area
    property int layoutIndex
    property var preview: null

    property alias repeater: repeater

    Repeater {
        id: repeater
        model: config.layouts[layoutIndex].zones

        Rectangle {
            objectName: "zone-" + index
            property int zonePadding: config.layouts[layoutIndex].padding || 0

            x: modelData.x / 100 * (area.width - zonePadding) + zonePadding
            y: modelData.y / 100 * (area.height - zonePadding) + zonePadding
            width: modelData.width / 100 * (area.width - zonePadding) - zonePadding
            height: modelData.height / 100 * (area.height - zonePadding) - zonePadding

            // The preview uses global coordinates. Ignore subpixel rounding at
            // shared edges so a merely adjacent zone does not disappear.
            visible: !preview || !(
                Math.min(x + area.x + width, preview.x + preview.width)
                    - Math.max(x + area.x, preview.x) > 1
                && Math.min(y + area.y + height, preview.y + preview.height)
                    - Math.max(y + area.y, preview.y) > 1)

            color: "transparent"
            border.color: "#a0a0a0"
            border.width: 2
            radius: 6
        }
    }
}
