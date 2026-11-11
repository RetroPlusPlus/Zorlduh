// What is solid in an area: its collision map's solid cells, everything off the map except the way
// out through an exit, and past a seamless join, the next area's own cells.
//
//     walker.walk(config, move, camera.solids(world));   // a Camera hands out the current area's
//
// Anything that moves asks it about rectangles, in the area's own pixels, and decides for itself what to
// do with the answer; WorldKit moves nothing.
#pragma once

#include <cstddef>

#include "retropp/geometry.h"
#include "retropp/image.h"

#include "WorldKit/area.h"

namespace WorldKit {

// The solid query for one area of a world. A cell is solid unless its collision value is its layout's
// `open`. Every cell off the map is solid too, except the cells straight out through an edge opening
// that leads somewhere: past a scroll or a fade those are open as far out as anything cares to go, so
// a thing walking out through an exit can cross the edge; past a seamless join they are the next area's
// cells, read from its own collision map where it is attached, and solid beyond its edge. The world
// must outlive the query.
struct AreaSolids {
    const World* world = nullptr;
    int layout = 0;
    int area = 0;

    // Whether `pixel` is in a solid cell.
    [[nodiscard]] bool solidAt(retropp::Vec2i pixel) const noexcept {
        const int cellSize = here().cellSize;
        return solidCell(detail::cellOf(pixel.x, cellSize), detail::cellOf(pixel.y, cellSize));
    }

    // Whether any pixel of `box` is in a solid cell. An empty box covers no cell and is never solid.
    [[nodiscard]] bool solidBox(retropp::IntRect box) const noexcept {
        if (box.width <= 0 || box.height <= 0) {
            return false;
        }
        const int cellSize = here().cellSize;
        const int lastColumn = detail::cellOf(box.x + box.width - 1, cellSize);
        const int lastRow = detail::cellOf(box.y + box.height - 1, cellSize);
        for (int cy = detail::cellOf(box.y, cellSize); cy <= lastRow; ++cy) {
            for (int cx = detail::cellOf(box.x, cellSize); cx <= lastColumn; ++cx) {
                if (solidCell(cx, cy)) {
                    return true;
                }
            }
        }
        return false;
    }

private:
    [[nodiscard]] const Layout& here() const noexcept {
        return world->layouts[static_cast<std::size_t>(layout)];
    }

    [[nodiscard]] bool solidCell(int cx, int cy) const noexcept {
        const Layout& layout_ = here();
        const Area& area_ = layout_.areas[static_cast<std::size_t>(area)];
        const retropp::IndexGrid& map = *area_.collision;
        if (cx >= 0 && cy >= 0 && cx < map.width && cy < map.height) {
            return map.at(cx, cy) != layout_.open.value;
        }
        for (const Opening& opening : area_.openings) {
            if (!detail::isEdge(opening) || opening.leadsTo.area == kNoArea) {
                continue;
            }
            const bool out = (opening.side == Side::East && cx >= map.width) ||
                             (opening.side == Side::West && cx < 0) ||
                             (opening.side == Side::South && cy >= map.height) ||
                             (opening.side == Side::North && cy < 0);
            const int along = detail::runsAcross(opening.side) ? cx : cy;
            if (!out || !detail::holds(opening.span, along)) {
                continue;
            }
            if (opening.transition != Transition::Seamless) {
                return false;
            }
            // The cell is the next area's, where that area is attached.
            const detail::Link link = detail::link(*world, layout, area, opening);
            const Layout& there = world->layouts[static_cast<std::size_t>(link.to.layout)];
            const retropp::IndexGrid& next = *there.areas[static_cast<std::size_t>(link.to.area)].collision;
            const int nx = cx - link.origin.x / layout_.cellSize;
            const int ny = cy - link.origin.y / layout_.cellSize;
            if (nx < 0 || ny < 0 || nx >= next.width || ny >= next.height) {
                return true;
            }
            return next.at(nx, ny) != there.open.value;
        }
        return true;
    }
};

}  // namespace WorldKit
