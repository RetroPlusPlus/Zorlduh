// The game's world: the three layouts over the real maps, every area's layers and actor slot, every
// opening's cells on the map it names, every way out having a way back, the five art catalogs, and the
// player going from area to area through the tick: the scroll east into the lake, the fade down into
// the cave, the camera following across the lake, input ignored while a transition runs; the dungeon —
// down from the lake, up into the hall, across the seam into the treasury and back, the wings, and
// out; and the forest — down from the field, then across every seam both ways, each crossing a case of
// how the camera follows. No renderer is needed: the maps are decoded from the bytes baked into the
// test binary and the art catalogs name handles the test makes up.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "retropp/atlas_manifest.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/palette.h"
#include "retropp/timing.h"

#include "TopDownMovement/walker.h"
#include "WorldKit/area.h"

#include "assets/assets.h"
#include "game/areas.h"
#include "game/player.h"
#include "game/sim.h"
#include "render/layout.h"

namespace {

using retropp::Vec2i;
using zorlduh::Area;
using zorlduh::CollisionCell;
using zorlduh::kCellSize;
using zorlduh::Layout;
using zorlduh::Room;
using zorlduh::WorldCell;
using zorlduh::Woods;

constexpr retropp::TimingProfile kHz60{.tickPeriodNs = retropp::TickPeriodNs::Hz60};

// The depths the world is built at, written out rather than read from the game's depth table.
constexpr zorlduh::AreaDepths kDepths{.ground = 0, .underfoot = 5, .actors = 10, .overhead = 15};

// The real maps, with the sheets and palettes as handles the test makes up.
zorlduh::Assets headlessAssets() {
    zorlduh::Assets assets;
    assets.terrain = retropp::AtlasManifest{
        .atlasId = retropp::AtlasId{7},
        .slots = retropp::sliceLayout({.width = 56, .height = 8}, {.width = 8, .height = 8},
                                      retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown),
        .kind = retropp::ContentKind::Tileset,
    };
    assets.forest = retropp::AtlasManifest{
        .atlasId = retropp::AtlasId{9},
        .slots = retropp::sliceLayout({.width = 32, .height = 8}, {.width = 8, .height = 8},
                                      retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown),
        .kind = retropp::ContentKind::Tileset,
    };
    assets.overlay = retropp::AtlasManifest{
        .atlasId = retropp::AtlasId{11},
        .slots = retropp::sliceLayout({.width = 32, .height = 8}, {.width = 8, .height = 8},
                                      retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown),
        .kind = retropp::ContentKind::Tileset,
    };
    assets.palettes = zorlduh::Palettes{
        .floor = retropp::PaletteId{4},
        .wall = retropp::PaletteId{8},
        .water = retropp::PaletteId{12},
        .grass = retropp::PaletteId{16},
        .leaf = retropp::PaletteId{20},
        .canopy = retropp::PaletteId{24},
    };
    zorlduh::OverworldMaps& overworld = assets.areas.overworld;
    overworld.meadow.tiles = retropp::loadMapPng("assets/maps/overworld/meadow/tiles.png");
    overworld.meadow.collision = retropp::loadMapPng("assets/maps/overworld/meadow/collision.png");
    overworld.meadowOverhang = retropp::loadMapPng("assets/maps/overworld/meadow/overhang.png");
    overworld.lake.tiles = retropp::loadMapPng("assets/maps/overworld/lake/tiles.png");
    overworld.lake.collision = retropp::loadMapPng("assets/maps/overworld/lake/collision.png");
    overworld.field.tiles = retropp::loadMapPng("assets/maps/overworld/field/tiles.png");
    overworld.field.collision = retropp::loadMapPng("assets/maps/overworld/field/collision.png");
    overworld.shrine.tiles = retropp::loadMapPng("assets/maps/overworld/shrine/tiles.png");
    overworld.shrine.collision = retropp::loadMapPng("assets/maps/overworld/shrine/collision.png");
    overworld.cave.tiles = retropp::loadMapPng("assets/maps/overworld/cave/tiles.png");
    overworld.cave.collision = retropp::loadMapPng("assets/maps/overworld/cave/collision.png");
    zorlduh::DungeonOneMaps& dungeon = assets.areas.dungeonOne;
    dungeon.entrance.tiles = retropp::loadMapPng("assets/maps/dungeon-one/entrance/tiles.png");
    dungeon.entrance.collision = retropp::loadMapPng("assets/maps/dungeon-one/entrance/collision.png");
    dungeon.hall.tiles = retropp::loadMapPng("assets/maps/dungeon-one/hall/tiles.png");
    dungeon.hall.collision = retropp::loadMapPng("assets/maps/dungeon-one/hall/collision.png");
    dungeon.westWing.tiles = retropp::loadMapPng("assets/maps/dungeon-one/west-wing/tiles.png");
    dungeon.westWing.collision = retropp::loadMapPng("assets/maps/dungeon-one/west-wing/collision.png");
    dungeon.eastWing.tiles = retropp::loadMapPng("assets/maps/dungeon-one/east-wing/tiles.png");
    dungeon.eastWing.collision = retropp::loadMapPng("assets/maps/dungeon-one/east-wing/collision.png");
    dungeon.treasury.tiles = retropp::loadMapPng("assets/maps/dungeon-one/treasury/tiles.png");
    dungeon.treasury.collision = retropp::loadMapPng("assets/maps/dungeon-one/treasury/collision.png");
    zorlduh::SeamlessForestMaps& forest = assets.areas.seamlessForest;
    forest.edge.tiles = retropp::loadMapPng("assets/maps/seamless-forest/edge/tiles.png");
    forest.edge.collision = retropp::loadMapPng("assets/maps/seamless-forest/edge/collision.png");
    forest.edgePath = retropp::loadMapPng("assets/maps/seamless-forest/edge/path.png");
    forest.glade.tiles = retropp::loadMapPng("assets/maps/seamless-forest/glade/tiles.png");
    forest.glade.collision = retropp::loadMapPng("assets/maps/seamless-forest/glade/collision.png");
    forest.gladeCanopy = retropp::loadMapPng("assets/maps/seamless-forest/glade/canopy.png");
    forest.thicket.tiles = retropp::loadMapPng("assets/maps/seamless-forest/thicket/tiles.png");
    forest.thicket.collision = retropp::loadMapPng("assets/maps/seamless-forest/thicket/collision.png");
    forest.bower.tiles = retropp::loadMapPng("assets/maps/seamless-forest/bower/tiles.png");
    forest.bower.collision = retropp::loadMapPng("assets/maps/seamless-forest/bower/collision.png");
    forest.hollow.tiles = retropp::loadMapPng("assets/maps/seamless-forest/hollow/tiles.png");
    forest.hollow.collision = retropp::loadMapPng("assets/maps/seamless-forest/hollow/collision.png");
    forest.brook.tiles = retropp::loadMapPng("assets/maps/seamless-forest/brook/tiles.png");
    forest.brook.collision = retropp::loadMapPng("assets/maps/seamless-forest/brook/collision.png");
    forest.clearing.tiles = retropp::loadMapPng("assets/maps/seamless-forest/clearing/tiles.png");
    forest.clearing.collision = retropp::loadMapPng("assets/maps/seamless-forest/clearing/collision.png");
    return assets;
}

zorlduh::Config headlessConfig(const zorlduh::Assets& assets) {
    zorlduh::Config config = zorlduh::gameConfig(assets, zorlduh::kView, kDepths, zorlduh::kDialogBox,
                                                 zorlduh::kMenuBoxes);
    config.player.timing = kHz60;
    return config;
}

zorlduh::GameState start(const zorlduh::Config& config) {
    zorlduh::GameState state{.player = TopDownMovement::Walker{zorlduh::kPlayerStart}};
    state.camera.place(config.world, state.player.collision(config.player).bounds());
    return state;
}

TopDownMovement::DirectionalInput Hold(TopDownMovement::Directions held) {
    return TopDownMovement::DirectionalInput{.held = held, .pressed = held};
}

// Ticks `count` times holding `held`, returning how many AreaChanged events were seen and the last.
struct Entered {
    int count = 0;
    zorlduh::AreaChanged last{};
};

Entered run(zorlduh::GameState& state, const zorlduh::Config& config, TopDownMovement::Directions held, int count) {
    Entered entered;
    for (int i = 0; i < count; ++i) {
        zorlduh::tick(state, zorlduh::Input{.move = Hold(held)}, config);
        for (const zorlduh::GameEvent& event : state.events) {
            if (const auto* changed = std::get_if<zorlduh::AreaChanged>(&event)) {
                ++entered.count;
                entered.last = *changed;
            }
        }
    }
    return entered;
}

std::uint16_t value(WorldCell cell) {
    return static_cast<std::uint16_t>(cell);
}

std::uint16_t value(CollisionCell cell) {
    return static_cast<std::uint16_t>(cell);
}

// ── The world ────────────────────────────────────────────────────────────────────────────────

TEST(Areas, OneLayoutPerNameAndOneAreaPerNameInOrderOverItsOwnMaps) {
    const zorlduh::Assets assets = headlessAssets();
    const WorldKit::World world = zorlduh::gameWorld(assets, zorlduh::kView, kDepths);
    ASSERT_EQ(world.layouts.size(), 3u);
    const WorldKit::Layout& overworld = world.layouts[Layout::Overworld];
    ASSERT_EQ(overworld.areas.size(), 5u);
    EXPECT_EQ(overworld.areas[Area::Meadow].layers[0].tiles, &assets.areas.overworld.meadow.tiles);
    EXPECT_EQ(overworld.areas[Area::Meadow].collision, &assets.areas.overworld.meadow.collision);
    EXPECT_EQ(overworld.areas[Area::Lake].layers[0].tiles, &assets.areas.overworld.lake.tiles);
    EXPECT_EQ(overworld.areas[Area::Field].layers[0].tiles, &assets.areas.overworld.field.tiles);
    EXPECT_EQ(overworld.areas[Area::Shrine].layers[0].tiles, &assets.areas.overworld.shrine.tiles);
    EXPECT_EQ(overworld.areas[Area::Cave].collision, &assets.areas.overworld.cave.collision);
    const WorldKit::Layout& dungeon = world.layouts[Layout::DungeonOne];
    ASSERT_EQ(dungeon.areas.size(), 5u);
    EXPECT_EQ(dungeon.areas[Room::Entrance].layers[0].tiles, &assets.areas.dungeonOne.entrance.tiles);
    EXPECT_EQ(dungeon.areas[Room::Hall].collision, &assets.areas.dungeonOne.hall.collision);
    EXPECT_EQ(dungeon.areas[Room::WestWing].layers[0].tiles, &assets.areas.dungeonOne.westWing.tiles);
    EXPECT_EQ(dungeon.areas[Room::EastWing].layers[0].tiles, &assets.areas.dungeonOne.eastWing.tiles);
    EXPECT_EQ(dungeon.areas[Room::Treasury].layers[0].tiles, &assets.areas.dungeonOne.treasury.tiles);
    const WorldKit::Layout& forest = world.layouts[Layout::SeamlessForest];
    ASSERT_EQ(forest.areas.size(), 7u);
    EXPECT_EQ(forest.areas[Woods::Edge].layers[0].tiles, &assets.areas.seamlessForest.edge.tiles);
    EXPECT_EQ(forest.areas[Woods::Glade].collision, &assets.areas.seamlessForest.glade.collision);
    EXPECT_EQ(forest.areas[Woods::Thicket].layers[0].tiles, &assets.areas.seamlessForest.thicket.tiles);
    EXPECT_EQ(forest.areas[Woods::Bower].layers[0].tiles, &assets.areas.seamlessForest.bower.tiles);
    EXPECT_EQ(forest.areas[Woods::Hollow].layers[0].tiles, &assets.areas.seamlessForest.hollow.tiles);
    EXPECT_EQ(forest.areas[Woods::Brook].collision, &assets.areas.seamlessForest.brook.collision);
    EXPECT_EQ(forest.areas[Woods::Clearing].layers[0].tiles, &assets.areas.seamlessForest.clearing.tiles);
    for (const WorldKit::Layout& layout : world.layouts) {
        EXPECT_EQ(layout.cellSize, kCellSize);
        EXPECT_EQ(layout.open.value, value(CollisionCell::Open));
        EXPECT_EQ(layout.view, zorlduh::kView);
        EXPECT_EQ(layout.seamDepth, 0u);
    }
    EXPECT_EQ(zorlduh::kStartLayout, Layout::Overworld);
    EXPECT_EQ(zorlduh::kStartArea, Area::Meadow);
}

// Every area is its ground at the ground's depth with the actor slot at the actors' depth; the meadow,
// the edge and the glade each have one more layer, at the depth of what it is.
TEST(Areas, EveryAreaHasItsGroundAndTheActorSlotAndThreeHaveAnOverlay) {
    const zorlduh::Assets assets = headlessAssets();
    const WorldKit::World world = zorlduh::gameWorld(assets, zorlduh::kView, kDepths);
    struct Over {
        Layout layout;
        int area;
        const char* name;
        const retropp::IndexGrid* tiles;
        std::int32_t z;
    };
    const std::vector<Over> overlays{
        {Layout::Overworld, Area::Meadow, "overhang", &assets.areas.overworld.meadowOverhang, kDepths.overhead},
        {Layout::SeamlessForest, Woods::Edge, "path", &assets.areas.seamlessForest.edgePath, kDepths.underfoot},
        {Layout::SeamlessForest, Woods::Glade, "canopy", &assets.areas.seamlessForest.gladeCanopy, kDepths.overhead},
    };
    int layered = 0;
    for (std::size_t l = 0; l < world.layouts.size(); ++l) {
        for (std::size_t a = 0; a < world.layouts[l].areas.size(); ++a) {
            const WorldKit::Area& area = world.layouts[l].areas[a];
            const std::string name = std::to_string(l) + ":" + std::to_string(a);
            ASSERT_GE(area.layers.size(), 1u) << name;
            EXPECT_EQ(area.layers[0].name, "ground") << name;
            EXPECT_EQ(area.layers[0].z, kDepths.ground) << name;
            ASSERT_EQ(area.actorSlots.size(), 1u) << name;
            EXPECT_EQ(area.actorSlots[0].name, zorlduh::kActorSlot) << name;
            EXPECT_EQ(area.actorSlots[0].z, kDepths.actors) << name;
            std::size_t expected = 1;
            for (const Over& over : overlays) {
                if (static_cast<std::size_t>(over.layout) == l && static_cast<std::size_t>(over.area) == a) {
                    expected = 2;
                    ++layered;
                    ASSERT_EQ(area.layers.size(), 2u) << name;
                    EXPECT_EQ(area.layers[1].name, over.name) << name;
                    EXPECT_EQ(area.layers[1].tiles, over.tiles) << name;
                    EXPECT_EQ(area.layers[1].z, over.z) << name;
                    EXPECT_EQ(area.layers[1].art.entries[0].sheet, assets.overlay.atlasId) << name;
                }
            }
            EXPECT_EQ(area.layers.size(), expected) << name;
        }
    }
    EXPECT_EQ(layered, 3);
}

TEST(Areas, EveryWayOutHasAWayBack) {
    const zorlduh::Assets assets = headlessAssets();
    const WorldKit::World world = zorlduh::gameWorld(assets, zorlduh::kView, kDepths);
    int exits = 0;
    for (std::size_t l = 0; l < world.layouts.size(); ++l) {
        const WorldKit::Layout& layout = world.layouts[l];
        for (std::size_t a = 0; a < layout.areas.size(); ++a) {
            for (const WorldKit::Opening& opening : layout.areas[a].openings) {
                const std::string name = "layout " + std::to_string(l) + " area " + std::to_string(a) + " '" + opening.name + "'";
                ASSERT_NE(opening.leadsTo.area, WorldKit::kNoArea) << name;
                ++exits;
                const WorldKit::Place to = WorldKit::detail::resolve(opening.leadsTo, static_cast<int>(l));
                const WorldKit::Area& next = world.layouts[static_cast<std::size_t>(to.layout)].areas[static_cast<std::size_t>(to.area)];
                bool back = false;
                for (const WorldKit::Opening& entrance : next.openings) {
                    const WorldKit::Place from = WorldKit::detail::resolve(entrance.leadsTo, to.layout);
                    back = back || (entrance.name == opening.leadsTo.opening &&
                                    from == WorldKit::Place{.layout = static_cast<int>(l), .area = static_cast<int>(a)} &&
                                    entrance.leadsTo.opening == opening.name &&
                                    entrance.transition == opening.transition);
                }
                EXPECT_TRUE(back) << name;
            }
        }
    }
    EXPECT_EQ(exits, 32);
}

// The doorway and stairs cells on each map are exactly where its openings say.
TEST(Areas, EveryOpeningIsOnItsMap) {
    const zorlduh::Assets assets = headlessAssets();
    const WorldKit::World world = zorlduh::gameWorld(assets, zorlduh::kView, kDepths);
    for (std::size_t l = 0; l < world.layouts.size(); ++l) {
        for (std::size_t a = 0; a < world.layouts[l].areas.size(); ++a) {
            const WorldKit::Area& area = world.layouts[l].areas[a];
            const retropp::IndexGrid& tiles = *area.layers[0].tiles;
            const std::string name = std::to_string(l) + ":" + std::to_string(a);
            std::set<std::pair<int, int>> openingCells;
            for (const WorldKit::Opening& opening : area.openings) {
                if (opening.side == WorldKit::Side::None) {
                    for (int y = opening.at.y; y < opening.at.y + opening.at.height; ++y) {
                        for (int x = opening.at.x; x < opening.at.x + opening.at.width; ++x) {
                            EXPECT_EQ(tiles.at(x, y), value(WorldCell::Stairs)) << name << " " << opening.name;
                            openingCells.insert({x, y});
                        }
                    }
                    continue;
                }
                for (int along = opening.span.first; along <= opening.span.last; ++along) {
                    int x = along;
                    int y = along;
                    switch (opening.side) {
                        case WorldKit::Side::North: y = 0; break;
                        case WorldKit::Side::South: y = tiles.height - 1; break;
                        case WorldKit::Side::West: x = 0; break;
                        case WorldKit::Side::East: x = tiles.width - 1; break;
                        case WorldKit::Side::None: break;
                    }
                    EXPECT_EQ(tiles.at(x, y), value(WorldCell::Doorway)) << name << " " << opening.name;
                    openingCells.insert({x, y});
                }
            }
            for (int y = 0; y < tiles.height; ++y) {
                for (int x = 0; x < tiles.width; ++x) {
                    const std::uint16_t cell = tiles.at(x, y);
                    if (cell == value(WorldCell::Doorway) || cell == value(WorldCell::Stairs)) {
                        EXPECT_TRUE(openingCells.contains({x, y})) << name << " " << x << "," << y;
                    }
                }
            }
        }
    }
}

TEST(Areas, TheArtDrawsEveryKindOfCellFromItsTileThroughItsPalette) {
    const zorlduh::Assets assets = headlessAssets();
    const auto entry = [](const retropp::TileCatalog& art, WorldCell cell) {
        for (const retropp::TileCatalogEntry& e : art.entries) {
            if (e.id == value(cell)) {
                return e;
            }
        }
        return retropp::TileCatalogEntry{};
    };
    const retropp::TileCatalog overworld = zorlduh::OverworldArt(assets);
    EXPECT_EQ(entry(overworld, WorldCell::Floor).slot, 0);
    EXPECT_EQ(entry(overworld, WorldCell::Floor).palette, assets.palettes.floor);
    EXPECT_EQ(entry(overworld, WorldCell::Wall).slot, 1);
    EXPECT_EQ(entry(overworld, WorldCell::Wall).palette, assets.palettes.wall);
    EXPECT_EQ(entry(overworld, WorldCell::Water).slot, 2);
    EXPECT_EQ(entry(overworld, WorldCell::Water).palette, assets.palettes.water);
    EXPECT_EQ(entry(overworld, WorldCell::Doorway).slot, 3);
    EXPECT_EQ(entry(overworld, WorldCell::Doorway).palette, assets.palettes.floor);
    EXPECT_EQ(entry(overworld, WorldCell::Stairs).slot, 4);
    EXPECT_EQ(entry(overworld, WorldCell::Stairs).sheet, assets.terrain.atlasId);
    EXPECT_EQ(entry(overworld, WorldCell::Sign).slot, 5);
    EXPECT_EQ(entry(overworld, WorldCell::Sign).palette, assets.palettes.floor);
    EXPECT_EQ(entry(overworld, WorldCell::Rock).slot, 6);
    EXPECT_EQ(entry(overworld, WorldCell::Rock).palette, assets.palettes.wall);
    const retropp::TileCatalog cave = zorlduh::CaveArt(assets);
    EXPECT_EQ(entry(cave, WorldCell::Floor).slot, 0);
    EXPECT_EQ(entry(cave, WorldCell::Floor).palette, assets.palettes.wall);
    EXPECT_EQ(entry(cave, WorldCell::Wall).palette, assets.palettes.water);
    const retropp::TileCatalog dungeon = zorlduh::DungeonArt(assets);
    EXPECT_EQ(entry(dungeon, WorldCell::Floor).palette, assets.palettes.wall);
    EXPECT_EQ(entry(dungeon, WorldCell::Doorway).palette, assets.palettes.wall);
    EXPECT_EQ(entry(dungeon, WorldCell::Wall).palette, assets.palettes.water);
    EXPECT_EQ(entry(dungeon, WorldCell::Stairs).palette, assets.palettes.floor);
    EXPECT_EQ(entry(dungeon, WorldCell::Stairs).slot, 4);
    const retropp::TileCatalog forest = zorlduh::ForestArt(assets);
    EXPECT_EQ(entry(forest, WorldCell::Floor).sheet, assets.forest.atlasId);
    EXPECT_EQ(entry(forest, WorldCell::Floor).slot, 0);
    EXPECT_EQ(entry(forest, WorldCell::Floor).palette, assets.palettes.grass);
    EXPECT_EQ(entry(forest, WorldCell::Wall).sheet, assets.forest.atlasId);
    EXPECT_EQ(entry(forest, WorldCell::Wall).slot, 1);
    EXPECT_EQ(entry(forest, WorldCell::Wall).palette, assets.palettes.leaf);
    EXPECT_EQ(entry(forest, WorldCell::Water).slot, 2);
    EXPECT_EQ(entry(forest, WorldCell::Water).palette, assets.palettes.water);
    EXPECT_EQ(entry(forest, WorldCell::Doorway).slot, 3);
    EXPECT_EQ(entry(forest, WorldCell::Doorway).palette, assets.palettes.grass);
    EXPECT_EQ(entry(forest, WorldCell::Stairs).sheet, assets.terrain.atlasId);
    EXPECT_EQ(entry(forest, WorldCell::Stairs).slot, 4);
    EXPECT_EQ(entry(forest, WorldCell::Stairs).palette, assets.palettes.floor);
    // Every value any map holds has an entry in all four.
    const WorldKit::World world = zorlduh::gameWorld(assets, zorlduh::kView, kDepths);
    for (const WorldKit::Layout& layout : world.layouts) {
        for (const WorldKit::Area& area : layout.areas) {
            for (const std::uint16_t cell : area.layers[0].tiles->values) {
                EXPECT_NE(entry(overworld, static_cast<WorldCell>(cell)).sheet, retropp::AtlasId{}) << cell;
                EXPECT_NE(entry(cave, static_cast<WorldCell>(cell)).sheet, retropp::AtlasId{}) << cell;
                EXPECT_NE(entry(dungeon, static_cast<WorldCell>(cell)).sheet, retropp::AtlasId{}) << cell;
                EXPECT_NE(entry(forest, static_cast<WorldCell>(cell)).sheet, retropp::AtlasId{}) << cell;
            }
        }
    }
}

TEST(Areas, TheOverlayArtDrawsEachOverlayCellFromItsSlotThroughItsPalette) {
    const zorlduh::Assets assets = headlessAssets();
    const retropp::TileCatalog art = zorlduh::OverlayArt(assets);
    using zorlduh::OverlayCell;
    using zorlduh::OverlayTile;
    const auto entry = [&](OverlayCell cell) {
        for (const retropp::TileCatalogEntry& e : art.entries) {
            if (e.id == static_cast<std::uint16_t>(cell)) {
                return e;
            }
        }
        return retropp::TileCatalogEntry{};
    };
    const auto slot = [&](OverlayTile tile) { return assets.overlay[static_cast<std::size_t>(tile)].tile; };
    ASSERT_EQ(art.entries.size(), 4u);
    for (const retropp::TileCatalogEntry& e : art.entries) {
        EXPECT_EQ(e.sheet, assets.overlay.atlasId) << e.id;
    }
    EXPECT_EQ(entry(OverlayCell::None).slot, slot(OverlayTile::Nothing));
    EXPECT_EQ(entry(OverlayCell::Canopy).slot, slot(OverlayTile::Canopy));
    EXPECT_EQ(entry(OverlayCell::Canopy).palette, assets.palettes.canopy);
    EXPECT_EQ(entry(OverlayCell::Path).slot, slot(OverlayTile::Path));
    EXPECT_EQ(entry(OverlayCell::Path).palette, assets.palettes.floor);
    EXPECT_EQ(entry(OverlayCell::Roof).slot, slot(OverlayTile::Roof));
    EXPECT_EQ(entry(OverlayCell::Roof).palette, assets.palettes.wall);
    // Every value the three overlay maps hold has an entry.
    for (const retropp::IndexGrid* map : {&assets.areas.overworld.meadowOverhang, &assets.areas.seamlessForest.edgePath,
                                          &assets.areas.seamlessForest.gladeCanopy}) {
        for (const std::uint16_t cell : map->values) {
            EXPECT_NE(entry(static_cast<OverlayCell>(cell)).sheet, retropp::AtlasId{}) << cell;
        }
    }
}

// ── The player through the tick ──────────────────────────────────────────────────────────────

TEST(Travel, StartsInTheMeadowWithTheCameraAtItsTopLeft) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    const zorlduh::GameState state = start(config);
    EXPECT_EQ(state.camera.layout(), Layout::Overworld);
    EXPECT_EQ(state.camera.area(), Area::Meadow);
    EXPECT_EQ(state.camera.position(), (Vec2i{0, 0}));
    EXPECT_EQ(state.camera.scroll(config.world), (retropp::LayerScroll{.x = 0, .y = -16}));
    EXPECT_FALSE(config.player.collision.hits(state.camera.solids(config.world), zorlduh::kPlayerStart));
}

// Walking right from (72, 56): the box's center, 8 px in, crosses the east wall at x = 152 on tick 80,
// the scroll takes 32 ticks and carries the player 16 px, and the lake's pixels start 160 px right.
TEST(Travel, WalkingOutThroughTheEastDoorScrollsIntoTheLake) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = start(config);

    Entered entered = run(state, config, {.right = true}, 80);
    EXPECT_EQ(state.player.position(), (Vec2i{152, 56}));
    EXPECT_TRUE(state.camera.transitioning());
    EXPECT_EQ(state.camera.area(), Area::Meadow);
    EXPECT_EQ(entered.count, 0);

    // Mid-scroll, holding up changes nothing: the player is carried, not walked.
    entered = run(state, config, {.up = true}, 16);
    EXPECT_EQ(state.player.position(), (Vec2i{160, 56}));
    EXPECT_EQ(state.camera.position(), (Vec2i{80, 0}));
    EXPECT_EQ(entered.count, 0);

    entered = run(state, config, {.up = true}, 16);
    EXPECT_FALSE(state.camera.transitioning());
    EXPECT_EQ(state.camera.area(), Area::Lake);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.layout, Layout::Overworld);
    EXPECT_EQ(entered.last.area, Area::Lake);
    EXPECT_EQ(state.player.position(), (Vec2i{8, 56}));
    EXPECT_EQ(state.camera.position(), (Vec2i{0, 0}));

    // Now walking again, in the lake's pixels.
    entered = run(state, config, {.right = true}, 10);
    EXPECT_EQ(state.player.position(), (Vec2i{18, 56}));
    EXPECT_EQ(entered.count, 0);
}

TEST(Travel, TheCameraFollowsAcrossTheLakeAndStopsAtItsEdges) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = start(config);
    run(state, config, {.right = true}, 112);
    ASSERT_EQ(state.camera.area(), Area::Lake);

    // Walking right along row 56 (the box on rows 64–71) clears the pond (rows 40–87, columns 128–191)
    // only below it, so walk down first.
    run(state, config, {.down = true}, 40);
    EXPECT_EQ(state.player.position(), (Vec2i{8, 96}));
    run(state, config, {.right = true}, 100);
    EXPECT_EQ(state.player.position(), (Vec2i{108, 96}));   // center x 116: the camera follows
    EXPECT_EQ(state.camera.position(), (Vec2i{36, 0}));
    run(state, config, {.right = true}, 300);
    EXPECT_EQ(state.player.position(), (Vec2i{299, 96}));   // the box flush with the east wall at 312
    EXPECT_EQ(state.camera.position(), (Vec2i{160, 0}));    // stopped at the lake's edge
    EXPECT_FALSE(state.camera.transitioning());
}

// The field is one and a half screens wide and the meadow one. Coming back up the north road from the
// road's right-hand side, the camera is 8 px right of where the meadow fits, and the scroll slides
// up only, so the meadow lands 8 px left with void beside it. It fills the screen, so the camera comes
// into the meadow's place as the player walks left — those 8 px, with the player's own steps — and
// not at all while the player stands or walks right.
TEST(Travel, AnAreaThatFillsTheScreenComesIntoPlaceAsThePlayerWalksThatWay) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = start(config);
    run(state, config, {.down = true}, 60 + 32);   // down the south road into the field
    ASSERT_EQ(state.camera.area(), Area::Field);
    EXPECT_EQ(state.player.position(), (Vec2i{72, 4}));
    EXPECT_EQ(state.camera.position(), (Vec2i{0, 0}));
    run(state, config, {.right = true}, 100);     // the camera follows to the field's right
    EXPECT_EQ(state.camera.position(), (Vec2i{80, 0}));
    run(state, config, {.left = true}, 92);        // back to the road's right-hand columns
    EXPECT_EQ(state.player.position(), (Vec2i{80, 4}));
    EXPECT_EQ(state.camera.position(), (Vec2i{8, 0}));
    Entered entered = run(state, config, {.up = true}, 17 + 32);
    EXPECT_EQ(entered.last.area, Area::Meadow);
    EXPECT_EQ(state.player.position(), (Vec2i{80, 99}));
    EXPECT_EQ(state.camera.position(), (Vec2i{8, 0}));
    run(state, config, {}, 30);
    EXPECT_EQ(state.camera.position(), (Vec2i{8, 0}));
    run(state, config, {.right = true}, 20);
    EXPECT_EQ(state.camera.position(), (Vec2i{8, 0}));
    run(state, config, {.left = true}, 5);
    EXPECT_EQ(state.camera.position(), (Vec2i{3, 0}));
    run(state, config, {.left = true}, 20);
    EXPECT_EQ(state.camera.position(), (Vec2i{0, 0}));
    run(state, config, {.right = true}, 20);
    EXPECT_EQ(state.camera.position(), (Vec2i{0, 0}));
}

// The cave mouth is cells (12, 4) and (13, 4): pixels 96–111 across, 32–39 down. The player's box
// center is its position plus (8, 12), so standing at (92, 24) puts it on the stairs.
TEST(Travel, SteppingOnTheCaveMouthFadesDownIntoTheCave) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = start(config);
    state.player.place({.x = 92, .y = 40});
    run(state, config, {}, 1);
    EXPECT_FALSE(state.camera.transitioning());
    state.player.place({.x = 92, .y = 24});
    Entered entered = run(state, config, {}, 1);
    EXPECT_TRUE(state.camera.transitioning());
    EXPECT_EQ(entered.count, 0);

    entered = run(state, config, {.left = true}, 16);
    EXPECT_EQ(state.camera.area(), Area::Cave);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.area, Area::Cave);
    // Centered on the cave's stairs, cells (5, 4)–(6, 4): pixels 40–55 × 32–39.
    EXPECT_EQ(state.player.position(), (Vec2i{40, 24}));
    EXPECT_EQ(state.camera.position(), (Vec2i{-32, -24}));   // the cave is centered on screen

    entered = run(state, config, {.left = true}, 16);
    EXPECT_FALSE(state.camera.transitioning());
    EXPECT_EQ(entered.count, 0);
    EXPECT_EQ(state.player.position(), (Vec2i{40, 24}));   // not walked while fading in

    // Walking off the stairs and back fades up again.
    run(state, config, {.down = true}, 8);
    entered = run(state, config, {.up = true}, 8);
    EXPECT_TRUE(state.camera.transitioning());
    entered = run(state, config, {}, 16);
    EXPECT_EQ(entered.last.area, Area::Meadow);
    EXPECT_EQ(state.player.position(), (Vec2i{96, 24}));   // centered on the cave mouth: box (99, 32)
}

// ── The dungeon ──────────────────────────────────────────────────────────────────────────────

// The lake's dungeon door is cells (30, 2) and (31, 2): pixels 240–255 × 16–23, so a player at
// (236, 8) has its box's center on it. It fades to the entrance's door, cells (9, 13)–(10, 13):
// pixels 72–87 × 104–111, where the 10×8 box is centered at (75, 104) — the player at (72, 96).
zorlduh::GameState inTheEntrance(const zorlduh::Config& config) {
    zorlduh::GameState state = start(config);
    run(state, config, {.right = true}, 112);   // into the lake
    state.player.place({.x = 236, .y = 24});
    run(state, config, {}, 1);
    state.player.place({.x = 236, .y = 8});
    run(state, config, {}, 1);
    run(state, config, {}, 32);
    return state;
}

TEST(Dungeon, TheLakesDungeonDoorFadesDownIntoTheEntrance) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = start(config);
    run(state, config, {.right = true}, 112);
    state.player.place({.x = 236, .y = 24});
    run(state, config, {}, 1);
    EXPECT_FALSE(state.camera.transitioning());
    state.player.place({.x = 236, .y = 8});
    Entered entered = run(state, config, {}, 1);
    EXPECT_TRUE(state.camera.transitioning());
    EXPECT_EQ(state.camera.layout(), Layout::Overworld);

    entered = run(state, config, {}, 16);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.layout, Layout::DungeonOne);
    EXPECT_EQ(entered.last.area, Room::Entrance);
    EXPECT_EQ(state.camera.layout(), Layout::DungeonOne);
    EXPECT_EQ(state.camera.area(), Room::Entrance);
    EXPECT_EQ(state.player.position(), (Vec2i{72, 96}));
    EXPECT_EQ(state.camera.position(), (Vec2i{0, 0}));   // the entrance is one screen
    run(state, config, {}, 16);
    EXPECT_FALSE(state.camera.transitioning());
}

// From (72, 96) the box's center (80, 108) reaches the north wall, y < 0, after 109 ticks up; the hall
// scrolls in level (both doors are columns 8–11) and the carry takes the player 16 px further.
TEST(Dungeon, TheEntrancesNorthDoorScrollsIntoTheHall) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = inTheEntrance(config);
    ASSERT_EQ(state.camera.area(), Room::Entrance);
    Entered entered = run(state, config, {.up = true}, 109);
    EXPECT_EQ(state.player.position(), (Vec2i{72, -13}));
    EXPECT_TRUE(state.camera.transitioning());
    entered = run(state, config, {.up = true}, 32);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.layout, Layout::DungeonOne);
    EXPECT_EQ(entered.last.area, Room::Hall);
    EXPECT_EQ(state.camera.area(), Room::Hall);
    EXPECT_EQ(state.player.position(), (Vec2i{72, 99}));   // −13 − 16 + 128
    EXPECT_EQ(state.camera.position(), (Vec2i{0, 0}));
    EXPECT_FALSE(state.camera.transitioning());
}

// The treasury (30×16) sits over the hall (20×16) with their open walls level: its top-left is
// (−40, −128) in the hall's pixels. From (72, 99) the box's center reaches y < 0 after 112 ticks up and
// the switch is immediate: the player is re-based by (40, 128), the camera by the same.
TEST(Dungeon, TheHallsNorthWallIsASeamlessJoinWithTheTreasuryAndBack) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = inTheEntrance(config);
    run(state, config, {.up = true}, 141);
    ASSERT_EQ(state.camera.area(), Room::Hall);
    ASSERT_EQ(state.player.position(), (Vec2i{72, 99}));

    Entered entered = run(state, config, {.up = true}, 111);
    EXPECT_EQ(state.player.position(), (Vec2i{72, -12}));
    EXPECT_EQ(state.camera.area(), Room::Hall);
    EXPECT_EQ(state.camera.position(), (Vec2i{0, -64}));   // the law follows the center (80, 0) up past the wall
    EXPECT_EQ(entered.count, 0);
    // Both rooms are on screen, the treasury's layer 128 px up and 40 px left.
    std::vector<WorldKit::Shown> shown = state.camera.shown(config.world);
    ASSERT_EQ(shown.size(), 2u);
    EXPECT_EQ(shown[1].area, Room::Treasury);
    EXPECT_EQ(shown[1].scroll, (retropp::LayerScroll{.x = 40, .y = -64 - 16 + 128}));

    entered = run(state, config, {.up = true}, 1);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.layout, Layout::DungeonOne);
    EXPECT_EQ(entered.last.area, Room::Treasury);
    EXPECT_EQ(state.camera.area(), Room::Treasury);
    EXPECT_FALSE(state.camera.transitioning());
    EXPECT_EQ(state.player.position(), (Vec2i{112, 115}));
    EXPECT_EQ(state.camera.position(), (Vec2i{40, 63}));
    // The hall's layer keeps the scroll it had and the treasury's is now the camera's own.
    shown = state.camera.shown(config.world);
    ASSERT_EQ(shown.size(), 2u);
    EXPECT_EQ(shown[0].area, Room::Treasury);
    EXPECT_EQ(shown[0].scroll, (retropp::LayerScroll{.x = 40, .y = 47}));
    EXPECT_EQ(shown[1].area, Room::Hall);
    EXPECT_EQ(shown[1].scroll, (retropp::LayerScroll{.x = 0, .y = -81}));   // one tick further up than before

    // Walking on, the camera follows into the treasury; walking back down crosses the seam again.
    run(state, config, {.up = true}, 40);
    EXPECT_EQ(state.player.position(), (Vec2i{112, 75}));
    EXPECT_EQ(state.camera.position(), (Vec2i{40, 23}));
    entered = run(state, config, {.down = true}, 41);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.area, Room::Hall);
    EXPECT_EQ(state.camera.area(), Room::Hall);
    EXPECT_EQ(state.player.position(), (Vec2i{72, -12}));
    EXPECT_FALSE(state.camera.transitioning());
}

// From the hall's (72, 99), the west door (rows 6–9, pixels 48–79) is reached by walking up to row 60
// and left; the box's center crosses x < 0 at position x = −9.
TEST(Dungeon, TheWingsAreReachedThroughTheHallsSideDoors) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = inTheEntrance(config);
    run(state, config, {.up = true}, 141);
    ASSERT_EQ(state.camera.area(), Room::Hall);
    run(state, config, {.up = true}, 39);
    EXPECT_EQ(state.player.position(), (Vec2i{72, 60}));
    Entered entered = run(state, config, {.left = true}, 81);
    EXPECT_EQ(state.player.position(), (Vec2i{-9, 60}));
    EXPECT_TRUE(state.camera.transitioning());
    entered = run(state, config, {}, 32);
    EXPECT_EQ(entered.last.area, Room::WestWing);
    EXPECT_EQ(state.camera.area(), Room::WestWing);
    // The west wing is 12 cells wide (96 px) and its door is rows 3–6, three cells above the hall's.
    EXPECT_EQ(state.player.position(), (Vec2i{-9 - 16 + 96, 60 - 24}));
    // The camera slid one screen west and no further: the wing sits against the right of the screen
    // with 64 px of void left of it, 24 px below the top as the door lined it up. It fits the screen,
    // so the camera holds there until the next transition, whatever the player does.
    EXPECT_EQ(state.camera.position(), (Vec2i{-64, -24}));
    run(state, config, {}, 30);
    EXPECT_EQ(state.camera.position(), (Vec2i{-64, -24}));
    entered = run(state, config, {.left = true}, 20);
    entered = run(state, config, {.right = true}, 20);
    EXPECT_EQ(state.player.position(), (Vec2i{71, 36}));
    EXPECT_EQ(state.camera.position(), (Vec2i{-64, -24}));
    // Back through the door, and across the hall to the east wing.
    entered = run(state, config, {.right = true}, 360);
    EXPECT_EQ(entered.last.area, Room::EastWing);
    EXPECT_EQ(state.camera.area(), Room::EastWing);
    EXPECT_EQ(state.camera.position(), (Vec2i{0, -24}));   // against the left of the screen this time
}

TEST(Dungeon, TheEntrancesDoorFadesBackUpToTheLakeWithoutGoingStraightDownAgain) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = inTheEntrance(config);
    // Standing on the door where the fade left the player starts nothing; stepping off and back does.
    Entered entered = run(state, config, {}, 2);
    EXPECT_FALSE(state.camera.transitioning());
    run(state, config, {.up = true}, 8);
    entered = run(state, config, {.down = true}, 8);
    EXPECT_TRUE(state.camera.transitioning());
    entered = run(state, config, {}, 16);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.layout, Layout::Overworld);
    EXPECT_EQ(entered.last.area, Area::Lake);
    EXPECT_EQ(state.camera.layout(), Layout::Overworld);
    EXPECT_EQ(state.player.position(), (Vec2i{240, 8}));   // centered on the dungeon door: box (243, 16)
    run(state, config, {}, 16);
    EXPECT_FALSE(state.camera.transitioning());
}

// ── The forest ───────────────────────────────────────────────────────────────────────────────
//
// In the glade's pixels the edge's top-left is (−160, 16), the thicket's (88, −80), the bower's
// (184, −80), the hollow's (240, 64), the brook's (400, 80) and the clearing's (32, 160). The player's
// box is its position plus (3, 8), 10×8, so its center is the position plus (8, 12).

// From the meadow down the south road into the field at (72, 4), right to (104, 4) — the camera
// following to x = 32 — and down through the forest path: the box's center reaches y = 128 after 112
// ticks, and the scroll carries the player 16 px into the edge, which sits 40 px right of the field's
// left edge and 128 px down.
zorlduh::GameState inTheEdge(const zorlduh::Config& config) {
    zorlduh::GameState state = start(config);
    run(state, config, {.down = true}, 60 + 32);
    run(state, config, {.right = true}, 32);
    run(state, config, {.down = true}, 112 + 32);
    return state;
}

// Down the edge to (64, 56), then east across its seam into the glade at (−8, 72).
zorlduh::GameState inTheGlade(const zorlduh::Config& config) {
    zorlduh::GameState state = inTheEdge(config);
    run(state, config, {.down = true}, 52);
    run(state, config, {.right = true}, 88);
    return state;
}

// The scroll slides down only, so the edge lands where the field's camera was across: 8 px left of its
// own place. The edge is exactly the view's size, so the camera follows it on both axes — but only with
// the player's steps toward that place.
TEST(Forest, TheFieldsForestPathScrollsDownIntoTheEdgeWhichComesIntoPlaceAsThePlayerWalks) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = start(config);
    run(state, config, {.down = true}, 60 + 32);
    run(state, config, {.right = true}, 32);
    EXPECT_EQ(state.camera.position(), (Vec2i{32, 0}));
    Entered entered = run(state, config, {.down = true}, 112);
    EXPECT_TRUE(state.camera.transitioning());
    EXPECT_EQ(entered.count, 0);
    entered = run(state, config, {.down = true}, 32);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.layout, Layout::SeamlessForest);
    EXPECT_EQ(entered.last.area, Woods::Edge);
    EXPECT_FALSE(state.camera.transitioning());
    EXPECT_EQ(state.player.position(), (Vec2i{64, 4}));   // (104 − 40, 116 + 16 − 128)
    EXPECT_EQ(state.camera.position(), (Vec2i{-8, 0}));   // 32 − 40

    run(state, config, {}, 30);
    EXPECT_EQ(state.camera.position(), (Vec2i{-8, 0}));
    run(state, config, {.left = true}, 10);
    EXPECT_EQ(state.camera.position(), (Vec2i{-8, 0}));
    run(state, config, {.right = true}, 5);
    EXPECT_EQ(state.camera.position(), (Vec2i{-3, 0}));
    run(state, config, {.right = true}, 8);
    EXPECT_EQ(state.camera.position(), (Vec2i{0, 0}));     // in place, and it stays there
    run(state, config, {.left = true}, 20);
    EXPECT_EQ(state.camera.position(), (Vec2i{0, 0}));

    // Back up the path: the field scrolls in above, sliding up only.
    run(state, config, {.right = true}, 20);
    EXPECT_EQ(state.player.position(), (Vec2i{67, 4}));
    entered = run(state, config, {.up = true}, 17 + 32);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.layout, Layout::Overworld);
    EXPECT_EQ(entered.last.area, Area::Field);
    EXPECT_EQ(state.player.position(), (Vec2i{107, 99}));   // (67 + 40, −13 − 16 + 128)
    EXPECT_EQ(state.camera.position(), (Vec2i{40, 0}));
}

// The glade sits 16 px higher than level with the edge: crossing moves the player and the camera by
// (−160, 16) and nothing on screen. The glade's law then wants the camera 4 px lower, and it goes there
// only as the player walks down.
TEST(Forest, CrossingIntoTheBiggerGladeMovesNothingOnScreenAndTheCameraFollowsOnlyWithThePlayer) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = inTheEdge(config);
    run(state, config, {.down = true}, 52);
    EXPECT_EQ(state.player.position(), (Vec2i{64, 56}));
    Entered entered = run(state, config, {.right = true}, 87);
    EXPECT_EQ(entered.count, 0);
    EXPECT_EQ(state.player.position(), (Vec2i{151, 56}));
    EXPECT_EQ(state.camera.position(), (Vec2i{79, 0}));
    std::vector<WorldKit::Shown> shown = state.camera.shown(config.world);
    ASSERT_EQ(shown.size(), 2u);
    EXPECT_EQ(shown[1].area, Woods::Glade);
    EXPECT_EQ(shown[1].scroll, (retropp::LayerScroll{.x = 79 - 160, .y = -16 + 16}));

    entered = run(state, config, {.right = true}, 1);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.layout, Layout::SeamlessForest);
    EXPECT_EQ(entered.last.area, Woods::Glade);
    EXPECT_FALSE(state.camera.transitioning());
    EXPECT_EQ(state.player.position(), (Vec2i{-8, 72}));
    EXPECT_EQ(state.camera.position(), (Vec2i{-80, 16}));
    shown = state.camera.shown(config.world);
    ASSERT_EQ(shown.size(), 2u);
    EXPECT_EQ(shown[0].scroll, (retropp::LayerScroll{.x = -80, .y = 0}));    // one step on from (−81, 0)
    EXPECT_EQ(shown[1].area, Woods::Edge);
    EXPECT_EQ(shown[1].scroll, (retropp::LayerScroll{.x = 80, .y = -16}));   // one step on from (79, −16)

    run(state, config, {}, 20);
    EXPECT_EQ(state.camera.position(), (Vec2i{-80, 16}));
    run(state, config, {.up = true}, 4);
    EXPECT_EQ(state.camera.position(), (Vec2i{-80, 16}));
    run(state, config, {.down = true}, 10);
    EXPECT_EQ(state.camera.position(), (Vec2i{-80, 26}));
    run(state, config, {.right = true}, 20);
    EXPECT_EQ(state.camera.position(), (Vec2i{-60, 26}));

    // Back across: the edge is current again, the camera 10 px below its rows where the glade had it.
    entered = run(state, config, {.left = true}, 21);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.area, Woods::Edge);
    EXPECT_EQ(state.player.position(), (Vec2i{151, 62}));
    EXPECT_EQ(state.camera.position(), (Vec2i{79, 10}));
}

// Below the thicket in the glade, the view reaches past the thicket's east wall, so the bower — two
// seams away — is drawn where it hangs. The thicket is smaller than the view, but its seams reach into
// the glade below and the bower beside it, so it follows on both axes; the bower, with one seam west,
// follows across and holds still up and down.
TEST(Forest, TheBowerIsSeenThroughTheThicketWhichFollowsOnBothAxesWhileTheBowerHoldsStillUpAndDown) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = inTheGlade(config);
    ASSERT_EQ(state.camera.area(), Woods::Glade);
    ASSERT_EQ(state.player.position(), (Vec2i{-8, 72}));
    run(state, config, {.right = true}, 128);
    EXPECT_EQ(state.camera.position(), (Vec2i{48, 16}));
    Entered entered = run(state, config, {.up = true}, 84);
    EXPECT_EQ(entered.count, 0);
    EXPECT_EQ(state.player.position(), (Vec2i{120, -12}));
    EXPECT_EQ(state.camera.position(), (Vec2i{48, -64}));
    std::vector<WorldKit::Shown> shown = state.camera.shown(config.world);
    ASSERT_EQ(shown.size(), 3u);
    EXPECT_EQ(shown[1].area, Woods::Thicket);
    EXPECT_EQ(shown[1].scroll, (retropp::LayerScroll{.x = 48 - 88, .y = -80 + 80}));
    EXPECT_EQ(shown[2].area, Woods::Bower);
    EXPECT_EQ(shown[2].scroll, (retropp::LayerScroll{.x = 48 - 184, .y = -80 + 80}));

    entered = run(state, config, {.up = true}, 1);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.area, Woods::Thicket);
    EXPECT_EQ(state.player.position(), (Vec2i{32, 67}));
    EXPECT_EQ(state.camera.position(), (Vec2i{-40, 15}));
    shown = state.camera.shown(config.world);
    ASSERT_EQ(shown.size(), 3u);
    EXPECT_EQ(shown[1].area, Woods::Glade);
    EXPECT_EQ(shown[1].scroll, (retropp::LayerScroll{.x = 48, .y = -81}));
    EXPECT_EQ(shown[2].area, Woods::Bower);
    EXPECT_EQ(shown[2].scroll, (retropp::LayerScroll{.x = -136, .y = -1}));

    run(state, config, {.up = true}, 14);   // the camera follows up, though the thicket is short
    EXPECT_EQ(state.camera.position(), (Vec2i{-40, 1}));
    entered = run(state, config, {.right = true}, 56);   // and across, with the player's steps
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.area, Woods::Bower);
    EXPECT_EQ(state.player.position(), (Vec2i{-8, 53}));
    EXPECT_EQ(state.camera.position(), (Vec2i{-80, 1}));   // (16 − 96, 1)

    run(state, config, {.right = true}, 30);
    EXPECT_EQ(state.camera.position(), (Vec2i{-50, 1}));
    run(state, config, {.down = true}, 10);
    EXPECT_EQ(state.player.position(), (Vec2i{22, 56}));
    EXPECT_EQ(state.camera.position(), (Vec2i{-50, 1}));
    run(state, config, {.up = true}, 60);
    EXPECT_EQ(state.player.position(), (Vec2i{22, 0}));
    EXPECT_EQ(state.camera.position(), (Vec2i{-50, 1}));

    // Back into the thicket, and down into the glade.
    entered = run(state, config, {.left = true}, 31);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.area, Woods::Thicket);
    EXPECT_EQ(state.player.position(), (Vec2i{87, 0}));
    EXPECT_EQ(state.camera.position(), (Vec2i{15, 1}));
    run(state, config, {.left = true}, 20);
    EXPECT_EQ(state.camera.position(), (Vec2i{0, 1}));
    entered = run(state, config, {.down = true}, 68);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.area, Woods::Glade);
    EXPECT_EQ(state.player.position(), (Vec2i{155, -12}));
    EXPECT_EQ(state.camera.position(), (Vec2i{88, -64}));
}

// The hollow and the brook are half the view's height: through the chain the camera holds still up
// and down where the crossing from the glade left it, and follows across. The brook sits two cells
// lower than level, so the hollow's top row runs into the brook's void, and the crossing in the rows
// that meet moves everything by (−160, −16).
TEST(Forest, TheChainHoldsStillUpAndDownAndTheBrookSitsTwoCellsLower) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = inTheGlade(config);
    Entered entered = run(state, config, {.right = true}, 240);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.area, Woods::Hollow);
    EXPECT_EQ(state.player.position(), (Vec2i{-8, 8}));
    EXPECT_EQ(state.camera.position(), (Vec2i{-80, -48}));   // (160 − 240, 16 − 64)
    std::vector<WorldKit::Shown> shown = state.camera.shown(config.world);
    ASSERT_EQ(shown.size(), 2u);
    EXPECT_EQ(shown[1].area, Woods::Glade);

    run(state, config, {.right = true}, 40);
    EXPECT_EQ(state.camera.position(), (Vec2i{-40, -48}));
    run(state, config, {.up = true}, 10);
    EXPECT_EQ(state.player.position(), (Vec2i{32, 0}));
    EXPECT_EQ(state.camera.position(), (Vec2i{-40, -48}));
    entered = run(state, config, {.right = true}, 200);   // along the top row: the brook's void stops it
    EXPECT_EQ(entered.count, 0);
    EXPECT_EQ(state.player.position(), (Vec2i{147, 0}));
    EXPECT_EQ(state.camera.position(), (Vec2i{75, -48}));
    EXPECT_TRUE(state.camera.solids(config.world).solidBox({.x = 160, .y = 8, .width = 1, .height = 8}));
    EXPECT_FALSE(state.camera.solids(config.world).solidBox({.x = 160, .y = 24, .width = 1, .height = 8}));
    run(state, config, {.down = true}, 24);
    EXPECT_EQ(state.camera.position(), (Vec2i{75, -48}));

    entered = run(state, config, {.right = true}, 5);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.area, Woods::Brook);
    EXPECT_EQ(state.player.position(), (Vec2i{-8, 8}));       // (152 − 160, 24 − 16)
    EXPECT_EQ(state.camera.position(), (Vec2i{-80, -64}));    // (80 − 160, −48 − 16)
    shown = state.camera.shown(config.world);
    ASSERT_EQ(shown.size(), 2u);
    EXPECT_EQ(shown[1].area, Woods::Hollow);
    EXPECT_EQ(shown[1].scroll, (retropp::LayerScroll{.x = 80, .y = -64}));   // the hollow's own, one step on
    run(state, config, {.right = true}, 20);
    EXPECT_EQ(state.camera.position(), (Vec2i{-60, -64}));

    entered = run(state, config, {.left = true}, 21);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.area, Woods::Hollow);
    EXPECT_EQ(state.player.position(), (Vec2i{151, 24}));
    EXPECT_EQ(state.camera.position(), (Vec2i{79, -48}));
    entered = run(state, config, {.left = true}, 160);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.area, Woods::Glade);
    EXPECT_EQ(state.player.position(), (Vec2i{231, 88}));
    EXPECT_EQ(state.camera.position(), (Vec2i{159, 16}));
}

// The clearing is the view's size, centered under the wider glade. Crossing near its left end, the
// glade's camera is 32 px left of the glade (its law reaches into the edge), 64 px left of the
// clearing's place: void shows left of the clearing, and the edge — two seams away — is in view above
// it. The camera comes into the clearing's place only as the player walks right, and stops there.
TEST(Forest, TheClearingIsEnteredOffCenterAndComesIntoPlaceOnlyAsThePlayerWalks) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = inTheGlade(config);
    run(state, config, {.right = true}, 48);
    EXPECT_EQ(state.camera.position(), (Vec2i{-32, 16}));
    Entered entered = run(state, config, {.down = true}, 76);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.area, Woods::Clearing);
    EXPECT_EQ(state.player.position(), (Vec2i{8, -12}));
    EXPECT_EQ(state.camera.position(), (Vec2i{-64, -68}));   // (−32 − 32, 92 − 160)
    const std::vector<WorldKit::Shown> shown = state.camera.shown(config.world);
    ASSERT_EQ(shown.size(), 3u);
    EXPECT_EQ(shown[1].area, Woods::Glade);
    EXPECT_EQ(shown[1].scroll, (retropp::LayerScroll{.x = -32, .y = 76}));
    EXPECT_EQ(shown[2].area, Woods::Edge);

    run(state, config, {}, 20);
    EXPECT_EQ(state.camera.position(), (Vec2i{-64, -68}));
    run(state, config, {.left = true}, 3);
    EXPECT_EQ(state.camera.position(), (Vec2i{-64, -68}));
    run(state, config, {.right = true}, 30);
    EXPECT_EQ(state.camera.position(), (Vec2i{-34, -68}));
    run(state, config, {.down = true}, 20);
    EXPECT_EQ(state.camera.position(), (Vec2i{-34, -48}));
    run(state, config, {.right = true}, 50);
    EXPECT_EQ(state.camera.position(), (Vec2i{0, -48}));     // in place across, and no further
    run(state, config, {.left = true}, 10);
    EXPECT_EQ(state.camera.position(), (Vec2i{0, -48}));

    entered = run(state, config, {.up = true}, 21);
    EXPECT_EQ(entered.count, 1);
    EXPECT_EQ(entered.last.area, Woods::Glade);
    EXPECT_EQ(state.player.position(), (Vec2i{107, 147}));
    EXPECT_EQ(state.camera.position(), (Vec2i{32, 95}));
}

}  // namespace
