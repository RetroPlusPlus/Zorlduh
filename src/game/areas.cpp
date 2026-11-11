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

WorldKit::Layout overworld(const Assets& assets, CameraKit::View view) {
    using WorldKit::Side;
    using WorldKit::Transition;
    const OverworldMaps& maps = assets.areas.overworld;
    return WorldKit::Layout{
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
                    {.name = "dungeon door", .at = {.x = 30, .y = 2, .width = 2, .height = 1},
                     .transition = Transition::Fade,
                     .leadsTo = {.layout = Layout::DungeonOne, .area = Room::Entrance, .opening = "door"}},
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
}

WorldKit::Layout dungeonOne(const Assets& assets, CameraKit::View view) {
    using WorldKit::Side;
    using WorldKit::Transition;
    const DungeonOneMaps& maps = assets.areas.dungeonOne;
    return WorldKit::Layout{
        .areas = {
            WorldKit::Area{
                .tiles = &maps.entrance.tiles,
                .art = DungeonArt(assets),
                .collision = &maps.entrance.collision,
                .openings = {
                    {.name = "door", .at = {.x = 9, .y = 13, .width = 2, .height = 1}, .transition = Transition::Fade,
                     .leadsTo = {.layout = Layout::Overworld, .area = Area::Lake, .opening = "dungeon door"}},
                    {.name = "north door", .side = Side::North, .span = {8, 11}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Room::Hall, .opening = "south door"}},
                }},
            WorldKit::Area{
                .tiles = &maps.hall.tiles,
                .art = DungeonArt(assets),
                .collision = &maps.hall.collision,
                .openings = {
                    {.name = "south door", .side = Side::South, .span = {8, 11}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Room::Entrance, .opening = "north door"}},
                    {.name = "west door", .side = Side::West, .span = {6, 9}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Room::WestWing, .opening = "east door"}},
                    {.name = "east door", .side = Side::East, .span = {6, 9}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Room::EastWing, .opening = "west door"}},
                    // The treasury's open wall is as wide as this one and the two spans line up with the
                    // treasury centered over the hall, so no offset is needed.
                    {.name = "north wall", .side = Side::North, .span = {1, 18}, .transition = Transition::Seamless,
                     .leadsTo = {.area = Room::Treasury, .opening = "south wall"}},
                }},
            WorldKit::Area{
                .tiles = &maps.westWing.tiles,
                .art = DungeonArt(assets),
                .collision = &maps.westWing.collision,
                .openings = {
                    {.name = "east door", .side = Side::East, .span = {3, 6}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Room::Hall, .opening = "west door"}},
                }},
            WorldKit::Area{
                .tiles = &maps.eastWing.tiles,
                .art = DungeonArt(assets),
                .collision = &maps.eastWing.collision,
                .openings = {
                    {.name = "west door", .side = Side::West, .span = {3, 6}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Room::Hall, .opening = "east door"}},
                }},
            WorldKit::Area{
                .tiles = &maps.treasury.tiles,
                .art = DungeonArt(assets),
                .collision = &maps.treasury.collision,
                .openings = {
                    {.name = "south wall", .side = Side::South, .span = {6, 23}, .transition = Transition::Seamless,
                     .leadsTo = {.area = Room::Hall, .opening = "north wall"}},
                }},
        },
        .cellSize = kCellSize,
        .open = CollisionCell::Open,
        .view = view,
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

retropp::TileCatalog DungeonArt(const Assets& assets) {
    const Palettes& palettes = assets.palettes;
    return retropp::TileCatalog{.entries = {
        terrain(assets, WorldCell::Stairs, TerrainTile::Stairs, palettes.floor),
        terrain(assets, WorldCell::Wall, TerrainTile::Wall, palettes.water),
        terrain(assets, WorldCell::Water, TerrainTile::Water, palettes.water),
        terrain(assets, WorldCell::Doorway, TerrainTile::Doorway, palettes.wall),
        terrain(assets, WorldCell::Floor, TerrainTile::Floor, palettes.wall),
    }};
}

WorldKit::World gameWorld(const Assets& assets, CameraKit::View view) {
    WorldKit::World world{.layouts = {overworld(assets, view), dungeonOne(assets, view)}};
    WorldKit::validate(world);
    return world;
}

}  // namespace zorlduh
