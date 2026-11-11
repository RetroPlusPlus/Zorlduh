// The game's world: the two layouts over the real maps, every opening's cells on the map it names,
// every way out having a way back, the three art catalogs, and the player going from area to area
// through the tick: the scroll east into the lake, the fade down into the cave, the camera following
// across the lake, input ignored while a transition runs, and the dungeon — down from the lake, up
// into the hall, across the seam into the treasury and back, the wings, and out. No renderer is
// needed: the maps are decoded from the bytes baked into the test binary and the art catalogs name
// handles the test makes up.

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

constexpr retropp::TimingProfile kHz60{.tickPeriodNs = retropp::TickPeriodNs::Hz60};

// The real maps, with the sheets and palettes as handles the test makes up.
zorlduh::Assets headlessAssets() {
    zorlduh::Assets assets;
    assets.terrain = retropp::AtlasManifest{
        .atlasId = retropp::AtlasId{7},
        .slots = retropp::sliceLayout({.width = 40, .height = 8}, {.width = 8, .height = 8},
                                      retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown),
        .kind = retropp::ContentKind::Tileset,
    };
    assets.palettes = zorlduh::Palettes{
        .floor = retropp::PaletteId{4},
        .wall = retropp::PaletteId{8},
        .water = retropp::PaletteId{12},
    };
    zorlduh::OverworldMaps& overworld = assets.areas.overworld;
    overworld.meadow.tiles = retropp::loadMapPng("assets/maps/overworld/meadow/tiles.png");
    overworld.meadow.collision = retropp::loadMapPng("assets/maps/overworld/meadow/collision.png");
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
    return assets;
}

zorlduh::Config headlessConfig(const zorlduh::Assets& assets) {
    zorlduh::Config config = zorlduh::gameConfig(assets, zorlduh::kView);
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
    const WorldKit::World world = zorlduh::gameWorld(assets, zorlduh::kView);
    ASSERT_EQ(world.layouts.size(), 2u);
    const WorldKit::Layout& overworld = world.layouts[Layout::Overworld];
    ASSERT_EQ(overworld.areas.size(), 5u);
    EXPECT_EQ(overworld.areas[Area::Meadow].tiles, &assets.areas.overworld.meadow.tiles);
    EXPECT_EQ(overworld.areas[Area::Meadow].collision, &assets.areas.overworld.meadow.collision);
    EXPECT_EQ(overworld.areas[Area::Lake].tiles, &assets.areas.overworld.lake.tiles);
    EXPECT_EQ(overworld.areas[Area::Field].tiles, &assets.areas.overworld.field.tiles);
    EXPECT_EQ(overworld.areas[Area::Shrine].tiles, &assets.areas.overworld.shrine.tiles);
    EXPECT_EQ(overworld.areas[Area::Cave].collision, &assets.areas.overworld.cave.collision);
    const WorldKit::Layout& dungeon = world.layouts[Layout::DungeonOne];
    ASSERT_EQ(dungeon.areas.size(), 5u);
    EXPECT_EQ(dungeon.areas[Room::Entrance].tiles, &assets.areas.dungeonOne.entrance.tiles);
    EXPECT_EQ(dungeon.areas[Room::Hall].collision, &assets.areas.dungeonOne.hall.collision);
    EXPECT_EQ(dungeon.areas[Room::WestWing].tiles, &assets.areas.dungeonOne.westWing.tiles);
    EXPECT_EQ(dungeon.areas[Room::EastWing].tiles, &assets.areas.dungeonOne.eastWing.tiles);
    EXPECT_EQ(dungeon.areas[Room::Treasury].tiles, &assets.areas.dungeonOne.treasury.tiles);
    for (const WorldKit::Layout& layout : world.layouts) {
        EXPECT_EQ(layout.cellSize, kCellSize);
        EXPECT_EQ(layout.open.value, value(CollisionCell::Open));
        EXPECT_EQ(layout.view, zorlduh::kView);
    }
    EXPECT_EQ(zorlduh::kStartLayout, Layout::Overworld);
    EXPECT_EQ(zorlduh::kStartArea, Area::Meadow);
}

TEST(Areas, EveryWayOutHasAWayBack) {
    const zorlduh::Assets assets = headlessAssets();
    const WorldKit::World world = zorlduh::gameWorld(assets, zorlduh::kView);
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
    EXPECT_EQ(exits, 18);
}

// The doorway and stairs cells on each map are exactly where its openings say.
TEST(Areas, EveryOpeningIsOnItsMap) {
    const zorlduh::Assets assets = headlessAssets();
    const WorldKit::World world = zorlduh::gameWorld(assets, zorlduh::kView);
    for (std::size_t l = 0; l < world.layouts.size(); ++l) {
        for (std::size_t a = 0; a < world.layouts[l].areas.size(); ++a) {
            const WorldKit::Area& area = world.layouts[l].areas[a];
            const retropp::IndexGrid& tiles = *area.tiles;
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
    // Every value any map holds has an entry in all three.
    const WorldKit::World world = zorlduh::gameWorld(assets, zorlduh::kView);
    for (const WorldKit::Layout& layout : world.layouts) {
        for (const WorldKit::Area& area : layout.areas) {
            for (const std::uint16_t cell : area.tiles->values) {
                EXPECT_NE(entry(overworld, static_cast<WorldCell>(cell)).sheet, retropp::AtlasId{}) << cell;
                EXPECT_NE(entry(cave, static_cast<WorldCell>(cell)).sheet, retropp::AtlasId{}) << cell;
                EXPECT_NE(entry(dungeon, static_cast<WorldCell>(cell)).sheet, retropp::AtlasId{}) << cell;
            }
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

}  // namespace
