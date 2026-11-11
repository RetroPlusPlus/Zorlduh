#include "game/areas.h"

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

retropp::TileCatalog OverworldArt(const Assets& assets) {
    const Palettes& palettes = assets.palettes;
    return retropp::TileCatalog{.entries = {
        terrain(assets, WorldCell::Stairs, TerrainTile::Stairs, palettes.floor),
        terrain(assets, WorldCell::Wall, TerrainTile::Wall, palettes.wall),
        terrain(assets, WorldCell::Water, TerrainTile::Water, palettes.water),
        terrain(assets, WorldCell::Doorway, TerrainTile::Doorway, palettes.floor),
        terrain(assets, WorldCell::Floor, TerrainTile::Floor, palettes.floor),
    }};
}

retropp::TileCatalog CaveArt(const Assets& assets) {
    const Palettes& palettes = assets.palettes;
    return retropp::TileCatalog{.entries = {
        terrain(assets, WorldCell::Stairs, TerrainTile::Stairs, palettes.wall),
        terrain(assets, WorldCell::Wall, TerrainTile::Wall, palettes.water),
        terrain(assets, WorldCell::Water, TerrainTile::Water, palettes.water),
        terrain(assets, WorldCell::Doorway, TerrainTile::Doorway, palettes.wall),
        terrain(assets, WorldCell::Floor, TerrainTile::Floor, palettes.wall),
    }};
}

WorldKit::Layout worldLayout(const Assets& assets, CameraKit::View view) {
    using WorldKit::Side;
    using WorldKit::Transition;
    const AreaMapSet& maps = assets.areas;
    WorldKit::Layout layout{
        .areas = {
            WorldKit::Area{
                .tiles = &maps.meadow.tiles,
                .art = OverworldArt(assets),
                .collision = &maps.meadow.collision,
                .openings = {
                    {.name = "east door", .side = Side::East, .span = {6, 9}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Area::Lake, .opening = "west door"}},
                    {.name = "south road", .side = Side::South, .span = {8, 11}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Area::Field, .opening = "north road"}},
                    {.name = "cave mouth", .at = {.x = 12, .y = 4, .width = 2, .height = 1},
                     .transition = Transition::Fade, .leadsTo = {.area = Area::Cave, .opening = "cave exit"}},
                }},
            WorldKit::Area{
                .tiles = &maps.lake.tiles,
                .art = OverworldArt(assets),
                .collision = &maps.lake.collision,
                .openings = {
                    {.name = "west door", .side = Side::West, .span = {6, 9}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Area::Meadow, .opening = "east door"}},
                }},
            WorldKit::Area{
                .tiles = &maps.field.tiles,
                .art = OverworldArt(assets),
                .collision = &maps.field.collision,
                .openings = {
                    {.name = "north road", .side = Side::North, .span = {8, 11}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Area::Meadow, .opening = "south road"}},
                    {.name = "east door", .side = Side::East, .span = {3, 6}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Area::Shrine, .opening = "west door"}},
                }},
            WorldKit::Area{
                .tiles = &maps.shrine.tiles,
                .art = OverworldArt(assets),
                .collision = &maps.shrine.collision,
                .openings = {
                    {.name = "west door", .side = Side::West, .span = {3, 6}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Area::Field, .opening = "east door"}},
                }},
            WorldKit::Area{
                .tiles = &maps.cave.tiles,
                .art = CaveArt(assets),
                .collision = &maps.cave.collision,
                .openings = {
                    {.name = "cave exit", .at = {.x = 5, .y = 4, .width = 2, .height = 1},
                     .transition = Transition::Fade, .leadsTo = {.area = Area::Meadow, .opening = "cave mouth"}},
                }},
        },
        .cellSize = kCellSize,
        .open = CollisionCell::Open,
        .view = view,
    };
    WorldKit::validate(layout);
    return layout;
}

}  // namespace zorlduh
