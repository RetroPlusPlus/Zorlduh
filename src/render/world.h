// The world's tiles: every cell of the world map, drawn from the terrain sheet through the palette of
// its kind, for one tile layer over the whole map.
//
//     layers::TileLayer(kWorldLayer, kWorldZ, WorldTiles(world, assets), world.tiles.width,
//                       world.tiles.height, {.scroll = {cameraX - kPlayAreaX, cameraY - kPlayAreaY}});
#pragma once

#include "retropp/tilemap.h"

#include "assets/assets.h"
#include "game/world.h"
#include "layers/types.h"

namespace zorlduh {

// The world map's cells, row-major, `world.tiles.width` across and `world.tiles.height` down. Each
// WorldCell value selects its TerrainTile; floor and doorway draw through the floor palette, and wall,
// water and void through their own.
[[nodiscard]] layers::Cells WorldTiles(const World& world, const Assets& assets);

// The table WorldTiles looks each map value up in: one entry per WorldCell value.
[[nodiscard]] retropp::TileCatalog TerrainCatalog(const Assets& assets);

}  // namespace zorlduh
