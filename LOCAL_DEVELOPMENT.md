# Local development

## MVP baseline

Version **0.9.2.2** is the minimal, unpolished MVP confirmed working in the
active desktop session. Preserve its gesture behavior before further pruning.
It retains upstream features and rough edges. KWin can cache changed QML across
script reloads; source updates may require logging out and back in. Automatic
cache cleanup is not part of this MVP.

Branch `local/fancyzones` started from upstream v0.9.2. The local feature adds
multi-zone selection and an eight-zone (4 columns x 2 rows) default layout.
By default, zones are shown only while Shift is held during a window drag.
Ctrl adds visited zones to the selection. Releasing the mouse first accepts
the bounding rectangle; releasing Shift first immediately clears the selection
and continues the normal window drag without snapping.

## Build requirements

On Kubuntu 26.04, install these yourself with administrator privileges:

```sh
sudo apt-get install --no-install-recommends kwin-dev extra-cmake-modules qt6-base-dev qt6-declarative-dev cmake g++ make zip
```

The native QML module uses KWin's modifier state and observes Escape before
KWin processes drag cancellation. It does not intercept input, inject events,
run a background service, access the network, or modify KWin's system files.
It builds against KWin 6.6's development API. Rebuild it when upgrading KWin;
compatibility with other KWin versions and X11 has not been established.

## Build and install

Package the local fork without installing it:

```sh
make build
```

This compiles `native/inputplugin.cpp`, copies the resulting QML module into
`src/contents/ui/native`, and builds `kzones-fancyzones.kwinscript`. Generated files are
ignored by Git. The package includes only the production native module.

To install the fork as a separate script:

```sh
make install
```

In System Settings > Window Management > KWin Scripts, disable the original
**KZones** and enable **KZones FancyZones**. The fork uses plugin ID
`kzones-fancyzones`, its own install directory, and `[Script-kzones-fancyzones]`
settings. This avoids KWin reusing the original script's cached QML or settings.
Its defaults are the 4x2 layout and **I hold Shift while dragging** activation.
Window callbacks are owned QML Connections, so they are removed on unload.
KWin's built-in Shift-drag handler still runs, but the fork hides its outline
while selecting KZones areas and detaches its native tile before applying the
final geometry. Native Meta+Arrow shortcuts are separate: remove their Quick
Tile bindings and bind KZones actions if independent resizing is desired.

## Checks

```sh
make check
```

Runs geometry, selection-state, and drop tests against the actual JavaScript
library and JavaScript methods extracted from the QML overlay. No desktop is
needed. Node.js 24 or newer is used (`--test-isolation=none`).

For an integration test inside a separate headless KWin compositor:

```sh
cmake -S native -B build/native -DKZONES_BUILD_INTEGRATION_TESTS=ON
make native
python3 tests/integration.py
```

The test creates its own D-Bus session, virtual output, configuration, data, and
runtime directories. It maps a test window and checks actual Ctrl/Shift input,
multi-zone preview, final window size, native tile detachment, Ctrl release,
Shift release cancellation, mouse-first acceptance, and the normal Escape
window-move cancellation. It also unloads and reloads the script in the same
compositor and verifies that callbacks are cleaned up. This does not verify
loading changed source in an existing QML engine. Logs are in `build/integration.log` and
`build/integration-bus.log`. Temporary processes are confined to their own
process group and stopped afterward.

The input injection driver in `tests/native` is compiled only when the test
option is enabled, is never packaged, and refuses input injection without
`KZONES_INTEGRATION_TEST=1`.

Original audit reproductions remain available with `node audit/reproduce.cjs`.
They explicitly load the original `v0.9.2` source from Git, so they continue to
document the original defects after local fixes.
