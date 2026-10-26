import QtQuick
import QtTest
import "../../src/contents/ui/components" as Components

TestCase {
    name: "UnselectedZones"
    when: windowShown
    visible: true
    width: 800
    height: 400

    Components.UnselectedZones {
        id: overlay
        area: Qt.rect(0, 0, 800, 400)
        layoutIndex: 0
        config: ({layouts: [{zones: [
            {x:0,y:0,width:25,height:50}, {x:25,y:0,width:25,height:50},
            {x:50,y:0,width:25,height:50}, {x:75,y:0,width:25,height:50},
            {x:0,y:50,width:25,height:50}, {x:25,y:50,width:25,height:50},
            {x:50,y:50,width:25,height:50}, {x:75,y:50,width:25,height:50}
        ]}]})
    }

    function init() {
        overlay.preview = null;
        overlay.area = Qt.rect(0, 0, 800, 400);
    }

    function test_gridUsesUnfilledGrayOutlines() {
        compare(overlay.repeater.count, 8);
        for (let i = 0; i < 8; i++) {
            const zone = overlay.repeater.itemAt(i);
            compare(zone.x, i % 4 * 200);
            compare(zone.y, Math.floor(i / 4) * 200);
            compare(zone.width, 200);
            compare(zone.height, 200);
            compare(zone.color, Qt.rgba(0,0,0,0));
            compare(zone.border.color, Qt.rgba(160/255, 160/255, 160/255, 1));
            compare(zone.children.length, 0); // No mini-grid indicators or icons.
        }
    }

    function test_zoneGridContainsNoSelectionHighlight() {
        for (let i = 0; i < 8; i++) {
            const zone = overlay.repeater.itemAt(i);
            compare(zone.border.color, "#a0a0a0");
            compare(zone.border.width, 2);
            compare(zone.color, Qt.rgba(0,0,0,0));
        }
    }

    function test_previewHidesCoveredZonesAndPreservesAdjacentZones() {
        overlay.preview = Qt.rect(0, 0, 400, 200);
        for (let i = 0; i < 8; i++) {
            compare(overlay.repeater.itemAt(i).visible, i !== 0 && i !== 1);
        }
    }

    function test_boundingRectangleHidesInterveningZonesToo() {
        overlay.preview = Qt.rect(0, 0, 600, 400);
        for (let i = 0; i < 8; i++) {
            compare(overlay.repeater.itemAt(i).visible, i === 3 || i === 7);
        }
    }

    function test_partialOverlapHidesTheWholeGrayRectangle() {
        overlay.preview = Qt.rect(150, 50, 100, 100);
        compare(overlay.repeater.itemAt(0).visible, false);
        compare(overlay.repeater.itemAt(1).visible, false);
        compare(overlay.repeater.itemAt(2).visible, true);
    }

    function test_previewCoordinatesWorkOnOffsetOutputs() {
        overlay.area = Qt.rect(1920, -200, 800, 400);
        overlay.preview = Qt.rect(1920, -200, 400, 200);
        compare(overlay.repeater.itemAt(0).visible, false);
        compare(overlay.repeater.itemAt(1).visible, false);
        compare(overlay.repeater.itemAt(2).visible, true);
        compare(overlay.repeater.itemAt(4).visible, true);
    }

    function test_clearingPreviewRestoresAllGrayOutlines() {
        overlay.preview = Qt.rect(0, 0, 800, 400);
        overlay.preview = null;
        for (let i = 0; i < 8; i++) compare(overlay.repeater.itemAt(i).visible, true);
    }
}
