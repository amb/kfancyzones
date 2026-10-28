const {test} = require('node:test');
const assert = require('node:assert/strict');
const {layouts, selection, createContext, plain} = require('./helpers.cjs');

test('subdivision changes the default 4x2 to a row-major 8x4 grid', () => {
    const c = createContext({subdivideZones: true});
    const layout = c.config.layouts[0];
    assert.equal(layout.zones.length, 32);
    for (let i = 0; i < 32; i++) {
        assert.deepEqual(plain(selection.zoneGeometry(layout,
            {x: 0, y: 0, width: 1920, height: 1080}, i)),
            {x: i % 8 * 240, y: Math.floor(i / 8) * 270, width: 240, height: 270});
    }
    assert.deepEqual(plain(selection.combinedGeometry(layout,
        {x: 0, y: 0, width: 1920, height: 1080}, [0, 9])),
        {x: 0, y: 0, width: 480, height: 540});
});

test('columns, rows and gap settings change the active grid', () => {
    const layout = createContext({gridColumns: 3, gridRows: 1, gridGap: 12}).config.layouts[0];
    assert.equal(layout.zones.length, 3);
    const area = {x: 1920, y: -200, width: 1920, height: 1080};
    const a = selection.zoneGeometry(layout, area, 0), b = selection.zoneGeometry(layout, area, 1);
    assert.equal(a.x, 1932);
    assert.equal(b.x - (a.x + a.width), 12);
});

test('a saved legacy default grid does not override new grid controls', () => {
    const legacy = JSON.stringify([layouts.grid(4, 2, 0, false)]);
    const c = createContext({layoutsJson: legacy, subdivideZones: true, gridGap: 6});
    assert.equal(c.config.layouts[0].zones.length, 32);
    assert.equal(c.config.layouts[0].padding, 6);
});

test('custom JSON keeps layouts, zone metadata and custom spacing intact', () => {
    const custom = [{name: 'Custom', padding: 8, zones: [
        {x: 0, y: 0, width: 100, height: 100, applications: ['example']}]}];
    const c = createContext({useCustomLayout: true, layoutsJson: JSON.stringify(custom), subdivideZones: true});
    assert.deepEqual(plain(c.config.layouts), custom);
});

test('invalid enabled custom layouts fall back to plain 4x2, regardless of grid settings', () => {
    for (const json of ['{', 'null', '[]', '[{}]', '[{"zones":[]}]',
        '[{"zones":[{"x":0,"y":0,"width":101,"height":100}]}]']) {
        const c = createContext({useCustomLayout: true, layoutsJson: json, gridColumns: 2, gridRows: 1, gridGap: 12, subdivideZones: true});
        assert.equal(c.config.layouts[0].zones.length, 8);
        assert.equal(c.config.layouts[0].padding, 0);
        assert.equal(c.errors.length, 1);
        assert.match(c.customLayoutError, /default 4×2/);
    }
});

test('invalid grid settings use bounded defaults', () => {
    const c = createContext({gridColumns: 999, gridRows: -1, gridGap: -10});
    assert.equal(c.config.layouts[0].zones.length, 8);
    assert.equal(c.config.layouts[0].padding, 0);
});

test('retired behavior settings cannot change the fixed Shift/Ctrl workflow', () => {
    const c = createContext({enableZoneSelector: true, enableEdgeSnapping: true,
        enableMultiZoneSelection: false, zoneOverlayShowWhen: 0,
        rememberWindowGeometries: false, fadeWindowsWhileMoving: true,
        autoSnapAllNew: true, pollingRate: 1000});
    assert.equal(c.config.enableZoneSelector, false);
    assert.equal(c.config.enableEdgeSnapping, false);
    assert.equal(c.config.enableMultiZoneSelection, true);
    assert.equal(c.config.zoneOverlayShowWhen, 2);
    assert.equal(c.config.rememberWindowGeometries, true);
    assert.equal(c.config.fadeWindowsWhileMoving, false);
    assert.equal(c.config.autoSnapAllNew, false);
    assert.equal(c.config.pollingRate, 100);
});

test('disabled custom layout ignores stored invalid JSON and uses the subdivided grid', () => {
    const c = createContext({useCustomLayout: false, layoutsJson: '{invalid', subdivideZones: true});
    assert.equal(c.config.layouts[0].zones.length, 32);
    assert.equal(c.customLayoutError, '');
});

test('enabled empty JSON is invalid and uses plain 4x2', () => {
    const c = createContext({useCustomLayout: true, layoutsJson: '', subdivideZones: true});
    assert.equal(c.config.layouts[0].zones.length, 8);
    assert.match(c.customLayoutError, /default 4×2/);
});
