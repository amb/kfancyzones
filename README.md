# KFancyZones

FancyZones-style window snapping for KDE Plasma, forked from
[KZones](https://github.com/gerritdevriese/kzones).

**Experimental MVP · version 0.9.2.11 · tested on KWin 6.6.6, Wayland,
Kubuntu 26.04.** Other KWin versions and X11 have not been verified.
The native helper builds against KWin's development API: build it on the target
system and rebuild after KWin upgrades. This is a source-build project.

## How it works

The default layout is **four columns by two rows**, giving eight equal zones.

1. Start dragging a window and hold **Shift** to show the zones.
2. Hold **Ctrl** as well and move across more zones to combine them. The preview
   is the bounding rectangle of every zone you visit, including space between them.
3. Release the **mouse button before Shift** to snap the window to that rectangle.
4. Release **Shift before the mouse button** to cancel snapping and continue
   dragging freely. Releasing Ctrl returns to the current single zone.

Available zones have unfilled gray outlines. One translucent green rectangle
shows the final window bounds; gray outlines covered by it are hidden. The
window stays above the overlay. The old mini-grid selector is disabled by default.
Snapped windows resize independently because they are detached from native KWin
custom tiles. KWin's competing Shift-drag preview is suppressed during selection.

## Build and install

Install the development dependencies yourself. On Kubuntu 26.04:

```sh
sudo apt-get install --no-install-recommends kwin-dev extra-cmake-modules qt6-base-dev qt6-declarative-dev cmake g++ make zip kpackagetool6 qdbus-qt6 python3 qt6-tools-dev libkf6kcmutils-dev
```

From this repository's root:

```sh
make build
make install
```

`make build` creates `kfancyzones.kwinscript`; `make install` installs or updates
it for the current user. Plain `make` also builds the package. Build and install
as the same user: the package records the settings controller’s per-user install
path under the standard XDG data directory.

Open **System Settings → Window Management → KWin Scripts**, disable the
original **KZones** and any older **KZones FancyZones** fork, and enable
**KFancyZones**. The plugin ID is `kfancyzones`. Restart System Settings if its
script list still shows the old name.

Settings take effect when you click **Apply**. Changes saved during a drag wait
until that drag ends. No script toggle is needed for ordinary settings changes.

After installing a new code version, KWin or System Settings may retain cached
QML or plugins. Restart System Settings to refresh the configuration dialog;
log out and back in if KWin still runs the older code. Never overwrite a loaded
native library in place during development.

To uninstall:

```sh
make uninstall
```

## Configuration

Use the settings button next to **KFancyZones** in KWin Scripts. The compact
**Layout** tab offers **Columns**, **Rows**, and **Gap**, with a grid preview. Enable
**Subdivide each zone into 2×2** to double the columns and rows: the default
4×2 becomes **8×4**. Subdivision is off by default. Shift/Ctrl selection and
release-order behavior stay the same at either grid size.

The separate **Advanced** tab contains **Use custom layout**, a JSON editor, and
application filters. Custom layout is **off by default**; saved JSON is ignored
while it is off, and the grid controls apply, including subdivision. Turning it
on uses the custom JSON instead. Custom layouts preserve their own padding.
[examples/simple.json](examples/simple.json) and
[examples/advanced.json](examples/advanced.json) show the format.

Invalid enabled custom JSON shows a **red warning while editing**. Applying it
uses the plain default **4×2 grid with zero gap**, regardless of the regular grid
settings. Empty JSON is also invalid when custom layout is enabled. Turning
custom layout off restores your configured grid, including an 8×4 subdivision.

Activation is always Shift-drag, Ctrl always combines zones, and restore-on-drag
and independent resizing are standard behavior. The old selector, edge snapping,
automatic snapping, fading, polling, and debug-overlay options are retired.
The selection resets when switching output or layout.

Keyboard actions appear under **KFancyZones** in System Settings → Shortcuts.
Internal action IDs retain their `KZones:` prefix to preserve existing key
bindings. Run only one fork at a time because those IDs are shared with upstream.
Native Meta+Arrow quick tiling remains a separate KWin feature; configure those
bindings separately if you want keyboard snapping through KFancyZones.

Settings use `[Script-kfancyzones]` in `~/.config/kwinrc`. Older local forks used
`[Script-kzones-fancyzones]`; installation does not automatically import that
group. Copy its settings to the new group with both scripts disabled if needed.

## Development and validation

See [LOCAL_DEVELOPMENT.md](LOCAL_DEVELOPMENT.md) for native-helper details and
the isolated KWin integration test.

```sh
make check
```

The unit tests require Python 3 and Node.js 24 or newer. The integration suite checks actual
Shift/Ctrl input, acceptance and cancellation order, multiple outputs, independent
window geometry, overlay stacking, suppression of the native preview, live settings
reload and custom-layout fallback.

## Credits and license

KFancyZones is maintained by [amb](https://github.com/amb) and is based
on KZones v0.9.2 by [Gerrit De Vriese](https://github.com/gerritdevriese) and its
contributors. The inherited Git history and original assets are retained.
The interaction is inspired by PowerToys FancyZones.

The project retains the GNU GPL v3 license; see [LICENSE.md](LICENSE.md).
