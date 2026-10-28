// SPDX-License-Identifier: GPL-3.0-or-later
// Shared layout configuration; coordinates are percentages of the output.

function grid(columns, rows, gap, subdivide) {
    const bounded = (value, fallback, maximum) => Number.isInteger(value)
        && value >= 1 && value <= maximum ? value : fallback;
    columns = bounded(columns, 4, 8);
    rows = bounded(rows, 2, 4);
    const factor = subdivide ? 2 : 1;
    columns *= factor;
    rows *= factor;
    const zones = [];
    for (let y = 0; y < rows; y++) {
        for (let x = 0; x < columns; x++) {
            zones.push({x: x * 100 / columns, y: y * 100 / rows,
                width: 100 / columns, height: 100 / rows});
        }
    }
    return {name: columns + " x " + rows + " Grid",
        padding: Number.isInteger(gap) && gap >= 0 && gap <= 100 ? gap : 0, zones: zones};
}

function legacyDefault(layouts) {
    // Previous versions saved the built-in grid as JSON. Let the new grid
    // controls take effect without treating that old default as a custom layout.
    if (!Array.isArray(layouts) || layouts.length !== 1) return false;
    const layout = layouts[0], expected = grid(4, 2, 0, false);
    return layout && layout.name === expected.name && (layout.padding || 0) === 0
        && Object.keys(layout).every(key => ["name", "padding", "zones"].includes(key))
        && Array.isArray(layout.zones) && layout.zones.length === 8
        && layout.zones.every((zone, i) => zone && Object.keys(zone).length === 4
            && ["x", "y", "width", "height"].every(key => zone[key] === expected.zones[i][key]));
}

function load(json, columns, rows, gap, subdivide) {
    const fallback = [grid(columns, rows, gap, subdivide)];
    if (!json || !String(json).trim()) return {layouts: fallback, error: ""};
    try {
        const layouts = JSON.parse(json);
        if (legacyDefault(layouts)) return {layouts: fallback, error: ""};
        if (!Array.isArray(layouts) || !layouts.length) throw new Error("Expected a nonempty layout array");
        for (const layout of layouts) {
            if (!layout || !Array.isArray(layout.zones) || !layout.zones.length) {
                throw new Error("Every layout needs at least one zone");
            }
            if (layout.padding != null && (!Number.isFinite(layout.padding) || layout.padding < 0)) {
                throw new Error("Padding must be a nonnegative number");
            }
            for (const zone of layout.zones) {
                if (!zone || !["x", "y", "width", "height"].every(key => Number.isFinite(zone[key]))
                    || zone.x < 0 || zone.y < 0 || zone.width <= 0 || zone.height <= 0
                    || zone.x + zone.width > 100.000001 || zone.y + zone.height > 100.000001) {
                    throw new Error("Zone coordinates must fit within 0–100 percent");
                }
            }
        }
        return {layouts: layouts, error: ""};
    } catch (error) {
        return {layouts: fallback, error: "Invalid custom layout; using the grid. " + error.message};
    }
}
