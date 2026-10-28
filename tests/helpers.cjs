const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const root = path.join(__dirname, '..');
const mainSource = fs.readFileSync(path.join(root, 'src/contents/ui/main.qml'), 'utf8');
const selection = vm.createContext({});
const layouts = vm.createContext({});
vm.runInContext(fs.readFileSync(path.join(root, 'src/contents/code/layouts.js'), 'utf8'), layouts);
vm.runInContext(fs.readFileSync(path.join(root, 'src/contents/code/selection.js'), 'utf8'), selection);

function createContext(overrides = {}) {
    const c = {
        Selection: selection, Layouts: layouts,
        config: {}, errors: [], currentLayout: 0,
        clientArea: {x: 0, y: 0, width: 1920, height: 1080},
        activeScreen: {name: 'screen-1'},
        moving: true, dragCancelled: false, dragOriginalGeometry: null, dragOriginalTile: null,
        draggedWindow: null, overlayWindow: null, stackedOverlay: null, stackedClient: null,
        highlightedZone: -1,
        zoneSelection: {context: '', zones: [], hovered: -1, extending: false},
        zoneSelector: {expanded: false},
        mainDialog: {visible: false}, shown: true,
        inputState: {available: true, modifiers: 0},
        Workspace: {currentDesktop: {id: 'desktop-1'}, currentActivity: 'activity-1',
            cursorPos: {x: 100, y: 100}, stackingOrder: [], hideOutline() {},
            constrain() {}, unconstrain() {}, raiseWindow() {}},
        KWin: {readConfig(key, fallback) {return key in overrides ? overrides[key] : fallback;}},
        Qt: {ShiftModifier: 0x02000000, ControlModifier: 0x04000000,
            rect: (x, y, width, height) => ({x, y, width, height})},
        log() {}, refreshClientArea() {}
    };
    Object.defineProperty(c, 'shiftDrag', {get() {
        return c.config.enableZoneOverlay
            && c.inputState.available && (c.inputState.modifiers & c.Qt.ShiftModifier) !== 0;
    }});
    Object.defineProperty(c, 'highlightedZones', {get() {return selection.selected(c.zoneSelection);}});
    vm.createContext(c);
    for (const name of ['loadConfig', 'checkFilter', 'resetZoneSelection', 'releaseOverlayStacking',
        'updateOverlayStacking', 'zoneDragEnabled', 'updateZoneSelection',
        'updateShiftSelection', 'moveClientToZone', 'moveClientToZones', 'saveClientProperties',
        'matchZone', 'finishZoneDrop', 'cancelZoneDrop']) {
        const match = mainSource.match(new RegExp('^    function ' + name + '\\([^]*?^    }', 'm'));
        if (!match) throw new Error('Missing QML method: ' + name);
        vm.runInContext(match[0], c);
    }
    c.loadConfig();
    return c;
}

function client() {
    return {normalWindow: true, resourceClass: 'test-app', popupWindow: false, skipTaskbar: false,
        zone: -1, layout: -1, zones: [], tile: null,
        frameGeometry: {x: 100, y: 100, width: 600, height: 400},
        setMaximize() {}};
}

const plain = value => JSON.parse(JSON.stringify(value));
const nativeTile = () => ({unmanage(w) {w.tile=null;}, manage(w) {w.tile=this;}});
module.exports = {root, selection, layouts, createContext, client, plain, nativeTile};
