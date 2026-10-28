# Development

## Supported baseline

The tested environment is Kubuntu 26.04, KWin/Plasma 6.6.6, Qt 6.10.2, Wayland.
Other versions and X11 are unverified. The helper is loaded into KWin and links
against its development API. Build on the target system and rebuild after KWin
upgrades; the package is not a portable binary for arbitrary Plasma releases.

Build dependencies and user installation are documented in [README.md](README.md).
Node.js 24 or newer is required for unit tests. Python 3, Qt Quick Test tools,
`dbus-run-session`, `qdbus6`, and `kwin_wayland` are needed for integration tests.

## Native helper and overlay

`native/inputplugin.cpp` registers `KFancyZones.LiveInput`. It observes raw modifier
state and Escape, and installs an input filter immediately before KWin's move
filter. The filter handles only the window owned by this script while Shift is
held during an interactive move. It forwards KWin's normal move update with
Shift removed from that call, while QML continues to see the real modifiers.
Other window moves, resizing, and mouse-button handling remain with KWin.
The production helper does not inject input or modify KWin's system files.

This avoids a native-preview loop: KWin showed its custom-tile outline on each
pointer move, then the script hid it. The outline animates from the moving
window's geometry. An isolated short drag produced three native activations
before suppression and zero afterward; desktop testing confirmed the flicker
was no longer visible. With the overlay disabled, native Shift tiling still works.

The overlay temporarily constrains itself below the dragged window and above
other normal windows, without changing the window's always-on-top flag. The
constraint is removed on Shift release, drop, cancellation, window closure, or
script unload. Client callbacks use owned QML Connections to avoid stale
callbacks after script reloads.

## Build products and live reloads

```sh
make build
```

Native binaries stay under `build/native`. `bin/package.py` writes the package
from source files and those build products, with no generated files in `src/`.
It includes the license and a separate root QML entry URL for live development.
The settings library filename includes the package version so a new controller
can load without overwriting a library mapped by System Settings.

The packager resolves the controller's absolute per-user install path in package
metadata; source metadata stays free of machine-specific paths. Rebuild when
installing under another account or location. Test helpers are never packaged.

KWin can cache QML across reloads and retain native libraries in memory. A changed
entry-point URL can bypass a cached root QML file during development. Native
changes may require a new module/library name or a fresh desktop session. Never
overwrite a loaded shared library in place. Use the isolated test compositor
for native development; no KWin restart is required for the test suite.

`make` builds the package. `make test` runs unit tests. Installation and enabling
are explicit targets: `make install`, then enable in KWin Scripts. `make enable`
and `make disable` write the plugin flag and ask KWin to reconfigure.

## Checks

Unit tests execute the actual selection library and methods extracted from QML:

```sh
make check
```

For the visual component tests:

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software /usr/lib/qt6/bin/qmltestrunner -input tests/qml
```

For the integration test in a separate headless KWin compositor:

```sh
cmake -S native -B build/native -DKFANCYZONES_BUILD_INTEGRATION_TESTS=ON
make build
python3 tests/integration.py
```

The test creates a private D-Bus session, XDG directories, configuration and two
virtual outputs. It loads the built package and checks fresh-install defaults,
real Shift/Ctrl input, multi-zone bounds, green preview and gray outlines,
mouse-first acceptance, Shift-first cancellation, tile detachment, Ctrl release,
Escape, cross-output placement, overlay stacking and cleanup. It checks zero
native preview activations during zone selection and native preview availability
when the overlay is disabled. It unloads/reloads the script and repeats in the
same compositor, but does not verify cache invalidation for changed source.
Logs are `build/integration.log` and `build/integration-bus.log`. All test
processes are confined to their own process group and stopped afterward.

The injection driver under `tests/native` is built only when the integration
option is enabled, is never packaged, and refuses injection without
`KFANCYZONES_INTEGRATION_TEST=1`.

## Settings controller and live updates

`native/settingsplugin.cpp` is a user-local KCModule loaded by System Settings.
It constructs the compact two-tab form directly and binds `main.xml` through KConfigLoader and
KConfigDialogManager. A single preview widget follows the grid controls. The
Advanced tab has an explicit custom-layout toggle and live red validation errors.
The controller evaluates the same `configuration.js` functions used by the overlay,
passing editor contents as data to JSON.parse, not evaluating user-supplied code.

The controller requires `libkf6kcmutils-dev` to build. Runtime KDE libraries are
normally present on Plasma; Qt UiTools is not needed.
The build package targets the current user's standard XDG install path; rebuild
when installing under another account or location.

The native input helper also reads the script's configuration through its own
KConfig object and watches the config file and its parent directory. A short
single-shot timer coalesces writes and restores the watch after atomic file
replacement. Only changes to relevant settings emit `configurationChanged`.
QML applies them immediately between drags and defers them during a drag.
This avoids KWin's cached script configuration and the generic settings loader's
missing script-reload notification. Existing window geometry is left unchanged;
old layout associations are discarded when settings change.

The regular grid defaults to 4×2 and supports optional 2×2 subdivision. Custom
mode defaults off and ignores stored JSON. When enabled, valid custom layouts
replace the grid; invalid or empty custom JSON falls back to plain 4×2 with no
padding and a diagnostic. Turning custom mode off restores the configured grid.

With integration-test builds enabled, test the actual controller offscreen. It
stages its own package and config directory, checks the custom toggle and red
warnings, and verifies Apply/defaults/reload without touching desktop settings:

```sh
QT_QPA_PLATFORM=offscreen build/native/kfancyzones-settings-test
```

Run the KWin gestures against the subdivided layout:

```sh
KFANCYZONES_TEST_SUBDIVIDE=1 python3 tests/integration.py
```

The integration test also edits its private configuration while KWin is running,
checking grid changes, custom mode, invalid fallback and deferred updates during
a drag. Retired behavior keys are ignored; manual `enableDebugLogging` remains
available for diagnostics.

## History

- `65e653e` / 0.9.2.2: minimal unpolished MVP with Shift/Ctrl multi-zone gestures.
- `3b9629c` / 0.9.2.5: unfilled gray zones and one filled green bounding preview.
- 0.9.2.6: keep the dragged window above the overlay.
- 0.9.2.7: suppress the competing native preview, resolving the observed flicker.
- 0.9.2.8: rename the fork to KFancyZones.
- 0.9.2.9: disable the old selector on fresh installs and prepare public docs.
- 0.9.2.10: compact grid settings, 2×2 subdivision, and Advanced custom layouts/filters.
- 0.9.2.11: explicit custom mode, live red validation, plain 4×2 fallback, and automatic settings application.
- 0.9.2.12: prune retired overlay features and tooling, construct the settings form
  directly, and keep generated build products out of the source tree.

The earlier local rename copied settings from `[Script-kzones-fancyzones]` into
`[Script-kfancyzones]` on the development desktop. That was a one-time migration;
`make install` does not perform it. Shortcut IDs retain `KZones:` for compatibility.
