// The game's areas: their names, and the world they make with their maps, art and openings.
//
//     const WorldKit::Layout layout = zorlduh::worldLayout(assets, kView);
//     WorldKit::Camera camera{zorlduh::kStartArea};
#pragma once

#include <cstdint>

#include "retropp/tilemap.h"

#include "CameraKit/camera.h"
#include "WorldKit/area.h"

#include "assets/assets.h"

namespace zorlduh {

// Every area, named. The values are the areas' indices in the layout, so an opening's destination is
// written as the area's name.
enum Area : std::uint8_t { Meadow, Lake, Field, Shrine, Cave };

// The side of one map cell, in pixels.
inline constexpr int kCellSize = 8;

// The area the game starts in.
inline constexpr Area kStartArea = Area::Meadow;

// The world: one area per Area, in that order, each drawn through its own art, with the openings that
// join them. The meadow's east door scrolls into the lake, its south road scrolls into the field, and
// its cave mouth fades into the cave; the field's east door scrolls into the shrine; every opening has
// one leading back. Validated before it is returned.
[[nodiscard]] WorldKit::Layout worldLayout(const Assets& assets, CameraKit::View view);

// How an overworld area's map values are drawn: each kind of cell from its terrain tile through the
// palette of its kind; doorways and stairs through the floor palette.
[[nodiscard]] retropp::TileCatalog OverworldArt(const Assets& assets);

// How the cave's map values are drawn: the same tiles, floor and stairs in stone gray and walls in the
// water's blue.
[[nodiscard]] retropp::TileCatalog CaveArt(const Assets& assets);

}  // namespace zorlduh
