# Local KZones audit

Audited 2026-10-26 for a PowerToys FancyZones-style workflow: predefined drop
zones, keyboard placement, and independent resizing of adjacent windows.

This document records the original upstream 0.9.2 baseline. The local branch
now adds a native modifier-state helper, Ctrl+Shift multi-zone selection, and
a 4x2 default grid. Related local fixes address zone recognition, invalid-zone
shortcuts, and missing initial restore geometry. See `LOCAL_DEVELOPMENT.md`
for the current behavior and verification; the findings below remain a record
of the original installed version.

## Baseline and scope

- Repository: this checkout (originally named `kzones`).
- Working branch: `local/fancyzones`.
- Baseline: upstream `v0.9.2`, commit
  `48fc4872eadfe3fb7bd19d4426b43c43cd617750`.
- Upstream remote: `https://github.com/gerritdevriese/kzones.git`.
- All 12 installed files exactly matched this release, including metadata,
  configuration, QML components, and the placeholder JavaScript entry point.
- Installed environment: Plasma/KWin 6.6.6, Kubuntu/Ubuntu 26.04.
- Upstream main was at `e261b5f` and advertised version 0.9.3 when cloned.
  This audit does not cover that newer development version.

Reviewed all installed source and configuration files. JavaScript reproductions
use actual functions extracted from `src/contents/ui/main.qml`, with mocked KWin
and QML objects. They do not execute in the compositor or move real windows.
No live desktop integration test was performed.

## Security assessment

No network requests, telemetry, shell/process execution, dynamic code evaluation,
or file-writing operations were found in the installed script. Its only D-Bus
call targets `org.kde.plasmashell`, `/org/kde/osdService`, `showText`
(`src/contents/ui/main.qml:790`). This displays an on-screen message.

The script reads window geometry and application identity to arrange windows.
Its optional local debug overlay includes the active window title. Debug logging
and the debug overlay default to off. The polling timer runs only while the
overlay is shown during a window move, at a default interval of 100 ms.

This is a source review of this release, not a guarantee about KWin/Qt libraries
or future updates. No malicious behavior was identified.

## Findings

### 1. Medium: zone recognition fails on monitors away from the origin

`src/contents/ui/main.qml:170` (`matchZone`) compares global window coordinates
against zone coordinates without adding `clientArea.x` and `clientArea.y`.
A window snapped correctly on a monitor at x=1920 is subsequently recorded as
outside all zones. This breaks zone recognition after resize and at startup;
navigation may then behave as if the window was never snapped.

The same comparison uses exact floating-point equality, while snapping rounds
coordinates and dimensions. Padding or nonintegral zone sizes can therefore
break recognition even on the primary monitor.

Suggested fix: share one global, rounded geometry calculation between snapping
and recognition, with a small tolerance for application size constraints.

### 2. Medium: shortcuts targeting nonexistent zones throw and leave bad state

`src/contents/ui/components/Shortcuts.qml:85` registers shortcuts for zones 1-9
regardless of the current layout's zone count. `moveClientToZone`, at
`src/contents/ui/main.qml:221`, saves the requested zone before checking that
the repeater item exists, then calls `mapToGlobal` on null.

With the default three-zone layout, requesting zone 9 reproduces this error and
leaves `client.zone` set to 8. Subsequent operations can dereference that invalid
zone again.

Suggested fix: validate the window, layout, zone index, and delegate before
changing remembered state. Treat unavailable-zone shortcuts as harmless no-ops.

### 3. Medium: layout JSON is parsed without structural validation

`src/contents/ui/main.qml:52` catches JSON syntax errors but accepts values such
as `[]` or `null`, and layouts with missing or invalid zones. Later operations
dereference `config.layouts[currentLayout].zones` without checks.

An empty JSON array is accepted without an error message and then fails during
zone matching. The layout editor is a plain text field, so this can happen
through normal configuration editing.

Suggested fix: validate a nonempty layout array, nonempty zones, finite numeric
coordinates/dimensions, and positive dimensions before accepting the config.
Preserve the last valid layout or use defaults when validation fails.

### 4. Medium behavioral hazard: bulk snap relocates windows from other screens

`src/contents/ui/main.qml:369` loops over the entire workspace stacking order.
It does not restrict windows to the current output, desktop, activity, or visible
state. Every snap uses the active screen's client area and layout.

In isolation, a window positioned at x=3000 on another screen is snapped into the
active 1920-pixel-wide screen. Other-desktop windows are processed too. Desktop
membership changes were not tested. This is a risk when invoking the default
`Meta+Space` "Snap all windows" shortcut.

Suggested fix: define bulk-action scope explicitly, default to visible windows
on the current desktop/activity/output, and compute each output's geometry
separately if an all-screens mode is offered.

### 5. Low: first automatic snap can fail to remember original geometry

`src/contents/ui/main.qml:253` stores the original geometry only when
`client.zone == -1`. A newly added window can have an undefined zone when
application-based placement or automatic snapping first runs at
`src/contents/ui/main.qml:862`. Undefined does not satisfy that comparison.

The reproduction confirms that direct snapping of such a new window does not
save `oldGeometry`, defeating the expected restore-on-drag behavior.

Suggested fix: initialize window metadata before auto placement, or treat
undefined/missing zone state as unsnapped.

## Findings specific to the requested workflow

- KZones directly sets the selected window's `frameGeometry` at line 238.
  It does not implement shared-edge resizing or assign windows to KWin tiles.
  Existing native-tiled windows should be detached before switching workflows.
- Saved `~/.config/kglobalshortcutsrc` still assigned Meta+Left/Right to native
  "Quick Tile Window" actions during the audit. Those actions retain KWin's
  linked resizing. KZones declares those same defaults, so resolve the conflict
  by clearing native actions and binding KZones actions explicitly.
- Saved `~/.config/kwinrc` did not contain `kzonesEnabled=true` or KZones settings.
  Metadata defaults to disabled; installation alone does not enable it. This
  describes saved configuration rather than live compositor state.
- Native edge tiling had no explicit disabling override in the saved config.
- The installed KZones does not implement FancyZones' hold-Shift-to-show-zones
  interaction. Its overlay can appear when dragging starts, or be toggled by a
  shortcut while dragging. Native KWin Shift-drag is a separate tiling feature.

## Recommended configuration for a FancyZones-like workflow

1. Enable KZones under System Settings > Window Management > KWin Scripts.
2. Disable native Screen Edges > Tile > Windows dragged to left or right edge.
3. Clear native Quick Tile Left/Right shortcuts and assign KZones Move active
   window left/right instead. The first directional action on an unsnapped
   window first snaps to its closest zone, rather than jumping straight to the
   requested outer edge; explicit zone shortcuts give predictable placement.
4. In KZones settings, enable Zone Overlay and set Highlight zone when to
   "My cursor is anywhere in the zone". Show it when moving starts for the most
   direct drag/drop interaction, or choose the toggle shortcut option for
   explicit activation. Its default toggle shortcut is Ctrl+Alt+C.
5. Add a left/right 50/50 layout and any other preferred layouts. Keep automatic
   snapping of new windows and window fading off initially. Consider unbinding
   Snap all windows until its cross-screen behavior is fixed.

Example 50/50 layout entry for the layouts array:

```json
{
  "name": "Halves",
  "padding": 0,
  "zones": [
    {"x": 0, "y": 0, "width": 50, "height": 100},
    {"x": 50, "y": 0, "width": 50, "height": 100}
  ]
}
```

No installed script files or desktop settings were changed by this audit.

## Reproducing the audit checks

```sh
node audit/reproduce.cjs
```

The six checks assert the existence of the observed defects on the unmodified
0.9.2 baseline. They are audit reproductions, not passing regression tests for
fixed behavior. Once a defect is fixed, invert its assertions and integrate it
into a regression suite.

To package without installing into the active desktop:

```sh
make build PKGFILE=/tmp/kzones-local-v0.9.2.kwinscript
```

The default `make` target installs the script, and `make test` also installs it
before starting a nested desktop. Use explicit targets when experimenting.
