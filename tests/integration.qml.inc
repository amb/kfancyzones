    // Appended only to a temporary test copy of the production overlay.
    property TestNative.TestInput integrationInput: TestNative.TestInput { id: testInput }
    property var integrationWindow: null
    property var integrationNeighbor: null
    property bool integrationKeepAbove: false
    property var integrationOriginal: null
    property var integrationFreeGeometry: null
    property bool integrationSubdivide: false
    property int integrationCellWidth: integrationSubdivide ? 240 : 480
    property int integrationCellHeight: integrationSubdivide ? 270 : 540
    property int integrationNextX: integrationSubdivide ? 350 : 600
    property int integrationStep: 0
    function integrationAssert(condition, message) {
        if (!condition) throw new Error(message);
    }
    property Timer integrationTimer: Timer {
        interval: 250
        running: true
        repeat: true
        onTriggered: {
            try {
                const w = mainDialog.integrationWindow;
                switch (mainDialog.integrationStep) {
                case 0:
                    integrationWindow = Workspace.stackingOrder.find(client => client.caption === "KFancyZones integration target");
                    if (!integrationWindow) return;
                    integrationNeighbor = Workspace.stackingOrder.find(client => client.caption === "KFancyZones integration neighbor");
                    if (!integrationNeighbor) return;
                    integrationKeepAbove = integrationWindow.keepAbove;
                    integrationAssert(inputState.available, "native input backend unavailable");
                    integrationAssert(config.layouts[0].zones.length === (integrationSubdivide ? 32 : 8), "configured grid size is wrong");
                    integrationOriginal = integrationWindow.frameGeometry;
                    Workspace.activeWindow = integrationWindow;
                    testInput.pointer(100,100);
                    testInput.button(true);
                    Workspace.slotWindowMove();
                    break;
                case 1:
                    integrationAssert(w.move && moving, "window move not active");
                    integrationAssert(!mainDialog.visible, "zones are visible without Shift");
                    testInput.pointer(100,100);
                    testInput.key(42,true); // left Shift
                    testInput.key(29,true); // left Ctrl
                    break;
                case 2:
                    integrationAssert(shiftDrag, "Shift state did not reach QML");
                    integrationAssert(snapPreview.visible && snapPreview.width === integrationCellWidth, "single-zone final rectangle is missing");
                    integrationAssert((inputState.modifiers & Qt.ControlModifier) !== 0, "Ctrl state did not reach QML");
                    integrationAssert(overlayWindow !== null, "overlay window not found for stacking");
                    integrationAssert(Workspace.stackingOrder.indexOf(w) > Workspace.stackingOrder.indexOf(overlayWindow), "dragged window is below painted rectangles");
                    integrationAssert(Workspace.stackingOrder.indexOf(integrationNeighbor) < Workspace.stackingOrder.indexOf(overlayWindow), "overlay is below the rest of the desktop");
                    integrationAssert(w.keepAbove === integrationKeepAbove, "always-on-top setting was changed");
                    testInput.pointer(integrationNextX,100);
                    break;
                case 3:
                    console.info("KFANCYZONES_NATIVE_OUTLINE_SHOWS: " + testInput.outlineShows);
                    integrationAssert(testInput.outlineShows === 0, "native tile preview was created during zone selection");
                    integrationAssert(highlightedZones.includes(0) && highlightedZones.includes(1), "multi-zone preview did not expand");
                    integrationAssert(selectionGeometry.width === integrationCellWidth * 2, "combined preview width is incorrect: "
                        + JSON.stringify({geometry:selectionGeometry,zones:highlightedZones,area:clientArea,cursor:Workspace.cursorPos}));
                    integrationAssert(snapPreview.visible && snapPreview.width === integrationCellWidth * 2 && snapPreview.height === integrationCellHeight, "final rectangle does not match the combined snap area");
                    integrationAssert(snapPreview.color.a > 0 && Qt.colorEqual(snapPreview.border.color,"#69db8f"), "final rectangle is not filled green");
                    const zoneItems = repeaterLayout.itemAt(currentLayout).repeater;
                    integrationAssert(!zoneItems.itemAt(0).visible && !zoneItems.itemAt(1).visible, "covered gray outlines remain visible");
                    integrationAssert(zoneItems.itemAt(2).visible && zoneItems.itemAt(4).visible, "adjacent gray outlines were hidden");
                    testInput.button(false);
                    break;
                case 4:
                    integrationAssert(stackedClient === null && stackedOverlay === null, "stacking constraint remains after drop");
                    integrationAssert(w.tile === null, "window remains linked to a native tile");
                    integrationAssert(Math.abs(w.frameGeometry.width - integrationCellWidth * 2) <= 1, "combined drop width is incorrect: " + JSON.stringify(w.frameGeometry));
                    integrationAssert(Math.abs(w.frameGeometry.height - integrationCellHeight) <= 1, "combined drop height is incorrect");
                    integrationAssert(w.oldGeometry.width === integrationOriginal.width, "restore geometry was overwritten by native tiling");
                    testInput.key(29,false);
                    testInput.key(42,false);
                    testInput.pointer(100,100);
                    testInput.button(true);
                    Workspace.slotWindowMove();
                    break;
                case 5:
                    testInput.pointer(100,100);
                    testInput.key(42,true);
                    testInput.key(29,true);
                    break;
                case 6:
                    testInput.pointer(integrationNextX,100);
                    break;
                case 7:
                    testInput.key(29,false);
                    break;
                case 8:
                    integrationAssert(highlightedZones.length === 1 && highlightedZones[0] === 1, "Ctrl release did not collapse preview");
                    testInput.key(1,true); // Escape: native KWin cancels the move.
                    break;
                case 9:
                    integrationAssert(!w.move && !moving, "Escape did not finish the move");
                    integrationAssert(w.tile === null, "Escape attached a native tile");
                    integrationAssert(Math.abs(w.frameGeometry.width - integrationCellWidth * 2) <= 1, "Escape did not restore original geometry");
                    testInput.key(1,false);
                    testInput.key(42,false);
                    testInput.button(false);
                    testInput.pointer(100,100);
                    testInput.button(true);
                    Workspace.slotWindowMove();
                    break;
                case 10:
                    testInput.pointer(100,100);
                    testInput.key(42,true);
                    testInput.key(29,true);
                    break;
                case 11:
                    testInput.pointer(2020,100);
                    break;
                case 12:
                    integrationAssert(highlightedZones.length === 1 && highlightedZones[0] === 0, "selection leaked across outputs");
                    testInput.pointer(1920 + integrationNextX,100);
                    break;
                case 13:
                    integrationAssert(selectionGeometry.x === 1920 && selectionGeometry.width === integrationCellWidth * 2, "secondary monitor preview geometry is wrong");
                    integrationAssert(mainDialog.x === Workspace.virtualScreenGeometry.x, "overlay is not aligned to virtual desktop");
                    testInput.button(false);
                    break;
                case 14:
                    integrationAssert(w.tile === null, "secondary monitor drop is tiled");
                    integrationAssert(Math.abs(w.frameGeometry.x - 1920) <= 1 && Math.abs(w.frameGeometry.width - integrationCellWidth * 2) <= 1, "secondary monitor drop has incorrect coordinates");
                    testInput.key(29,false);
                    testInput.key(42,false);
                    testInput.pointer(2020,100);
                    testInput.button(true);
                    Workspace.slotWindowMove();
                    break;
                case 15:
                    testInput.pointer(2020,100);
                    testInput.key(42,true);
                    testInput.key(29,true);
                    break;
                case 16:
                    testInput.pointer(1920 + integrationNextX,100);
                    break;
                case 17:
                    integrationAssert(highlightedZones.length === 2, "selection did not accumulate before cancellation");
                    testInput.key(42,false);
                    integrationAssert(highlightedZones.length === 0 && !mainDialog.visible, "Shift release did not immediately cancel zone mode");
                    integrationAssert(w.move, "Shift release cancelled the window move");
                    integrationAssert(stackedClient === null && stackedOverlay === null, "stacking constraint remains after Shift release");
                    testInput.pointer(2800,300);
                    break;
                case 18:
                    integrationFreeGeometry = {
                        x:w.frameGeometry.x, y:w.frameGeometry.y,
                        width:w.frameGeometry.width, height:w.frameGeometry.height
                    };
                    testInput.button(false);
                    break;
                case 19:
                    integrationAssert(testInput.outlineShows === 0, "native preview was created during a zone gesture");
                    integrationAssert(w.tile === null, "Shift-up before mouse-up left a native tile");
                    integrationAssert(Selection.matches(w.frameGeometry,integrationFreeGeometry), "Shift-up before mouse-up snapped instead of dropping freely");
                    testInput.key(29,false);
                    // A Shift already held when a move begins is also owned.
                    testInput.key(42,true);
                    testInput.button(true);
                    Workspace.slotWindowMove();
                    break;
                case 20:
                    testInput.pointer(2600,400);
                    break;
                case 21:
                    integrationAssert(mainDialog.visible && highlightedZones.length === 1, "Shift held before move did not select a zone");
                    integrationAssert(testInput.outlineShows === 0, "Shift held before move created native preview");
                    testInput.button(false);
                    testInput.key(42,false);
                    // Outside the owned filter, native tiling must remain available.
                    inputState.suppressNativeTiling = false;
                    testInput.button(true);
                    Workspace.slotWindowMove();
                    break;
                case 22:
                    testInput.key(42,true);
                    testInput.pointer(2700,450);
                    break;
                case 23:
                    integrationAssert(testInput.outlineShows > 0, "native Shift preview was suppressed with the filter disabled");
                    testInput.key(42,false);
                    testInput.button(false);
                    inputState.suppressNativeTiling = Qt.binding(() => mainDialog.moving);
                    testInput.writeSetting("subdivideZones", !integrationSubdivide);
                    break;
                case 24:
                    integrationAssert(config.layouts[0].zones.length === (integrationSubdivide ? 8 : 32), "saved subdivision change was not applied live");
                    testInput.writeSetting("layoutsJson", "{broken");
                    break;
                case 25:
                    integrationAssert(config.layouts[0].zones.length === (integrationSubdivide ? 8 : 32) && customLayoutError === "", "disabled custom JSON overrode the grid");
                    testInput.writeSetting("useCustomLayout", true);
                    break;
                case 26:
                    integrationAssert(config.layouts[0].zones.length === 8 && config.layouts[0].padding === 0, "invalid enabled custom JSON did not use plain 4x2");
                    integrationAssert(customLayoutError.length > 0, "invalid custom JSON has no diagnostic");
                    testInput.writeSetting("layoutsJson", '[{"name":"Custom","zones":[{"x":0,"y":0,"width":100,"height":100}]}]');
                    break;
                case 27:
                    integrationAssert(config.layouts[0].zones.length === 1 && customLayoutError === "", "valid custom JSON did not apply live");
                    testInput.writeSetting("useCustomLayout", false);
                    break;
                case 28:
                    integrationAssert(config.layouts[0].zones.length === (integrationSubdivide ? 8 : 32), "disabling custom mode did not restore the grid");
                    testInput.key(42,true);
                    testInput.button(true);
                    Workspace.slotWindowMove();
                    testInput.writeSetting("gridColumns", 2);
                    break;
                case 29:
                    integrationAssert(moving && configurationPending, "settings change during a drag was not deferred");
                    integrationAssert(config.layouts[0].zones.length === (integrationSubdivide ? 8 : 32), "settings changed the grid in the middle of a drag");
                    testInput.pointer(2200,200);
                    testInput.button(false);
                    testInput.key(42,false);
                    break;
                case 30:
                    integrationAssert(!configurationPending && config.layouts[0].zones.length === (integrationSubdivide ? 4 : 16), "deferred settings were not applied after the drag");
                    testInput.writeSetting("gridColumns", 4);
                    testInput.writeSetting("subdivideZones", integrationSubdivide);
                    testInput.writeSetting("layoutsJson", "");
                    console.log("KFANCYZONES_INTEGRATION: PASS");
                    stop();
                    break;
                }
                mainDialog.integrationStep++;
            } catch (error) {
                console.error("KFANCYZONES_INTEGRATION: FAIL: " + error.message);
                stop();
            }
        }
    }
