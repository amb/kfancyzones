    // Appended only to a temporary test copy of the production overlay.
    property TestNative.TestInput integrationInput: TestNative.TestInput { id: testInput }
    property var integrationWindow: null
    property var integrationOriginal: null
    property var integrationFreeGeometry: null
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
                    integrationWindow = Workspace.stackingOrder.find(client => client.caption === "KZones integration target");
                    if (!integrationWindow) return;
                    integrationAssert(inputState.available, "native input backend unavailable");
                    integrationAssert(config.layouts[0].zones.length === 8, "default is not 4x2");
                    integrationOriginal = integrationWindow.frameGeometry;
                    Workspace.activeWindow = integrationWindow;
                    testInput.pointer(100,100);
                    testInput.button(true);
                    Workspace.slotWindowMove();
                    break;
                case 1:
                    integrationAssert(w.move && moving, "window move not active");
                    integrationAssert(config.zoneOverlayShowWhen === 2 && !mainDialog.visible, "zones are visible without Shift");
                    testInput.pointer(100,100);
                    testInput.key(42,true); // left Shift
                    testInput.key(29,true); // left Ctrl
                    break;
                case 2:
                    integrationAssert(shiftDrag, "Shift state did not reach QML");
                    integrationAssert((inputState.modifiers & Qt.ControlModifier) !== 0, "Ctrl state did not reach QML");
                    testInput.pointer(600,100);
                    break;
                case 3:
                    integrationAssert(highlightedZones.includes(0) && highlightedZones.includes(1), "multi-zone preview did not expand");
                    integrationAssert(selectionGeometry.width === 960, "combined preview width is incorrect: "
                        + JSON.stringify({geometry:selectionGeometry,zones:highlightedZones,area:clientArea,cursor:Workspace.cursorPos}));
                    testInput.button(false);
                    break;
                case 4:
                    integrationAssert(w.tile === null, "window remains linked to a native tile");
                    integrationAssert(Math.abs(w.frameGeometry.width - 960) <= 1, "combined drop width is incorrect: " + JSON.stringify(w.frameGeometry));
                    integrationAssert(Math.abs(w.frameGeometry.height - 540) <= 1, "combined drop height is incorrect");
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
                    testInput.pointer(600,100);
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
                    integrationAssert(Math.abs(w.frameGeometry.width - 960) <= 1, "Escape did not restore original geometry");
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
                    testInput.pointer(2520,100);
                    break;
                case 13:
                    integrationAssert(selectionGeometry.x === 1920 && selectionGeometry.width === 960, "secondary monitor preview geometry is wrong");
                    integrationAssert(mainDialog.x === Workspace.virtualScreenGeometry.x, "overlay is not aligned to virtual desktop");
                    testInput.button(false);
                    break;
                case 14:
                    integrationAssert(w.tile === null, "secondary monitor drop is tiled");
                    integrationAssert(Math.abs(w.frameGeometry.x - 1920) <= 1 && Math.abs(w.frameGeometry.width - 960) <= 1, "secondary monitor drop has incorrect coordinates");
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
                    testInput.pointer(2520,100);
                    break;
                case 17:
                    integrationAssert(highlightedZones.length === 2, "selection did not accumulate before cancellation");
                    testInput.key(42,false);
                    integrationAssert(highlightedZones.length === 0 && !mainDialog.visible, "Shift release did not immediately cancel zone mode");
                    integrationAssert(w.move, "Shift release cancelled the window move");
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
                    integrationAssert(w.tile === null, "Shift-up before mouse-up left a native tile");
                    integrationAssert(Selection.matches(w.frameGeometry,integrationFreeGeometry), "Shift-up before mouse-up snapped instead of dropping freely");
                    testInput.key(29,false);
                    console.log("KZONES_INTEGRATION: PASS");
                    stop();
                    break;
                }
                mainDialog.integrationStep++;
            } catch (error) {
                console.error("KZONES_INTEGRATION: FAIL: " + error.message);
                stop();
            }
        }
    }
