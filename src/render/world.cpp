#include "render/world.h"

#include <cstddef>
#include <cstdint>

namespace zorlduh {

namespace {

retropp::TileCatalogEntry terrain(const Assets& assets, WorldCell cell, TerrainTile tile,
                                  retropp::PaletteId palette) {
    return retropp::TileCatalogEntry{
        .id = static_cast<std::uint16_t>(cell),
        .sheet = assets.terrain.atlasId,
        .slot = assets.terrain[static_cast<std::size_t>(tile)].tile,
        .palette = palette,
    };
}

}  // namespace

retropp::TileCatalog TerrainCatalog(const Assets& assets) {
    const Palettes& palettes = assets.palettes;
    return retropp::TileCatalog{.entries = {
        terrain(assets, WorldCell::Void, TerrainTile::Void, palettes.voidCells),
        terrain(assets, WorldCell::Wall, TerrainTile::Wall, palettes.wall),
        terrain(assets, WorldCell::Water, TerrainTile::Water, palettes.water),
        terrain(assets, WorldCell::Doorway, TerrainTile::Doorway, palettes.floor),
        terrain(assets, WorldCell::Floor, TerrainTile::Floor, palettes.floor),
    }};
}

LayerKit::Cells WorldTiles(const World& world, const Assets& assets) {
    return retropp::assembleTilemap(world.tiles, TerrainCatalog(assets)).cells;
}

}  // namespace zorlduh
