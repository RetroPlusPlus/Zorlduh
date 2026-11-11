// The world the game is played in: its tile map, its collision map and its rooms, with the questions
// the simulation asks of them. Every position here is in world pixels, the map's own pixel space: cell
// (cx, cy) covers pixels 8·cx … 8·cx + 7 across and 8·cy … 8·cy + 7 down.
//
//     const zorlduh::World world{.tiles = assets.world, .collision = assets.collision};
//
//     world.solidBox({.x = x + 3, .y = y + 8, .width = 10, .height = 8});  // would this step collide?
//     world.roomAt(x + 8, y + 8);                                           // which room is this point in?
//
// A World is the solid query a TopDownMovement::Walker walks against: walker.walk(config, move, world).
#pragma once

#include <optional>
#include <span>

#include "retropp/geometry.h"
#include "retropp/image.h"

#include "game/rooms.h"
#include "TopDownMovement/collision.h"

namespace zorlduh {

// The side of one map cell, in pixels.
inline constexpr int kCellSize = 8;

struct World {
    retropp::IndexGrid tiles;      // one WorldCell value per cell
    retropp::IndexGrid collision;  // the same size, one CollisionCell value per cell
    std::span<const Room> rooms = kRooms;

    // The collision map's solid cells: every cell but Open, so wall, water and void are solid, and so
    // is every pixel off the map. Valid while this World is.
    [[nodiscard]] TopDownMovement::TileCollision solids() const;

    // Whether the pixel at (px, py) is in a solid cell.
    [[nodiscard]] bool solidAt(int px, int py) const;

    // Whether any pixel of `box` is in a solid cell. A box with no width or no height covers no cell
    // and is never solid. A box covers [x, x + width) across and [y, y + height) down.
    [[nodiscard]] bool solidBox(retropp::IntRect box) const;

    // The room whose rectangle holds the pixel at (px, py), or nothing for a pixel in void or off the
    // map.
    [[nodiscard]] std::optional<RoomId> roomAt(int px, int py) const;
};

}  // namespace zorlduh
