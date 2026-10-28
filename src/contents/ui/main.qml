import QtQuick
import org.kde.kwin
import org.kde.plasma.core as PlasmaCore

import "components" as Components
import "liveinput" as Native
import "../code/selection.js" as Selection
import "../code/configuration.js" as Layouts

PlasmaCore.Dialog {

    id: mainDialog

    // properties
    property bool shown: false
    property bool moving: false
    property bool moved: false
    property bool resizing: false
    property var clientArea: ({})
    property var displaySize: ({})
    property int currentLayout: 0
    property int highlightedZone: -1
    property var zoneSelection: ({context: "", zones: [], hovered: -1, extending: false})
    property var highlightedZones: Selection.selected(zoneSelection)
    property var selectionGeometry: Selection.combinedGeometry(config.layouts?.[currentLayout], clientArea, highlightedZones)
    property bool dragCancelled: false
    property var dragOriginalGeometry: null
    property var dragOriginalTile: null
    property var draggedWindow: null
    property var overlayWindow: null
    property var stackedOverlay: null
    property var stackedClient: null
    property bool shiftDrag: inputState.available && (inputState.modifiers & Qt.ShiftModifier) !== 0
    property var activeScreen: null
    property var config: ({})
    property bool configurationReady: false
    property bool configurationPending: false
    property string customLayoutError: ""

    title: "KFancyZones Overlay"
    location: PlasmaCore.Types.Desktop
    type: PlasmaCore.Dialog.OnScreenDisplay
    backgroundHints: PlasmaCore.Types.NoBackground
    flags: Qt.BypassWindowManagerHint | Qt.FramelessWindowHint | Qt.Popup
    hideOnWindowDeactivate: true
    visible: false
    outputOnly: true
    opacity: 1
    width: displaySize.width
    height: displaySize.height

    function loadConfig() {

        const settings = inputState.configuration || {};
        const read = (key, fallback) => settings[key] !== undefined ? settings[key] : fallback;
        const loaded = Layouts.load(read("layoutsJson", ""),
            read("gridColumns", 4), read("gridRows", 2),
            read("gridGap", 0), read("subdivideZones", false), read("useCustomLayout", false));
        customLayoutError = loaded.error;
        const layouts = loaded.layouts;

        config = {
            layouts: layouts,
            filterMode: read("filterMode", 1),
            filterList: read("filterList", ""),
            enableDebugLogging: read("enableDebugLogging", false)
        };

        log("Config loaded: " + JSON.stringify(config));
    }

    function applyConfiguration() {
        if (!configurationReady) return;
        if (moving) {
            configurationPending = true;
            return;
        }
        configurationPending = false;
        currentLayout = 0;
        resetZoneSelection();
        loadConfig();
        // Keep geometry unchanged; discard associations with the old layout.
        for (const client of Workspace.stackingOrder) {
            if (client.zone !== undefined) {
                client.zone = -1;
                client.layout = -1;
                client.zones = [];
            }
        }
        console.info("KFancyZones settings applied: zones=" + config.layouts[0].zones.length);
    }

    function log(message) {
        if (!config.enableDebugLogging) return;
        console.log("KFancyZones: " + message);
    }

    function show() {
        mainDialog.shown = true;
        mainDialog.visible = zoneDragEnabled();
        mainDialog.setWidth(Workspace.virtualScreenSize.width);
        mainDialog.setHeight(Workspace.virtualScreenSize.height);
        mainDialog.x = Workspace.virtualScreenGeometry.x;
        mainDialog.y = Workspace.virtualScreenGeometry.y;
        refreshClientArea();
    }

    function hide() {
        releaseOverlayStacking();
        mainDialog.shown = false;
        mainDialog.visible = false;
        highlightedZone = -1;
        resetZoneSelection();
    }

    function refreshClientArea() {
        activeScreen = moving ? Workspace.screenAt(Workspace.cursorPos) : Workspace.activeScreen;
        clientArea = Workspace.clientArea(KWin.FullScreenArea, activeScreen, Workspace.currentDesktop);
        displaySize = Workspace.virtualScreenSize;
    }

    function resetZoneSelection() {
        zoneSelection = {context: "", zones: [], hovered: -1, extending: false};
    }

    function releaseOverlayStacking() {
        if (stackedOverlay && stackedClient) Workspace.unconstrain(stackedOverlay, stackedClient);
        stackedOverlay = null;
        stackedClient = null;
    }

    function updateOverlayStacking() {
        if (!moving || !mainDialog.visible || !draggedWindow) {
            releaseOverlayStacking();
            return;
        }
        if (!overlayWindow) {
            overlayWindow = Workspace.stackingOrder.find(window => window.caption === mainDialog.title) || null;
        }
        if (!overlayWindow || (stackedOverlay === overlayWindow && stackedClient === draggedWindow)) return;
        releaseOverlayStacking();
        Workspace.constrain(overlayWindow, draggedWindow);
        Workspace.raiseWindow(draggedWindow);
        stackedOverlay = overlayWindow;
        stackedClient = draggedWindow;
    }

    function zoneDragEnabled() {
        // Modifier signals can run before the shiftDrag binding refreshes.
        // Read raw state so Shift-up cancels synchronously in that signal.
        return inputState.available && (inputState.modifiers & Qt.ShiftModifier) !== 0;
    }

    function updateZoneSelection(zone) {
        const context = activeScreen.name + ":" + currentLayout;
        const extend = shiftDrag
            && (inputState.modifiers & Qt.ControlModifier) !== 0;
        zoneSelection = Selection.update(zoneSelection, zone, context, extend);
        highlightedZone = zone;
    }

    function updateShiftSelection() {
        if (!moving || dragCancelled) return;
        if (!zoneDragEnabled()) {
            releaseOverlayStacking();
            // Key release wins over any queued polling or mouse-release handler.
            resetZoneSelection();
            highlightedZone = -1;
            mainDialog.visible = false;
            Workspace.hideOutline();
            return;
        }
        mainDialog.visible = shown;
        updateOverlayStacking();
        refreshClientArea();
        // Sample on modifier changes and drop, as well as pointer motion.
        updateZoneSelection(Selection.zoneAt(config.layouts[currentLayout], clientArea, Workspace.cursorPos));
        Workspace.hideOutline();
    }

    function checkFilter(client) {

        if (!client) return false;
        if (!client.normalWindow) return false;
        if (client.popupWindow) return false;
        if (client.skipTaskbar) return false;

        const filter = config.filterList.split(/\r?\n/);
        if (config.filterList.length > 0) {
            if (config.filterMode == 0) {
                // include
                return filter.includes(client.resourceClass.toString());
            }
            if (config.filterMode == 1) {
                // exclude
                return !filter.includes(client.resourceClass.toString());
            }
        }
        return true;
    }

    function matchZone(client) {

        refreshClientArea();

        const layout = config.layouts[currentLayout];
        if (client.layout === currentLayout && client.zones?.length > 1
            && Selection.matches(client.frameGeometry, Selection.combinedGeometry(layout, clientArea, client.zones))) return;
        client.zone = -1;
        client.zones = [];
        for (let i = 0; i < layout.zones.length; i++) {
            if (Selection.matches(client.frameGeometry, Selection.zoneGeometry(layout, clientArea, i))) {
                client.zone = i;
                client.zones = [i];
                client.layout = currentLayout;
                break;
            }
        }
    }

    function getWindowsInZone(zone, layout) {
        const windows = [];
        for (let i = 0; i < Workspace.stackingOrder.length; i++) {
            const client = Workspace.stackingOrder[i];
            if (client.zone === zone && client.layout === layout && client.desktop === Workspace.currentDesktop && client.activity === Workspace.currentActivity && client.screen === Workspace.activeWindow.screen && checkFilter(client)) {
                windows.push(client);
            }
        }
        return windows;
    }

    function switchWindowInZone(zone, layout, reverse) {
        const clientsInZone = getWindowsInZone(zone, layout);
        if (reverse) clientsInZone.reverse();

        // cycle through clients in zone
        if (clientsInZone.length > 0) {
            const index = clientsInZone.indexOf(Workspace.activeWindow);
            if (index === -1) {
                Workspace.activeWindow = clientsInZone[0];
            } else {
                Workspace.activeWindow = clientsInZone[(index + 1) % clientsInZone.length];
            }
        }
    }

    function moveClientToZone(client, zone) {
        if (zone === -1) {
            if (checkFilter(client)) saveClientProperties(client, -1);
            return;
        }
        moveClientToZones(client, [zone]);
    }

    function moveClientToZones(client, zones) {
        if (!checkFilter(client)) return;
        refreshClientArea();
        const geometry = Selection.combinedGeometry(config.layouts[currentLayout], clientArea, zones);
        if (!geometry) return;
        saveClientProperties(client, zones[0]);
        client.zones = zones.slice();
        // KWin applies its own Shift-drag tile before emitting move-finished.
        // Detach it before setting the independent combined window geometry.
        if (client.tile) client.tile.unmanage(client);
        client.setMaximize(false, false);
        client.frameGeometry = Qt.rect(geometry.x, geometry.y, geometry.width, geometry.height);
    }

    function finishZoneDrop(client) {
        if (dragCancelled) return;
        updateShiftSelection();
        const zones = highlightedZones.slice();
        if (zones.length) {
            // Native tiling may already have resized the window at this point.
            // Save the geometry captured before the drag instead of that tile.
            const wasUnsnapped = client.zone == null || client.zone === -1;
            moveClientToZones(client, zones);
            if (dragOriginalGeometry && wasUnsnapped) {
                client.oldGeometry = dragOriginalGeometry;
            }
        } else {
            saveClientProperties(client, -1);
        }
    }

    function cancelZoneDrop(client) {
        // KWin 6.6 applies Shift tiling even after Escape restores the drag.
        // Put back the tiling state and geometry from before this move.
        if (dragOriginalTile) {
            dragOriginalTile.manage(client);
        } else {
            if (client.tile) client.tile.unmanage(client);
            if (dragOriginalGeometry) {
                const geometry = dragOriginalGeometry;
                client.frameGeometry = Qt.rect(geometry.x, geometry.y, geometry.width, geometry.height);
            }
        }
    }

    function saveClientProperties(client, zone) {
        if (zone !== -1 && (client.zone == null || client.zone === -1)) {
            client.oldGeometry = {
                x: client.frameGeometry.x, y: client.frameGeometry.y,
                width: client.frameGeometry.width, height: client.frameGeometry.height
            };
        }
        client.zone = zone;
        client.zones = zone === -1 ? [] : [zone];
        client.layout = currentLayout;
        client.desktop = Workspace.currentDesktop;
        client.activity = Workspace.currentActivity;
    }

    function moveClientToClosestZone(client) {
        if (!checkFilter(client)) return null;

        log("Moving client " + client.resourceClass.toString() + " to closest zone");

        refreshClientArea();

        const centerPointOfClient = {
            x: client.frameGeometry.x + (client.frameGeometry.width / 2),
            y: client.frameGeometry.y + (client.frameGeometry.height / 2)
        };

        const zones = config.layouts[currentLayout].zones;
        let closestZone = null;
        let closestDistance = Infinity;

        for (let i = 0; i < zones.length; i++) {
            const zone = zones[i];
            const zoneCenter = {
                x: (zone.x + zone.width / 2) / 100 * clientArea.width + clientArea.x,
                y: (zone.y + zone.height / 2) / 100 * clientArea.height + clientArea.y
            };
            const distance = Math.sqrt(Math.pow(centerPointOfClient.x - zoneCenter.x, 2) + Math.pow(centerPointOfClient.y - zoneCenter.y, 2));
            if (distance < closestDistance) {
                closestZone = i;
                closestDistance = distance;
            }
        }

        if (client.zone !== closestZone || client.layout !== currentLayout) moveClientToZone(client, closestZone);
        return closestZone;
    }

    function findClientSpecularZone(client, isVerticalAxis=false) {
        if (!checkFilter(client)) return null;
        refreshClientArea();
        const centerPointOfClient = {
            x: client.frameGeometry.x + (client.frameGeometry.width / 2),
            y: client.frameGeometry.y + (client.frameGeometry.height / 2)
        };
        const zones = config.layouts[currentLayout].zones;
        let currentZoneIndex = null;
        let closestDistance = Infinity;

        for (let i = 0; i < zones.length; i++) {
            const zone = zones[i];
            let zoneCenter = {
                x: (zone.x + zone.width / 2) / 100 * clientArea.width + clientArea.x,
                y: (zone.y + zone.height / 2) / 100 * clientArea.height + clientArea.y
            };
            const distance = Math.sqrt(
                Math.pow(centerPointOfClient.x - zoneCenter.x, 2) +
                Math.pow(centerPointOfClient.y - zoneCenter.y, 2)
            );
            if (distance < closestDistance) {
                currentZoneIndex = i;
                closestDistance = distance;
            }
        }
        if (currentZoneIndex === null) return null;
        const currentZone = zones[currentZoneIndex];
        const currentZoneCenter = {
            x: currentZone.x + currentZone.width / 2,
            y: currentZone.y + currentZone.height / 2
        };
        let specularZoneIndex = null;
        let minDistance = Infinity;

        for (let i = 0; i < zones.length; i++) {
            if (i === currentZoneIndex) continue;

            const zone = zones[i];
            const zoneCenter = {
                x: zone.x + zone.width / 2,
                y: zone.y + zone.height / 2
            };
            let isSpecular = false;
            if (isVerticalAxis) {
                isSpecular = Math.abs(zoneCenter.x - currentZoneCenter.x) < 5 &&
                    Math.abs((zoneCenter.y - 50) - (50 - currentZoneCenter.y)) < 5;
            } else {
                isSpecular = Math.abs(zoneCenter.y - currentZoneCenter.y) < 5 &&
                    Math.abs((zoneCenter.x - 50) - (50 - currentZoneCenter.x)) < 5;
            }
            if (isSpecular) {
                const specularPoint = {
                    x: !isVerticalAxis ? (100 - currentZoneCenter.x) : currentZoneCenter.x,
                    y: isVerticalAxis ? (100 - currentZoneCenter.y) : currentZoneCenter.y
                };
                const distance = Math.sqrt(
                    Math.pow(zoneCenter.x - specularPoint.x, 2) +
                    Math.pow(zoneCenter.y - specularPoint.y, 2)
                );
                if (distance < minDistance) {
                    specularZoneIndex = i;
                    minDistance = distance;
                }
            }
        }
        return specularZoneIndex !== null ? specularZoneIndex : currentZoneIndex;
    }

    function moveAllClientsToClosestZone() {
        log("Moving all clients to closest zone");
        let count = 0;
        for (let i = 0; i < Workspace.stackingOrder.length; i++) {
            const client = Workspace.stackingOrder[i];
            if (client.move) continue;
            moveClientToClosestZone(client) && count++;
        }
        log("Moved " + count + " clients to closest zone");
        return count;
    }

    function moveClientToNeighbour(client, direction) {
        if (!checkFilter(client)) return null;

        log("Moving client " + client.resourceClass.toString() + " to neighbour " + direction);

        refreshClientArea();

        const zones = config.layouts[currentLayout].zones;

        if (client.zone === -1 || client.layout !== currentLayout) {
            moveClientToClosestZone(client);
            return client.zone;
        };

        const currentZone = zones[client.zone];
        let targetZoneIndex = -1;

        let minDistance = Infinity;

        for (let i = 0; i < zones.length; i++) {
            if (i === client.zone) continue;

            const zone = zones[i];
            let isNeighbour = false;
            let distance = Infinity;

            switch (direction) {
                case "left":
                    if (zone.x + zone.width <= currentZone.x &&
                        zone.y < currentZone.y + currentZone.height &&
                        zone.y + zone.height > currentZone.y) {
                        isNeighbour = true;
                        distance = currentZone.x - (zone.x + zone.width);
                    }
                    break;
                case "right":
                    if (zone.x >= currentZone.x + currentZone.width &&
                        zone.y < currentZone.y + currentZone.height &&
                        zone.y + zone.height > currentZone.y) {
                        isNeighbour = true;
                        distance = zone.x - (currentZone.x + currentZone.width);
                    }
                    break;
                case "up":
                    if (zone.y + zone.height <= currentZone.y &&
                        zone.x < currentZone.x + currentZone.width &&
                        zone.x + zone.width > currentZone.x) {
                        isNeighbour = true;
                        distance = currentZone.y - (zone.y + zone.height);
                    }
                    break;
                case "down":
                    if (zone.y >= currentZone.y + currentZone.height &&
                        zone.x < currentZone.x + currentZone.width &&
                        zone.x + zone.width > currentZone.x) {
                        isNeighbour = true;
                        distance = zone.y - (currentZone.y + currentZone.height);
                    }
                    break;
            }

            if (isNeighbour && distance < minDistance) {
                minDistance = distance;
                targetZoneIndex = i;
            }
        }
        if (targetZoneIndex !== -1) {
            moveClientToZone(client, targetZoneIndex);
        } else {
            const toScreenMap = {
                left: "slotWindowToPrevScreen",
                right: "slotWindowToNextScreen",
                up: "slotWindowToAboveScreen",
                down: "slotWindowToBelowScreen"
            };
            if (Workspace[toScreenMap[direction]]) {
                const isVerticalAxis = direction === "up" || direction === "down";
                const specularZone = findClientSpecularZone(client, isVerticalAxis);
                Workspace[toScreenMap[direction]]();
                moveClientToZone(client, specularZone);
            }
        }
        return targetZoneIndex;
    }

    function setCurrentLayout(layout) {
        currentLayout = layout;
    }

    function connectSignals(client) {
        if (!checkFilter(client)) return;
        windowConnections.createObject(mainItem, {target: client});
    }

    property Native.InputState modifierState: Native.InputState {
        id: inputState
        window: mainDialog.draggedWindow
        suppressNativeTiling: mainDialog.moving
        onConfigurationChanged: mainDialog.applyConfiguration()
        onModifiersChanged: {
            // Clear selection synchronously on Shift-up; refresh once more after
            // the input event has finished updating the window position.
            mainDialog.updateShiftSelection();
            Qt.callLater(mainDialog.updateShiftSelection);
        }
        onCancelRequested: {
            if (moving) {
                dragCancelled = true;
                resetZoneSelection();
                highlightedZone = -1;
            }
        }
    }

    Components.Shortcuts {
        onCycleLayouts: {
            resetZoneSelection();
            setCurrentLayout((currentLayout + 1) % config.layouts.length);
            highlightedZone = -1;
            osdDbus.exec(config.layouts[currentLayout].name);
        }

        onCycleLayoutsReversed: {
            resetZoneSelection();
            setCurrentLayout((currentLayout - 1 + config.layouts.length) % config.layouts.length);
            highlightedZone = -1;
            osdDbus.exec(config.layouts[currentLayout].name);
        }

        onMoveActiveWindowToNextZone: {
            const client = Workspace.activeWindow;
            if (client.zone == -1) moveClientToClosestZone(client);
            const zonesLength = config.layouts[currentLayout].zones.length;
            moveClientToZone(client, (client.zone + 1) % zonesLength);
        }

        onMoveActiveWindowToPreviousZone: {
            const client = Workspace.activeWindow;
            if (client.zone == -1) moveClientToClosestZone(client);
            const zonesLength = config.layouts[currentLayout].zones.length;
            moveClientToZone(client, (client.zone - 1 + zonesLength) % zonesLength);
        }

        onSwitchToNextWindowInCurrentZone: {
            switchWindowInZone(Workspace.activeWindow.zone, Workspace.activeWindow.layout);
        }

        onSwitchToPreviousWindowInCurrentZone: {
            switchWindowInZone(Workspace.activeWindow.zone, Workspace.activeWindow.layout, true);
        }

        onMoveActiveWindowToZone: {
            moveClientToZone(Workspace.activeWindow, zone);
        }

        onActivateLayout: {
            resetZoneSelection();
            if (layout <= config.layouts.length - 1) {
                setCurrentLayout(layout);
                highlightedZone = -1;
                osdDbus.exec(config.layouts[currentLayout].name);
            } else {
                osdDbus.exec(`Layout ${layout + 1} does not exist`);
            }
        }

        onMoveActiveWindowUp: {
            moveClientToNeighbour(Workspace.activeWindow, "up");
        }

        onMoveActiveWindowDown: {
            moveClientToNeighbour(Workspace.activeWindow, "down");
        }

        onMoveActiveWindowLeft: {
            moveClientToNeighbour(Workspace.activeWindow, "left");
        }

        onMoveActiveWindowRight: {
            moveClientToNeighbour(Workspace.activeWindow, "right");
        }

        onSnapActiveWindow: {
            moveClientToClosestZone(Workspace.activeWindow);
        }

        onSnapAllWindows: {
            moveAllClientsToClosestZone();
        }
    }

    Component.onCompleted: {
        // refresh client area
        refreshClientArea();
        mainDialog.loadConfig();
        configurationReady = true;
        console.info("KFancyZones 0.9.2.12 loaded: native=" + inputState.available
            + ", zones=" + config.layouts[0].zones.length);

        // match all clients to zones and connect signals
        for (let i = 0; i < Workspace.stackingOrder.length; i++) {
            matchZone(Workspace.stackingOrder[i]);
            connectSignals(Workspace.stackingOrder[i]);
        }
    }

    Component.onDestruction: releaseOverlayStacking()

    Item {
        id: mainItem
        property alias repeaterLayout: repeaterLayout
        width: mainDialog.width
        height: mainDialog.height

        Component {
            id: windowConnections
            Connections {
                // Owned by the overlay: destroying/reloading the script removes
                // every client callback instead of leaving closures pointing at it.
                target: null
                property var client: target
                function onInteractiveMoveResizeStarted() {
                    log("Interactive move/resize started for client " + client.resourceClass.toString());
                    if (client.resizeable && checkFilter(client)) {
                        if (client.move && checkFilter(client)) {
                            draggedWindow = client;
                            dragCancelled = false;
                            resetZoneSelection();
                            dragOriginalGeometry = {
                                x: client.frameGeometry.x, y: client.frameGeometry.y,
                                width: client.frameGeometry.width, height: client.frameGeometry.height
                            };
                            dragOriginalTile = client.tile;

                            if (client.zone !== -1 && client.oldGeometry) {
                                const geometry = client.oldGeometry;
                                client.frameGeometry = Qt.rect(
                                    Math.round(Workspace.cursorPos.x - geometry.width / 2),
                                    Math.round(client.frameGeometry.y),
                                    Math.round(geometry.width), Math.round(geometry.height));
                            }

                            moving = true;
                            moved = false;
                            resizing = false;
                            log("Move start " + client.resourceClass.toString());
                            mainDialog.show();
                            Qt.callLater(updateShiftSelection);
                        }
                        if (client.resize) {
                            moving = false;
                            moved = false;
                            resizing = true;
                        }
                    }
                }

                function onInteractiveMoveResizeStepped() {
                    if (client.resizeable) {
                        if (moving && checkFilter(client)) {
                            moved = true;
                            Qt.callLater(updateShiftSelection);
                        }
                    }
                }

                function onInteractiveMoveResizeFinished() {
                    log("Interactive move/resize finished for client " + client.resourceClass.toString());

                    if (moving) {
                        log("Move end " + client.resourceClass.toString());
                        if (dragCancelled) cancelZoneDrop(client);
                        if (moved) {
                            if (shown && !dragCancelled) {
                                finishZoneDrop(client);
                            } else {
                                if (!dragCancelled) saveClientProperties(client, -1);
                            }
                        }
                        hide();
                    }  else if (resizing) {
                        matchZone(client);
                        log("Resizing end: Matched client " + client.resourceClass.toString() + " to layout.zone " + client.layout + " " + client.zone );
                        saveClientProperties(client, client.zone);
                    }
                    moving = false;
                    moved = false;
                    resizing = false;
                    dragOriginalGeometry = null;
                    dragOriginalTile = null;
                    draggedWindow = null;
                    if (configurationPending) Qt.callLater(applyConfiguration);
                }

                // Preserve optional custom-layout fullscreen placement.
                function onFullScreenChanged() {
                    const layout = config.layouts[client.layout];
                    const zone = layout?.zones[client.zone];
                    if (client.fullScreen && (layout?.fullscreen === true || zone?.fullscreen === true)) {
                        refreshClientArea();
                        const geometry = Selection.zoneGeometry(layout, clientArea, client.zone);
                        if (geometry) {
                            client.setMaximize(false, false);
                            client.frameGeometry = Qt.rect(geometry.x, geometry.y, geometry.width, geometry.height);
                        }
                    }
                    mainDialog.hide();
                }

            }
        }

        Timer {
            interval: 100
            running: shown && moving
            repeat: true
            triggeredOnStart: true
            onTriggered: mainDialog.updateShiftSelection()
        }

        DBusCall {
            id: osdDbus

            service: "org.kde.plasmashell"
            path: "/org/kde/osdService"
            method: "showText"

            function exec(text, icon = "preferences-desktop-virtual") {
                this.arguments = [icon, text];
                this.call();
            }
        }

        Item {
            x: (clientArea.x || 0) - Workspace.virtualScreenGeometry.x
            y: (clientArea.y || 0) - Workspace.virtualScreenGeometry.y
            width: clientArea.width || 0
            height: clientArea.height || 0
            clip: true

            Rectangle {
                visible: customLayoutError.length > 0
                x: 20
                y: 20
                z: 100
                width: Math.min(700, parent.width - 40)
                height: warningText.implicitHeight + 24
                color: "#dd202020"
                radius: 5
                Text {
                    id: warningText
                    x: 12
                    y: 12
                    width: parent.width - 24
                    text: customLayoutError
                    textFormat: Text.PlainText
                    color: "#ff6b6b"
                    wrapMode: Text.Wrap
                }
            }

            Repeater {
                id: repeaterLayout
                model: config.layouts

                Components.UnselectedZones {
                    id: zones
                    config: mainDialog.config
                    area: mainDialog.clientArea
                    preview: mainDialog.selectionGeometry
                    layoutIndex: index
                    visible: index == mainDialog.currentLayout
                }
            }

            Rectangle {
                id: snapPreview
                // Preview the actual bounding rectangle, including space between zones.
                visible: highlightedZones.length > 0 && selectionGeometry !== null
                x: selectionGeometry ? selectionGeometry.x - clientArea.x : 0
                y: selectionGeometry ? selectionGeometry.y - clientArea.y : 0
                width: selectionGeometry ? selectionGeometry.width : 0
                height: selectionGeometry ? selectionGeometry.height : 0
                color: "#2635c56b"
                border.color: "#69db8f"
                border.width: 3
                radius: 8
            }

        }

        // workspace connection
        Connections {
            target: Workspace

            function onWindowAdded(client) {

                if (client.caption === mainDialog.title) {
                    overlayWindow = client;
                    Qt.callLater(updateOverlayStacking);
                }

                connectSignals(client);

                // check if client is in a zone application list
                config.layouts[currentLayout].zones.forEach((zone, zoneIndex) => {
                    if (zone.applications && zone.applications.includes(client.resourceClass.toString())) {
                        moveClientToZone(client, zoneIndex);
                        return;
                    }
                });

                // check if new window spawns in a zone
                if (client.zone == undefined || client.zone == -1) matchZone(client);

            }

            function onWindowRemoved(client) {
                if (client === overlayWindow || client === draggedWindow) {
                    releaseOverlayStacking();
                    if (client === overlayWindow) overlayWindow = null;
                    if (client === draggedWindow) draggedWindow = null;
                }
            }
        }

    }
}
