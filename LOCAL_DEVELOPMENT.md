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

`native/inputplugin.cpp` registers `KFancyZones.Input`. It observes raw modifier
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

The build generates the native module in `build/native/qml`, copies it into
`src/contents/ui/moveinput`, copies the canonical `main.qml` to `CompactSettingsOverlay.qml`,
and includes the license in `kfancyzones.kwinscript`. Generated files and local
build directories are ignored by Git. Only the production helper is packaged.

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
make native
python3 tests/integration.py
```

The test creates a private D-Bus session, XDG directories, configuration and two
virtual outputs. It checks fresh-install defaults without overriding the selector,
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

## Compact settings

The generic KWin settings loader uses `config.ui` and `main.xml`; it does not
execute code behind the form. `bin/generate-settings.py` generates a form using
only standard Qt widgets and signal/slot connections. Its stacked grid previews
follow the column, row, and subdivision controls without a second native plugin.
The Advanced section is collapsed initially; the shortcut link opens KDE's
Shortcuts page. Custom JSON overrides the regular grid and its subdivision.

Regenerate the checked-in form after changing the generator:

```sh
python3 bin/generate-settings.py
python3 bin/generate-settings.py --check
```

With integration-test builds enabled, the actual Qt form can be tested offscreen
(`Qt6::UiTools` development files, supplied by `qt6-tools-dev`, are required):

```sh
QT_QPA_PLATFORM=offscreen build/native/kfancyzones-settings-test
```

Run the same KWin gestures against the subdivided layout:

```sh
KFANCYZONES_TEST_SUBDIVIDE=1 python3 tests/integration.py
```

`src/contents/code/layouts.js` generates the row-major grid, recognizes the legacy
saved default, and validates custom JSON. Retired behavior keys are ignored;
manual `enableDebugLogging` remains available for diagnostics.

## History

- `65e653e` / 0.9.2.2: minimal unpolished MVP with Shift/Ctrl multi-zone gestures.
- `3b9629c` / 0.9.2.5: unfilled gray zones and one filled green bounding preview.
- 0.9.2.6: keep the dragged window above the overlay.
- 0.9.2.7: suppress the competing native preview, resolving the observed flicker.
- 0.9.2.8: rename the fork to KFancyZones.
- 0.9.2.9: disable the old selector on fresh installs and prepare public docs.
- 0.9.2.10: compact grid settings, 2×2 subdivision, and Advanced custom layouts/filters.

The earlier local rename copied settings from `[Script-kzones-fancyzones]` into
`[Script-kfancyzones]` on the development desktop. That was a one-time migration;
`make install` does not perform it. Shortcut IDs retain `KZones:` for compatibility.

[LOCAL_AUDIT.md](LOCAL_AUDIT.md) records the original upstream v0.9.2 audit.
`node audit/reproduce.cjs` deliberately loads that original source from Git and
reproduces its defects; it does not audit the current fork.
