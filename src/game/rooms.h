// The world's rooms. A room is a rectangle of the world map, in cells; rooms differ in size and never
// overlap, and a cell outside every room is void. The table matches assets/maps/world/tiles.png: each
// room is ringed by wall except where an opening joins it to the next.
#pragma once

#include <array>
#include <cstddef>

namespace zorlduh {

// A rectangle of the world map in cells: `x` and `y` are its top-left cell, and it covers columns
// [x, x + width) and rows [y, y + height).
struct Room {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// A room's index in kRooms.
using RoomId = std::size_t;

inline constexpr std::array<Room, 4> kRooms{{
    {.x = 0, .y = 0, .width = 20, .height = 16},    // exactly one play area
    {.x = 20, .y = 0, .width = 40, .height = 16},   // two play areas wide
    {.x = 0, .y = 16, .width = 30, .height = 16},   // one and a half play areas wide
    {.x = 30, .y = 16, .width = 12, .height = 10},  // smaller than the play area on both axes
}};

// The room the game starts in.
inline constexpr RoomId kStartRoom = 0;

}  // namespace zorlduh
