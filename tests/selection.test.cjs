const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {root, selection: s, createContext, plain} = require('./helpers.cjs');
const area = {x: 0, y: 0, width: 1920, height: 1080};

test('fresh defaults use Shift-drag, eight zones, and no mini-grid selector', () => {
    const c = createContext();
    assert.equal(c.config.enableZoneSelector, false);
    assert.equal(c.config.zoneOverlayShowWhen, 2);
    assert.equal(c.config.enableMultiZoneSelection, true);
    const layout = c.config.layouts[0];
    assert.equal(layout.zones.length, 8);
    for (let i = 0; i < 8; i++) {
        assert.deepEqual(plain(s.zoneGeometry(layout, area, i)), {
            x: (i % 4) * 480, y: Math.floor(i / 4) * 540, width: 480, height: 540
        });
    }
    const xml = fs.readFileSync(path.join(root, 'src/contents/config/main.xml'), 'utf8');
    assert.match(xml, /<entry name="gridColumns"[^]*?<default>4<\/default>/);
    assert.match(xml, /<entry name="gridRows"[^]*?<default>2<\/default>/);
    assert.match(xml, /<entry name="subdivideZones"[^]*?<default>false<\/default>/);
});

test('holding Ctrl+Shift accumulates visited zones and does not duplicate them', () => {
    let state = s.update(null, 0, 'screen/layout', false);
    state = s.update(state, 1, 'screen/layout', true);
    state = s.update(state, 2, 'screen/layout', true);
    state = s.update(state, 1, 'screen/layout', true);
    assert.deepEqual(plain(s.selected(state)), [0, 1, 2]);
    assert.deepEqual(plain(s.combinedGeometry(createContext().config.layouts[0], area, state.zones)),
        {x: 0, y: 0, width: 1440, height: 540});
});

test('releasing Ctrl collapses the accumulated selection to the hovered zone', () => {
    let state = s.update(null, 0, 'a', true);
    state = s.update(state, 1, 'a', true);
    assert.deepEqual(plain(s.selected(s.update(state, 1, 'a', false))), [1]);
});

test('switching screen or layout starts a fresh selection', () => {
    const initial = s.update(null, 0, 'screen1/layout1', true);
    const expanded = s.update(initial, 1, 'screen1/layout1', true);
    assert.deepEqual(plain(s.selected(s.update(expanded, 4, 'screen2/layout1', true))), [4]);
    assert.deepEqual(plain(s.selected(s.update(expanded, 2, 'screen1/layout2', true))), [2]);
});

test('leaving all zones hides the selection and prevents a stale drop', () => {
    const selected = s.update(null, 0, 'a', true);
    const outside = s.update(selected, -1, 'a', true);
    assert.deepEqual(plain(s.selected(outside)), []);
    assert.deepEqual(plain(s.selected(s.update(outside, 1, 'a', true))), [0, 1]);
});

test('a rectangular window spans both rows and includes space between selected cells', () => {
    const layout = createContext().config.layouts[0];
    assert.deepEqual(plain(s.combinedGeometry(layout, area, [1, 6])),
        {x: 480, y: 0, width: 960, height: 1080});
});

test('combined geometry respects padding, fractional pixels, and negative output origins', () => {
    const layout = createContext().config.layouts[0];
    layout.padding = 7;
    const offset = {x: -1919, y: -321, width: 1919, height: 1081};
    const first = s.zoneGeometry(layout, offset, 0);
    const last = s.zoneGeometry(layout, offset, 5);
    const combined = s.combinedGeometry(layout, offset, [0, 5]);
    assert.equal(combined.x, first.x);
    assert.equal(combined.y, first.y);
    assert.equal(combined.width, last.x + last.width - first.x);
    assert.equal(combined.height, last.y + last.height - first.y);
    assert(Object.values(combined).every(Number.isInteger));
    assert(s.matches(combined, s.combinedGeometry(layout, offset, [0, 5])));
});

test('zone hit testing works on offset outputs and across padded boundaries', () => {
    const layout = createContext().config.layouts[0];
    layout.padding = 12;
    const offset = {...area, x: 1920, y: -200};
    assert.equal(s.zoneAt(layout, offset, {x: 1921, y: -199}), 0);
    assert.equal(s.zoneAt(layout, offset, {x: 2400, y: -199}), 1);
    assert.equal(s.zoneAt(layout, offset, {x: 3839, y: 879}), 7);
    assert.equal(s.zoneAt(layout, offset, {x: 1919, y: 100}), -1);
});

test('invalid zones and malformed dimensions cannot produce a snap rectangle', () => {
    const layout = createContext().config.layouts[0];
    for (const indices of [[], [8], [-1], [0, 99], [NaN], [null], [0.5]]) {
        assert.equal(s.combinedGeometry(layout, area, indices), null);
    }
    layout.zones[0].width = 'bad';
    assert.equal(s.combinedGeometry(layout, area, [0]), null);
});

test('an enabled custom layout takes precedence over the grid', () => {
    const custom = [{name: 'My layout', zones: [{x:0,y:0,width:100,height:100}]}];
    assert.deepEqual(plain(createContext({useCustomLayout: true, layoutsJson: JSON.stringify(custom)}).config.layouts), custom);
});
