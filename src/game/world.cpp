#include "game/world.h"

#include "assets/assets.h"

namespace zorlduh {

TopDownMovement::TileCollision World::solids() const {
    return TopDownMovement::TileCollision{.map = &collision, .cellSize = kCellSize, .open = CollisionCell::Open};
}

bool World::solidAt(int px, int py) const {
    return solids().solidAt({.x = px, .y = py});
}

bool World::solidBox(retropp::IntRect box) const {
    return solids().solidBox(box);
}

std::optional<RoomId> World::roomAt(int px, int py) const {
    const auto cellOf = [](int pixel) {
        return pixel >= 0 ? pixel / kCellSize : -((-pixel + kCellSize - 1) / kCellSize);
    };
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
