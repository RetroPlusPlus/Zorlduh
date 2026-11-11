// What is solid in an area: its collision map's solid cells, and everything off the map except the way
// out through an exit.
//
//     walker.walk(config, move, camera.solids(layout));   // a Camera hands out the current area's
//
// Anything that moves asks it about rectangles, in the area's own pixels, and decides for itself what to
// do with the answer; WorldKit moves nothing.
#pragma once

#include <cstddef>

#include "retropp/geometry.h"
#include "retropp/image.h"

#include "WorldKit/area.h"

namespace WorldKit {

// The solid query for one area of a layout. A cell is solid unless its collision value is the layout's
// `open`. Every cell off the map is solid too, except the cells straight out through an edge opening
// that leads somewhere: those are open as far out as anything cares to go, so a thing walking out
// through an exit can cross the edge. The layout must outlive the query.
struct AreaSolids {
    const Layout* layout = nullptr;
    int area = 0;

    // Whether `pixel` is in a solid cell.
    [[nodiscard]] bool solidAt(retropp::Vec2i pixel) const noexcept {
        return solidCell(detail::cellOf(pixel.x, layout->cellSize), detail::cellOf(pixel.y, layout->cellSize));
    }

    // Whether any pixel of `box` is in a solid cell. An empty box covers no cell and is never solid.
    [[nodiscard]] bool solidBox(retropp::IntRect box) const noexcept {
        if (box.width <= 0 || box.height <= 0) {
            return false;
        }
        const int cellSize = layout->cellSize;
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
    [[nodiscard]] bool solidCell(int cx, int cy) const noexcept {
        const Area& here = layout->areas[static_cast<std::size_t>(area)];
        const retropp::IndexGrid& map = *here.collision;
        if (cx >= 0 && cy >= 0 && cx < map.width && cy < map.height) {
            return map.at(cx, cy) != layout->open.value;
        }
        for (const Opening& opening : here.openings) {
            if (!detail::isEdge(opening) || opening.leadsTo.area == kNoArea) {
                continue;
            }
            const bool out = (opening.side == Side::East && cx >= map.width) ||
                             (opening.side == Side::West && cx < 0) ||
                             (opening.side == Side::South && cy >= map.height) ||
                             (opening.side == Side::North && cy < 0);
            const int along = detail::runsAcross(opening.side) ? cx : cy;
            if (out && detail::holds(opening.span, along)) {
                return false;
            }
        }
        return true;
    }
};

}  // namespace WorldKit
