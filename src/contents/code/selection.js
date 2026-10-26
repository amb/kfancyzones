// SPDX-License-Identifier: GPL-3.0-or-later
// Pure geometry and selection logic shared by the overlay and its tests.

function validZone(layout, index) {
    return layout && Array.isArray(layout.zones) && Number.isInteger(index)
        && index >= 0 && index < layout.zones.length;
}

function zoneGeometry(layout, area, index) {
    if (!validZone(layout, index)) return null;
    const zone = layout.zones[index];
    const padding = layout.padding || 0;
    const geometry = {
        x: Math.round(area.x + zone.x / 100 * (area.width - padding) + padding),
        y: Math.round(area.y + zone.y / 100 * (area.height - padding) + padding),
        width: Math.round(zone.width / 100 * (area.width - padding) - padding),
        height: Math.round(zone.height / 100 * (area.height - padding) - padding)
    };
    if (!Object.values(geometry).every(Number.isFinite)
        || geometry.width <= 0 || geometry.height <= 0) return null;
    return geometry;
}

function combinedGeometry(layout, area, indices) {
    if (!indices || !indices.length) return null;
    const geometries = indices.map(index => zoneGeometry(layout, area, index));
    if (geometries.some(geometry => !geometry)) return null;
    const x = Math.min(...geometries.map(geometry => geometry.x));
    const y = Math.min(...geometries.map(geometry => geometry.y));
    const right = Math.max(...geometries.map(geometry => geometry.x + geometry.width));
    const bottom = Math.max(...geometries.map(geometry => geometry.y + geometry.height));
    return {x: x, y: y, width: right - x, height: bottom - y};
}

function zoneAt(layout, area, point) {
    if (!layout || !Array.isArray(layout.zones) || area.width <= 0 || area.height <= 0) return -1;
    const x = (point.x - area.x) / area.width * 100;
    const y = (point.y - area.y) / area.height * 100;
    if (x < 0 || y < 0 || x > 100 || y > 100) return -1;
    // Use the unpadded zone for hit testing, so padding does not create dead gaps.
    return layout.zones.findIndex(zone => x >= zone.x && y >= zone.y
        && (x < zone.x + zone.width || (x === 100 && zone.x + zone.width === 100))
        && (y < zone.y + zone.height || (y === 100 && zone.y + zone.height === 100)));
}

function update(previous, hovered, context, extend) {
    const sameContext = previous && previous.context === context;
    const zones = extend && sameContext ? previous.zones.slice() : [];
    if (hovered >= 0 && !zones.includes(hovered)) zones.push(hovered);
    return {context: context, zones: zones, extending: extend, hovered: hovered};
}

function selected(state) {
    // Leaving all zones must not snap to a stale selection on mouse release.
    return state && state.hovered >= 0 ? state.zones : [];
}

function matches(geometry, target) {
    return target && ["x", "y", "width", "height"]
        .every(key => Math.abs(geometry[key] - target[key]) <= 1);
}
