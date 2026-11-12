#include "game/areas.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace zorlduh {

namespace {

// Each layout's area names, indexed by its enum.
constexpr std::array<std::string_view, 5> kOverworldNames{"Meadow", "Lake", "Field", "Shrine", "Cave"};
constexpr std::array<std::string_view, 5> kDungeonOneNames{"Entrance", "Hall", "West Wing", "East Wing",
                                                           "Treasury"};
constexpr std::array<std::string_view, 7> kSeamlessForestNames{"Edge",   "Glade", "Thicket", "Bower",
                                                               "Hollow", "Brook", "Clearing"};

retropp::TileCatalogEntry terrain(const Assets& assets, WorldCell cell, TerrainTile tile,
                                  retropp::PaletteId palette) {
    return retropp::TileCatalogEntry{
        .id = static_cast<std::uint16_t>(cell),
        .sheet = assets.terrain.atlasId,
        .slot = assets.terrain[static_cast<std::size_t>(tile)].tile,
        .palette = palette,
    };
}

retropp::TileCatalogEntry forest(const Assets& assets, WorldCell cell, ForestTile tile, retropp::PaletteId palette) {
    return retropp::TileCatalogEntry{
        .id = static_cast<std::uint16_t>(cell),
        .sheet = assets.forest.atlasId,
        .slot = assets.forest[static_cast<std::size_t>(tile)].tile,
        .palette = palette,
    };
}

retropp::TileCatalogEntry overlay(const Assets& assets, OverlayCell cell, OverlayTile tile, retropp::PaletteId palette) {
    return retropp::TileCatalogEntry{
        .id = static_cast<std::uint16_t>(cell),
        .sheet = assets.overlay.atlasId,
        .slot = assets.overlay[static_cast<std::size_t>(tile)].tile,
        .palette = palette,
    };
}

// An area's ground: its tile map through its art, at the bottom of the area.
WorldKit::MapLayer ground(const retropp::IndexGrid& tiles, retropp::TileCatalog art, const AreaDepths& depths) {
    return WorldKit::MapLayer{.name = "ground", .tiles = &tiles, .art = std::move(art), .z = depths.ground};
}

// A layer over or under the player at depth `z`, drawn through the overlay sheet.
WorldKit::MapLayer overlayLayer(std::string name, const retropp::IndexGrid& tiles, const Assets& assets,
                                std::int32_t z) {
    return WorldKit::MapLayer{.name = std::move(name), .tiles = &tiles, .art = OverlayArt(assets), .z = z};
}

// The actor slot every area has.
std::vector<WorldKit::ActorSlot> actorSlotsAt(const AreaDepths& depths) {
    return {WorldKit::ActorSlot{.name = std::string{kActorSlot}, .z = depths.actors}};
}

WorldKit::Layout overworld(const Assets& assets, CameraKit::View view, const AreaDepths& depths) {
    using WorldKit::Side;
    using WorldKit::Transition;
    const OverworldMaps& maps = assets.areas.overworld;
    return WorldKit::Layout{
        .areas = {
            WorldKit::Area{
                .layers = {ground(maps.meadow.tiles, OverworldArt(assets), depths),
                           overlayLayer("overhang", maps.meadowOverhang, assets, depths.overhead)},
                .actorSlots = actorSlotsAt(depths),
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
                .layers = {ground(maps.lake.tiles, OverworldArt(assets), depths)},
                .actorSlots = actorSlotsAt(depths),
                .collision = &maps.lake.collision,
                .openings = {
                    {.name = "west door", .side = Side::West, .span = {6, 9}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Area::Meadow, .opening = "east door"}},
                    {.name = "dungeon door", .at = {.x = 30, .y = 2, .width = 2, .height = 1},
                     .transition = Transition::Fade,
                     .leadsTo = {.layout = Layout::DungeonOne, .area = Room::Entrance, .opening = "door"}},
                }},
            WorldKit::Area{
                .layers = {ground(maps.field.tiles, OverworldArt(assets), depths)},
                .actorSlots = actorSlotsAt(depths),
                .collision = &maps.field.collision,
                .openings = {
                    {.name = "north road", .side = Side::North, .span = {8, 11}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Area::Meadow, .opening = "south road"}},
                    {.name = "east door", .side = Side::East, .span = {3, 6}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Area::Shrine, .opening = "west door"}},
                    {.name = "forest path", .side = Side::South, .span = {13, 16}, .transition = Transition::Scroll,
                     .leadsTo = {.layout = Layout::SeamlessForest, .area = Woods::Edge, .opening = "north path"}},
                }},
            WorldKit::Area{
                .layers = {ground(maps.shrine.tiles, OverworldArt(assets), depths)},
                .actorSlots = actorSlotsAt(depths),
                .collision = &maps.shrine.collision,
                .openings = {
                    {.name = "west door", .side = Side::West, .span = {3, 6}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Area::Field, .opening = "east door"}},
                }},
            WorldKit::Area{
                .layers = {ground(maps.cave.tiles, CaveArt(assets), depths)},
                .actorSlots = actorSlotsAt(depths),
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

WorldKit::Layout dungeonOne(const Assets& assets, CameraKit::View view, const AreaDepths& depths) {
    using WorldKit::Side;
    using WorldKit::Transition;
    const DungeonOneMaps& maps = assets.areas.dungeonOne;
    return WorldKit::Layout{
        .areas = {
            WorldKit::Area{
                .layers = {ground(maps.entrance.tiles, DungeonArt(assets), depths)},
                .actorSlots = actorSlotsAt(depths),
                .collision = &maps.entrance.collision,
                .openings = {
                    {.name = "door", .at = {.x = 9, .y = 13, .width = 2, .height = 1}, .transition = Transition::Fade,
                     .leadsTo = {.layout = Layout::Overworld, .area = Area::Lake, .opening = "dungeon door"}},
                    {.name = "north door", .side = Side::North, .span = {8, 11}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Room::Hall, .opening = "south door"}},
                }},
            WorldKit::Area{
                .layers = {ground(maps.hall.tiles, DungeonArt(assets), depths)},
                .actorSlots = actorSlotsAt(depths),
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
                .layers = {ground(maps.westWing.tiles, DungeonArt(assets), depths)},
                .actorSlots = actorSlotsAt(depths),
                .collision = &maps.westWing.collision,
                .openings = {
                    {.name = "east door", .side = Side::East, .span = {3, 6}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Room::Hall, .opening = "west door"}},
                }},
            WorldKit::Area{
                .layers = {ground(maps.eastWing.tiles, DungeonArt(assets), depths)},
                .actorSlots = actorSlotsAt(depths),
                .collision = &maps.eastWing.collision,
                .openings = {
                    {.name = "west door", .side = Side::West, .span = {3, 6}, .transition = Transition::Scroll,
                     .leadsTo = {.area = Room::Hall, .opening = "east door"}},
                }},
            WorldKit::Area{
                .layers = {ground(maps.treasury.tiles, DungeonArt(assets), depths)},
                .actorSlots = actorSlotsAt(depths),
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

WorldKit::Layout seamlessForest(const Assets& assets, CameraKit::View view, const AreaDepths& depths) {
    using WorldKit::Side;
    using WorldKit::Transition;
    const SeamlessForestMaps& maps = assets.areas.seamlessForest;
    const auto room = [&](const AreaMaps& area, std::vector<WorldKit::Opening> openings,
                          std::vector<WorldKit::MapLayer> over = {}) {
        std::vector<WorldKit::MapLayer> layers{ground(area.tiles, ForestArt(assets), depths)};
        layers.insert(layers.end(), std::make_move_iterator(over.begin()), std::make_move_iterator(over.end()));
        return WorldKit::Area{
            .layers = std::move(layers),
            .actorSlots = actorSlotsAt(depths),
            .collision = &area.collision,
            .openings = std::move(openings),
        };
    };
    return WorldKit::Layout{
        .areas = {
            room(maps.edge,
                 {
                     {.name = "north path", .side = Side::North, .span = {8, 11}, .transition = Transition::Scroll,
                      .leadsTo = {.layout = Layout::Overworld, .area = Area::Field, .opening = "forest path"}},
                     {.name = "east wall", .side = Side::East, .span = {1, 14}, .transition = Transition::Seamless,
                      .leadsTo = {.area = Woods::Glade, .opening = "west wall"}},
                 },
                 {overlayLayer("path", maps.edgePath, assets, depths.underfoot)}),
            room(maps.glade,
                 {
                     {.name = "west wall", .side = Side::West, .span = {3, 16}, .transition = Transition::Seamless,
                      .leadsTo = {.area = Woods::Edge, .opening = "east wall"}},
                     {.name = "north wall", .side = Side::North, .span = {12, 21}, .transition = Transition::Seamless,
                      .leadsTo = {.area = Woods::Thicket, .opening = "south wall"}},
                     {.name = "east wall", .side = Side::East, .span = {8, 15}, .transition = Transition::Seamless,
                      .leadsTo = {.area = Woods::Hollow, .opening = "west wall"}},
                     {.name = "south wall", .side = Side::South, .span = {5, 22}, .transition = Transition::Seamless,
                      .leadsTo = {.area = Woods::Clearing, .opening = "north wall"}},
                 },
                 {overlayLayer("canopy", maps.gladeCanopy, assets, depths.overhead)}),
            room(maps.thicket,
                 {
                     {.name = "south wall", .side = Side::South, .span = {1, 10}, .transition = Transition::Seamless,
                      .leadsTo = {.area = Woods::Glade, .opening = "north wall"}},
                     {.name = "east wall", .side = Side::East, .span = {1, 8}, .transition = Transition::Seamless,
                      .leadsTo = {.area = Woods::Bower, .opening = "west wall"}},
                 }),
            room(maps.bower,
                 {
                     {.name = "west wall", .side = Side::West, .span = {1, 8}, .transition = Transition::Seamless,
                      .leadsTo = {.area = Woods::Thicket, .opening = "east wall"}},
                 }),
            // The hollow's side walls are open along their whole height, so the brook can sit two cells
            // lower than level: the rows of the two walls that meet are the way through.
            room(maps.hollow,
                 {
                     {.name = "west wall", .side = Side::West, .span = {0, 7}, .transition = Transition::Seamless,
                      .leadsTo = {.area = Woods::Glade, .opening = "east wall"}},
                     {.name = "east wall", .side = Side::East, .span = {0, 7}, .transition = Transition::Seamless,
                      .leadsTo = {.area = Woods::Brook, .opening = "west wall", .offset = {.x = 0, .y = 2}}},
                 }),
            room(maps.brook,
                 {
                     {.name = "west wall", .side = Side::West, .span = {0, 7}, .transition = Transition::Seamless,
                      .leadsTo = {.area = Woods::Hollow, .opening = "east wall", .offset = {.x = 0, .y = -2}}},
                 }),
            room(maps.clearing,
                 {
                     {.name = "north wall", .side = Side::North, .span = {1, 18}, .transition = Transition::Seamless,
                      .leadsTo = {.area = Woods::Glade, .opening = "south wall"}},
                 }),
        },
        .cellSize = kCellSize,
        .open = CollisionCell::Open,
        .view = view,
    };
}

}  // namespace

std::string_view areaName(Layout layout, int area) noexcept {
    const auto pick = [area](const auto& names) -> std::string_view {
        return area >= 0 && static_cast<std::size_t>(area) < names.size() ? names[static_cast<std::size_t>(area)]
                                                                          : std::string_view{};
    };
    switch (layout) {
    case Layout::Overworld:
        return pick(kOverworldNames);
    case Layout::DungeonOne:
        return pick(kDungeonOneNames);
    case Layout::SeamlessForest:
        return pick(kSeamlessForestNames);
    }
    return {};
}

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

retropp::TileCatalog OverlayArt(const Assets& assets) {
    const Palettes& palettes = assets.palettes;
    return retropp::TileCatalog{.entries = {
        overlay(assets, OverlayCell::None, OverlayTile::Nothing, palettes.floor),
        overlay(assets, OverlayCell::Path, OverlayTile::Path, palettes.floor),
        overlay(assets, OverlayCell::Roof, OverlayTile::Roof, palettes.wall),
        overlay(assets, OverlayCell::Canopy, OverlayTile::Canopy, palettes.canopy),
    }};
}

retropp::TileCatalog ForestArt(const Assets& assets) {
    const Palettes& palettes = assets.palettes;
    return retropp::TileCatalog{.entries = {
        terrain(assets, WorldCell::Stairs, TerrainTile::Stairs, palettes.floor),
        forest(assets, WorldCell::Wall, ForestTile::Tree, palettes.leaf),
        forest(assets, WorldCell::Water, ForestTile::Stream, palettes.water),
        forest(assets, WorldCell::Doorway, ForestTile::Trodden, palettes.grass),
        forest(assets, WorldCell::Floor, ForestTile::Grass, palettes.grass),
    }};
}

WorldKit::World gameWorld(const Assets& assets, CameraKit::View view, const AreaDepths& depths) {
    WorldKit::World world{.layouts = {overworld(assets, view, depths), dungeonOne(assets, view, depths),
                                      seamlessForest(assets, view, depths)}};
    WorldKit::validate(world);
    return world;
}

}  // namespace zorlduh
