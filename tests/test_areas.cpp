// The game's world: the layout over the real maps, every opening's cells on the map it names, every
// way out having a way back, the two art catalogs, and the player going from area to area through the
// tick: the scroll east into the lake, the fade down into the cave, the camera following across the
// lake, and input ignored while a transition runs. No renderer is needed: the maps are decoded from
// the bytes baked into the test binary and the art catalogs name handles the test makes up.

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
    assets.areas.meadow.tiles = retropp::loadMapPng("assets/maps/meadow/tiles.png");
    assets.areas.meadow.collision = retropp::loadMapPng("assets/maps/meadow/collision.png");
    assets.areas.lake.tiles = retropp::loadMapPng("assets/maps/lake/tiles.png");
    assets.areas.lake.collision = retropp::loadMapPng("assets/maps/lake/collision.png");
    assets.areas.field.tiles = retropp::loadMapPng("assets/maps/field/tiles.png");
    assets.areas.field.collision = retropp::loadMapPng("assets/maps/field/collision.png");
    assets.areas.shrine.tiles = retropp::loadMapPng("assets/maps/shrine/tiles.png");
    assets.areas.shrine.collision = retropp::loadMapPng("assets/maps/shrine/collision.png");
    assets.areas.cave.tiles = retropp::loadMapPng("assets/maps/cave/tiles.png");
    assets.areas.cave.collision = retropp::loadMapPng("assets/maps/cave/collision.png");
    return assets;
}

zorlduh::Config headlessConfig(const zorlduh::Assets& assets) {
    zorlduh::Config config = zorlduh::gameConfig(assets, zorlduh::kView);
    config.player.timing = kHz60;
    return config;
}

zorlduh::GameState start(const zorlduh::Config& config) {
    zorlduh::GameState state{.player = TopDownMovement::Walker{zorlduh::kPlayerStart}};
    state.camera.place(config.layout, state.player.collision(config.player).bounds());
    return state;
}

TopDownMovement::DirectionalInput Hold(TopDownMovement::Directions held) {
    return TopDownMovement::DirectionalInput{.held = held, .pressed = held};
}

// Ticks `count` times holding `held`, returning how many AreaChanged events were seen and the last.
struct Entered {
    int count = 0;
    Area last{};
};

Entered run(zorlduh::GameState& state, const zorlduh::Config& config, TopDownMovement::Directions held, int count) {
    Entered entered;
    for (int i = 0; i < count; ++i) {
        zorlduh::tick(state, zorlduh::Input{.move = Hold(held)}, config);
        for (const zorlduh::GameEvent& event : state.events) {
            if (const auto* changed = std::get_if<zorlduh::AreaChanged>(&event)) {
                ++entered.count;
                entered.last = changed->area;
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

// ── The layout ───────────────────────────────────────────────────────────────────────────────

TEST(Areas, OneAreaPerNameInOrderOverItsOwnMaps) {
    const zorlduh::Assets assets = headlessAssets();
    const WorldKit::Layout layout = zorlduh::worldLayout(assets, zorlduh::kView);
    ASSERT_EQ(layout.areas.size(), 5u);
    EXPECT_EQ(layout.areas[Area::Meadow].tiles, &assets.areas.meadow.tiles);
    EXPECT_EQ(layout.areas[Area::Meadow].collision, &assets.areas.meadow.collision);
    EXPECT_EQ(layout.areas[Area::Lake].tiles, &assets.areas.lake.tiles);
    EXPECT_EQ(layout.areas[Area::Field].tiles, &assets.areas.field.tiles);
    EXPECT_EQ(layout.areas[Area::Shrine].tiles, &assets.areas.shrine.tiles);
    EXPECT_EQ(layout.areas[Area::Cave].collision, &assets.areas.cave.collision);
    EXPECT_EQ(layout.cellSize, kCellSize);
    EXPECT_EQ(layout.open.value, value(CollisionCell::Open));
    EXPECT_EQ(layout.view, zorlduh::kView);
    EXPECT_EQ(zorlduh::kStartArea, Area::Meadow);
}

TEST(Areas, EveryWayOutHasAWayBack) {
    const zorlduh::Assets assets = headlessAssets();
    const WorldKit::Layout layout = zorlduh::worldLayout(assets, zorlduh::kView);
    int exits = 0;
    for (std::size_t a = 0; a < layout.areas.size(); ++a) {
        for (const WorldKit::Opening& opening : layout.areas[a].openings) {
            ASSERT_NE(opening.leadsTo.area, WorldKit::kNoArea) << "area " << a << " '" << opening.name << "'";
            ++exits;
            const WorldKit::Area& next = layout.areas[static_cast<std::size_t>(opening.leadsTo.area)];
            bool back = false;
            for (const WorldKit::Opening& entrance : next.openings) {
                back = back || (entrance.name == opening.leadsTo.opening &&
                                entrance.leadsTo.area == static_cast<int>(a) &&
                                entrance.leadsTo.opening == opening.name &&
                                entrance.transition == opening.transition);
            }
            EXPECT_TRUE(back) << "area " << a << " '" << opening.name << "'";
        }
    }
    EXPECT_EQ(exits, 8);
}

// The doorway and stairs cells on each map are exactly where its openings say.
TEST(Areas, EveryOpeningIsOnItsMap) {
    const zorlduh::Assets assets = headlessAssets();
    const WorldKit::Layout layout = zorlduh::worldLayout(assets, zorlduh::kView);
    for (std::size_t a = 0; a < layout.areas.size(); ++a) {
        const WorldKit::Area& area = layout.areas[a];
        const retropp::IndexGrid& tiles = *area.tiles;
        std::set<std::pair<int, int>> openingCells;
        for (const WorldKit::Opening& opening : area.openings) {
            if (opening.side == WorldKit::Side::None) {
                for (int y = opening.at.y; y < opening.at.y + opening.at.height; ++y) {
                    for (int x = opening.at.x; x < opening.at.x + opening.at.width; ++x) {
                        EXPECT_EQ(tiles.at(x, y), value(WorldCell::Stairs)) << a << " " << opening.name;
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
                EXPECT_EQ(tiles.at(x, y), value(WorldCell::Doorway)) << a << " " << opening.name;
                openingCells.insert({x, y});
            }
        }
        for (int y = 0; y < tiles.height; ++y) {
            for (int x = 0; x < tiles.width; ++x) {
                const std::uint16_t cell = tiles.at(x, y);
                if (cell == value(WorldCell::Doorway) || cell == value(WorldCell::Stairs)) {
                    EXPECT_TRUE(openingCells.contains({x, y})) << a << " " << x << "," << y;
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
    // Every value any map holds has an entry in both.
    for (const zorlduh::AreaMaps* maps : {&assets.areas.meadow, &assets.areas.lake, &assets.areas.field,
                                          &assets.areas.shrine, &assets.areas.cave}) {
        for (const std::uint16_t cell : maps->tiles.values) {
            EXPECT_NE(entry(overworld, static_cast<WorldCell>(cell)).sheet, retropp::AtlasId{}) << cell;
            EXPECT_NE(entry(cave, static_cast<WorldCell>(cell)).sheet, retropp::AtlasId{}) << cell;
        }
    }
}

// ── The player through the tick ──────────────────────────────────────────────────────────────

TEST(Travel, StartsInTheMeadowWithTheCameraAtItsTopLeft) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    const zorlduh::GameState state = start(config);
    EXPECT_EQ(state.camera.area(), Area::Meadow);
    EXPECT_EQ(state.camera.position(), (Vec2i{0, 0}));
    EXPECT_EQ(state.camera.scroll(config.layout), (retropp::LayerScroll{.x = 0, .y = -16}));
    EXPECT_FALSE(config.player.collision.hits(state.camera.solids(config.layout), zorlduh::kPlayerStart));
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
    EXPECT_EQ(entered.last, Area::Lake);
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
    EXPECT_EQ(entered.last, Area::Cave);
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
    EXPECT_EQ(entered.last, Area::Meadow);
    EXPECT_EQ(state.player.position(), (Vec2i{96, 24}));   // centered on the cave mouth: box (99, 32)
}

}  // namespace
