#include "game/world.h"

#include <cstdint>

#include "assets/assets.h"

namespace zorlduh {

namespace {

// The cell holding pixel coordinate `pixel`, rounding toward negative infinity so a pixel left of or
// above the map lands in a cell off the map.
int cellOf(int pixel) {
    return pixel >= 0 ? pixel / kCellSize : -((-pixel + kCellSize - 1) / kCellSize);
}

bool solidCell(const retropp::IndexGrid& collision, int cx, int cy) {
    if (cx < 0 || cy < 0 || cx >= collision.width || cy >= collision.height) {
        return true;
    }
    return collision.at(cx, cy) != static_cast<std::uint16_t>(CollisionCell::Open);
}

}  // namespace

bool World::solidAt(int px, int py) const {
    return solidCell(collision, cellOf(px), cellOf(py));
}

bool World::solidBox(Box box) const {
    if (box.width <= 0 || box.height <= 0) {
        return false;
    }
    const int firstColumn = cellOf(box.x);
    const int lastColumn = cellOf(box.x + box.width - 1);
    const int firstRow = cellOf(box.y);
    const int lastRow = cellOf(box.y + box.height - 1);
    for (int cy = firstRow; cy <= lastRow; ++cy) {
        for (int cx = firstColumn; cx <= lastColumn; ++cx) {
            if (solidCell(collision, cx, cy)) {
                return true;
            }
        }
    }
    return false;
}

std::optional<RoomId> World::roomAt(int px, int py) const {
    const int cx = cellOf(px);
    const int cy = cellOf(py);
    for (RoomId id = 0; id < rooms.size(); ++id) {
        const Room& room = rooms[id];
        if (cx >= room.x && cx < room.x + room.width && cy >= room.y && cy < room.y + room.height) {
            return id;
        }
    }
    return std::nullopt;
}

}  // namespace zorlduh
