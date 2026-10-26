// Audit reproductions of known defects in upstream v0.9.2.
// KWin and QML objects are mocked; this does not touch the live desktop.
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const path = require('node:path');
const {execFileSync} = require('node:child_process');
const source = execFileSync('git', ['show', 'v0.9.2:src/contents/ui/main.qml'], {
  cwd: path.join(__dirname, '..'), encoding: 'utf8'
});
const methods = ['loadConfig', 'checkFilter', 'matchZone', 'moveClientToZone', 'saveClientProperties', 'moveClientToClosestZone', 'moveAllClientsToClosestZone'];
function createContext(overrides = {}) {
  const context = {
    config: {}, errors: [], currentLayout: 0, log() {}, refreshClientArea() {},
    KWin: { readConfig(key, fallback) { return key in overrides ? overrides[key] : fallback; } },
    Qt: { point: (x,y) => ({x,y}), rect: (x,y,width,height) => ({x,y,width,height}) },
    clientArea: {x:0,y:0,width:1920,height:1080},
    Workspace: {currentDesktop: 'desktop-1',currentActivity: 'activity-1',stackingOrder:[]},
  };
  vm.createContext(context);
  for (const name of methods) {
    const match = source.match(new RegExp('^    function '+name+'\\([^]*?^    }', 'm'));
    assert(match, name);
    vm.runInContext(match[0], context);
  }
  context.loadConfig();
  context.repeaterLayout = {itemAt(index) {
    const layout = context.config.layouts[index];
    return {repeater: {itemAt(zoneIndex) {
      const zone = layout.zones[zoneIndex];
      if (!zone) return null; // QML Repeater.itemAt for an invalid index.
      const pad = layout.padding || 0;
      const area = context.clientArea;
      return {
        mapToGlobal() { return {
          x:area.x+zone.x/100*(area.width-pad)+pad,
          y:area.y+zone.y/100*(area.height-pad)+pad,
        }; },
        width:zone.width/100*(area.width-pad)-pad,
        height:zone.height/100*(area.height-pad)-pad,
      };
    }}};
  }};
  return context;
}
function client(geometry = {x:0,y:0,width:400,height:600}) {
  return {zone:-1,layout:-1,normalWindow:true,popupWindow:false,skipTaskbar:false,
    resourceClass:'test-app',frameGeometry:geometry,setMaximize() {}};
}
function check(name, fn) { fn(); console.log('CONFIRMED:',name); }

check('matchZone fails for an exactly snapped window on a monitor at x=1920', () => {
  const c = createContext(); c.clientArea.x=1920;
  const w = client(); c.moveClientToZone(w,0);
  assert.equal(w.frameGeometry.x,1920);
  c.matchZone(w); assert.equal(w.zone,-1);
});
check('matchZone fails after padded snapping produces fractional coordinates rounded to pixels', () => {
  const c = createContext(); c.config.layouts[0].padding=7;
  const w = client(); c.moveClientToZone(w,1);
  c.matchZone(w); assert.equal(w.zone,-1);
});
check('a registered shortcut for a nonexistent zone throws after saving the invalid zone', () => {
  const c = createContext(); const w=client();
  assert.throws(()=>c.moveClientToZone(w,8), /mapToGlobal/);
  assert.equal(w.zone,8);
});
check('syntactically valid but empty layout JSON is accepted and then throws', () => {
  const c = createContext({layoutsJson:'[]'});
  assert.equal(c.errors.length,0);
  assert.throws(()=>c.matchZone(client()), /zones/);
});
check('new-window direct snapping does not save its old geometry when zone is undefined', () => {
  const c = createContext(); const w=client(); delete w.zone;
  c.moveClientToZone(w,0);
  assert.equal(w.oldGeometry,undefined);
});
check('Snap all windows processes windows on other monitors and desktops', () => {
  const c = createContext();
  const w1=client(),w2=client({x:3000,y:10,width:400,height:600});
  w2.desktop='desktop-2';w2.screen='other-screen';
  c.Workspace.stackingOrder=[w1,w2];
  c.moveAllClientsToClosestZone();
  assert(w2.frameGeometry.x<1920);
});
