const {test} = require('node:test');
const assert = require('node:assert/strict');
const {createContext, client, plain, nativeTile} = require('./helpers.cjs');

function hold(c, ctrl = true) {
    c.inputState.modifiers = c.Qt.ShiftModifier | (ctrl ? c.Qt.ControlModifier : 0);
}

test('multi-zone drop detaches the native tile and resizes only the dragged window', () => {
    const c = createContext();
    const w = client(), neighbor = client();
    const unchanged = plain(neighbor);
    w.tile = nativeTile();
    hold(c);
    c.Workspace.cursorPos = {x:100,y:100}; c.updateShiftSelection();
    c.Workspace.cursorPos = {x:600,y:100}; c.updateShiftSelection();
    c.finishZoneDrop(w);
    assert.deepEqual(plain(w.frameGeometry), {x:0,y:0,width:960,height:540});
    assert.equal(w.tile, null);
    assert.deepEqual(plain(w.zones), [0,1]);
    assert.deepEqual(plain(neighbor), unchanged);
});

test('drop samples the final cursor position even before the next polling tick', () => {
    const c = createContext(); const w = client();
    hold(c);
    c.Workspace.cursorPos = {x:100,y:100}; c.updateShiftSelection();
    c.Workspace.cursorPos = {x:1700,y:100};
    c.finishZoneDrop(w);
    assert.deepEqual(plain(w.frameGeometry), {x:0,y:0,width:1920,height:540});
});

test('Ctrl without Shift does not accumulate and releasing Ctrl restores single-zone snapping', () => {
    const c = createContext();
    c.inputState.modifiers = c.Qt.ControlModifier;
    c.updateZoneSelection(0); c.updateZoneSelection(1);
    assert.deepEqual(plain(c.highlightedZones), [1]);
    hold(c); c.updateZoneSelection(2);
    assert.deepEqual(plain(c.highlightedZones), [1,2]);
    hold(c,false); c.updateZoneSelection(2);
    assert.deepEqual(plain(c.highlightedZones), [2]);
});

test('disabling the feature or an unavailable native backend leaves single-zone operation', () => {
    for (const unavailable of [false,true]) {
        const c = createContext({enableMultiZoneSelection: unavailable});
        c.inputState.available = !unavailable;
        hold(c); c.updateZoneSelection(0); c.updateZoneSelection(1);
        assert.deepEqual(plain(c.highlightedZones), [1]);
    }
});

test('Escape cancellation does not change window geometry or metadata', () => {
    const c = createContext(); const w = client();
    const unchanged = plain(w);
    hold(c); c.updateZoneSelection(0); c.updateZoneSelection(1);
    c.dragCancelled = true;
    c.finishZoneDrop(w);
    assert.deepEqual(plain(w), unchanged);
});

test('dropping outside the output does not use the old multi-zone rectangle', () => {
    const c = createContext(); const w = client();
    const geometry = plain(w.frameGeometry);
    hold(c); c.updateZoneSelection(0); c.updateZoneSelection(1);
    c.Workspace.cursorPos = {x:-1,y:100};
    c.finishZoneDrop(w);
    assert.deepEqual(plain(w.frameGeometry), geometry);
    assert.equal(w.zone,-1);
});

test('invalid zone shortcuts are no-ops and preserve native tiles and remembered state', () => {
    const c = createContext(); const w = client();
    const unchanged = plain(w);
    c.moveClientToZone(w,8);
    c.moveClientToZones(w,[0,99]);
    assert.deepEqual(plain(w), unchanged);
});

test('the first drop remembers geometry from before KWin applied its native Shift tile', () => {
    const c = createContext(); const w = client();
    delete w.zone;
    c.dragOriginalGeometry = {x:50,y:60,width:700,height:500};
    w.frameGeometry = {x:0,y:0,width:480,height:540};
    w.tile = nativeTile();
    hold(c); c.Workspace.cursorPos = {x:100,y:100};
    c.finishZoneDrop(w);
    assert.deepEqual(plain(w.oldGeometry), plain(c.dragOriginalGeometry));
});

test('moving an already snapped window preserves its original restore geometry', () => {
    const c = createContext(); const w = client();
    const original = plain(w.frameGeometry);
    c.moveClientToZones(w,[0,1]);
    c.dragOriginalGeometry = plain(w.frameGeometry);
    hold(c); c.Workspace.cursorPos = {x:1200,y:700};
    c.finishZoneDrop(w);
    assert.deepEqual(plain(w.oldGeometry), original);
});

test('zone recognition retains combined zones on padded secondary monitors', () => {
    const c = createContext(); const w = client();
    c.clientArea = {x:1920,y:-100,width:1919,height:1081};
    c.config.layouts[0].padding = 7;
    c.moveClientToZones(w,[0,1,5]);
    c.matchZone(w);
    assert.deepEqual(plain(w.zones), [0,1,5]);
    w.frameGeometry.width -= 10;
    c.matchZone(w);
    assert.equal(w.zone,-1);
    assert.deepEqual(plain(w.zones), []);
});

test('cancelling restores geometry and removes the native tile added by Shift+Escape', () => {
    const c = createContext(); const w = client();
    c.dragOriginalGeometry = plain(w.frameGeometry);
    w.tile = nativeTile();
    w.frameGeometry = {x:0,y:0,width:480,height:1080};
    c.cancelZoneDrop(w);
    assert.equal(w.tile,null);
    assert.deepEqual(plain(w.frameGeometry), plain(c.dragOriginalGeometry));
});

test('cancelling a move of a native-tiled window preserves its original tile', () => {
    const c = createContext(); const w = client();
    c.dragOriginalTile = nativeTile();
    w.tile = nativeTile();
    c.cancelZoneDrop(w);
    assert.equal(w.tile,c.dragOriginalTile);
});

test('Shift release immediately hides zones and clears selection while dragging continues', () => {
    const c = createContext();
    hold(c); c.Workspace.cursorPos = {x:100,y:100}; c.updateShiftSelection();
    c.Workspace.cursorPos = {x:600,y:100}; c.updateShiftSelection();
    assert.equal(c.highlightedZones.length,2);
    c.inputState.modifiers = c.Qt.ControlModifier;
    c.updateShiftSelection();
    assert.deepEqual(plain(c.highlightedZones),[]);
    assert.equal(c.mainDialog.visible,false);
    assert.equal(c.moving,true);
});

test('Shift-up before mouse-up cancels snapping even without a polling tick', () => {
    const c = createContext(); const w = client();
    const original = plain(w.frameGeometry);
    hold(c); c.Workspace.cursorPos = {x:100,y:100}; c.updateShiftSelection();
    c.Workspace.cursorPos = {x:600,y:100}; c.updateShiftSelection();
    c.inputState.modifiers = c.Qt.ControlModifier;
    c.finishZoneDrop(w);
    assert.deepEqual(plain(w.frameGeometry),original);
    assert.deepEqual(plain(w.zones),[]);
});

test('mouse-up before Shift-up accepts the rectangle and later Shift-up does not undo it', () => {
    const c = createContext(); const w = client();
    hold(c); c.Workspace.cursorPos = {x:100,y:100}; c.updateShiftSelection();
    c.Workspace.cursorPos = {x:600,y:100}; c.updateShiftSelection();
    c.finishZoneDrop(w);
    c.moving = false;
    c.inputState.modifiers = c.Qt.ControlModifier;
    c.updateShiftSelection();
    assert.deepEqual(plain(w.frameGeometry),{x:0,y:0,width:960,height:540});
});

test('pressing Shift again after cancellation starts a fresh selection', () => {
    const c = createContext();
    hold(c); c.updateZoneSelection(0); c.updateZoneSelection(1);
    c.inputState.modifiers = c.Qt.ControlModifier; c.updateShiftSelection();
    hold(c); c.Workspace.cursorPos = {x:1700,y:800}; c.updateShiftSelection();
    assert.deepEqual(plain(c.highlightedZones),[7]);
});
