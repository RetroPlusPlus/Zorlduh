// The game's layouts and areas: their names, and the world they make with their maps, art and
// openings.
//
//     const WorldKit::World world = zorlduh::gameWorld(assets, kView);
//     WorldKit::Camera camera{zorlduh::kStartLayout, zorlduh::kStartArea};
#pragma once

#include <cstdint>

#include "retropp/tilemap.h"

#include "CameraKit/camera.h"
#include "WorldKit/area.h"

#include "assets/assets.h"

namespace zorlduh {

// Every layout, named. The values are the layouts' indices in the world, so an opening's destination
// is written as the layout's name. WorldKit::Layout, the module's type, is always written qualified.
enum Layout : std::uint8_t { Overworld, DungeonOne };

// Every overworld area, named. The values are the areas' indices in the overworld layout.
enum Area : std::uint8_t { Meadow, Lake, Field, Shrine, Cave };

// Every room of DungeonOne, named. The values are the rooms' indices in the dungeon layout.
enum Room : std::uint8_t { Entrance, Hall, WestWing, EastWing, Treasury };

// The side of one map cell, in pixels.
inline constexpr int kCellSize = 8;

// The layout and area the game starts in.
inline constexpr Layout kStartLayout = Layout::Overworld;
inline constexpr Area kStartArea = Area::Meadow;

// The world: the overworld, one area per Area in that order, and DungeonOne, one room per Room, each
// drawn through its own art, with the openings that join them. The meadow's east door scrolls into
// the lake, its south road scrolls into the field, and its cave mouth fades into the cave; the field's
// east door scrolls into the shrine; the lake's dungeon door fades into the dungeon's entrance, whose
// north door scrolls into the hall; the hall's west and east doors scroll into the wings, and its
// north wall is open along its whole length into the treasury, joined seamlessly; every opening has
// one leading back. Validated before it is returned.
[[nodiscard]] WorldKit::World gameWorld(const Assets& assets, CameraKit::View view);

// How an overworld area's map values are drawn: each kind of cell from its terrain tile through the
// palette of its kind; doorways and stairs through the floor palette.
[[nodiscard]] retropp::TileCatalog OverworldArt(const Assets& assets);

// How the cave's map values are drawn: the same tiles, floor and stairs in stone gray and walls in the
// water's blue.
[[nodiscard]] retropp::TileCatalog CaveArt(const Assets& assets);

// How a dungeon room's map values are drawn: the same tiles, floor and doorways in stone gray, walls
// in the water's blue, and stairs in the floor's brown.
[[nodiscard]] retropp::TileCatalog DungeonArt(const Assets& assets);

}  // namespace zorlduh
