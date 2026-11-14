// The current area's name in the HUD strip: every area's name, the name typed out as the game ticks,
// right-aligned across the strip, typed again from the start on entering another area, and a name with
// a space in it. The world is the game's over the real maps; the sheets and palettes are handles the
// test makes up, and the glyph sheet a manifest carved as the game loads it. No renderer is needed.

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "retropp/atlas_manifest.h"
#include "retropp/draw_state.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/palette.h"
#include "retropp/timing.h"

#include "TextKit/font.h"
#include "TopDownMovement/walker.h"
#include "WorldKit/camera.h"

#include "assets/assets.h"
#include "game/areas.h"
#include "game/player.h"
#include "game/sim.h"
#include "render/area_name.h"
#include "render/layout.h"

namespace {

using zorlduh::Area;
using zorlduh::Layout;
using zorlduh::Room;
using zorlduh::Woods;

constexpr retropp::TimingProfile kHz60{.tickPeriodNs = retropp::TickPeriodNs::Hz60};
constexpr zorlduh::AreaDepths kDepths{.ground = 0, .underfoot = 5, .actors = 10, .overhead = 15};

retropp::AtlasManifest tileset(retropp::AtlasId id, int width, int height, int count = 0) {
    return retropp::AtlasManifest{
        .atlasId = id,
        .slots = retropp::sliceLayout({.width = width, .height = height}, {.width = 8, .height = 8},
                                      retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown, count),
        .kind = retropp::ContentKind::Tileset,
    };
}

// The real maps, the sheets and palettes as made-up handles, and the font over a glyph manifest carved
// as the game carves the glyph sheet.
zorlduh::Assets headlessAssets() {
    zorlduh::Assets assets;
    assets.terrain = tileset(retropp::AtlasId{7}, 40, 8);
    assets.forest = tileset(retropp::AtlasId{9}, 32, 8);
    assets.overlay = tileset(retropp::AtlasId{11}, 32, 8);
    assets.glyphs = tileset(retropp::AtlasId{13}, 128, 24, static_cast<int>(zorlduh::kGlyphOrder.size()));
    assets.palettes = zorlduh::Palettes{
        .floor = retropp::PaletteId{4},
        .wall = retropp::PaletteId{8},
        .water = retropp::PaletteId{12},
        .grass = retropp::PaletteId{16},
        .leaf = retropp::PaletteId{20},
        .canopy = retropp::PaletteId{24},
        .text = retropp::PaletteId{28},
        .shadow = retropp::PaletteId{32},
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
    assets.font = TextKit::Font{
        .sheet = assets.glyphs,
        .glyphs = zorlduh::kGlyphOrder,
        .palette = assets.palettes.text,
        .shadow = assets.palettes.shadow,
    };
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

// Ticks `count` times holding `held`; returns whether the last tick entered an area.
bool run(zorlduh::GameState& state, const zorlduh::Config& config, TopDownMovement::Directions held, int count) {
    bool entered = false;
    for (int i = 0; i < count; ++i) {
        zorlduh::tick(state, zorlduh::Input{.move = {.held = held, .pressed = held}}, config);
        entered = false;
        for (const zorlduh::GameEvent& event : state.events) {
            entered = entered || std::holds_alternative<zorlduh::AreaChanged>(event);
        }
    }
    return entered;
}

std::vector<std::string> keys(const std::vector<retropp::Sprite>& sprites) {
    std::vector<std::string> out;
    for (const retropp::Sprite& sprite : sprites) {
        out.emplace_back(std::string_view(sprite.key));
    }
    return out;
}

// The strip's right edge less its 8 px margin.
constexpr int kRight = zorlduh::kPlayAreaWidth - 8;

TEST(AreaName, EveryAreaHasItsName) {
    const std::vector<std::string_view> overworld{"Meadow", "Lake", "Field", "Shrine", "Cave"};
    for (int area = Area::Meadow; area <= Area::Cave; ++area) {
        EXPECT_EQ(zorlduh::areaName(Layout::Overworld, area), overworld[static_cast<std::size_t>(area)]);
    }
    const std::vector<std::string_view> dungeon{"Entrance", "Hall", "West Wing", "East Wing", "Treasury"};
    for (int room = Room::Entrance; room <= Room::Treasury; ++room) {
        EXPECT_EQ(zorlduh::areaName(Layout::DungeonOne, room), dungeon[static_cast<std::size_t>(room)]);
    }
    const std::vector<std::string_view> woods{"Edge", "Glade", "Thicket", "Bower", "Hollow", "Brook", "Clearing"};
    for (int room = Woods::Edge; room <= Woods::Clearing; ++room) {
        EXPECT_EQ(zorlduh::areaName(Layout::SeamlessForest, room), woods[static_cast<std::size_t>(room)]);
    }
}

// "Meadow" is 6 characters, 48 px: right-aligned to x = 152, it starts at 104.
TEST(AreaName, TheStartingAreasNameTypesOutRightAlignedAcrossTheStrip) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = start(config);
    EXPECT_EQ(state.nameReveal.shown(), 0u);
    EXPECT_TRUE(zorlduh::AreaName(state, assets).empty());

    run(state, config, {}, 2);
    EXPECT_EQ(state.nameReveal.shown(), 1u);
    const std::vector<retropp::Sprite> sprites = zorlduh::AreaName(state, assets);
    EXPECT_EQ(keys(sprites), (std::vector<std::string>{"area-name:0:shadow", "area-name:0"}));
    EXPECT_EQ(sprites[1].x, kRight - 48);
    EXPECT_EQ(sprites[1].y, 4);
    EXPECT_EQ(sprites[1].tile, assets.glyphs[zorlduh::kGlyphOrder.find('M')].tile);
    EXPECT_EQ(sprites[1].palette, assets.palettes.text);
    EXPECT_EQ(sprites[0].palette, assets.palettes.shadow);
}

// Walking out through the meadow's east door scrolls into the lake on tick 112 (as the travel tests
// walk it): that tick's reveal starts over, and the next tick shows "Lake"'s first glyph, 32 px left
// of the strip's right edge.
TEST(AreaName, EnteringAnAreaTypesItsNameFromTheStart) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = start(config);
    EXPECT_FALSE(run(state, config, {.right = true}, 111));
    EXPECT_TRUE(state.nameReveal.done(6));

    EXPECT_TRUE(run(state, config, {}, 1));
    ASSERT_EQ(state.camera.area(), Area::Lake);
    EXPECT_EQ(state.nameReveal.shown(), 0u);
    EXPECT_TRUE(zorlduh::AreaName(state, assets).empty());

    run(state, config, {}, 1);
    const std::vector<retropp::Sprite> sprites = zorlduh::AreaName(state, assets);
    EXPECT_EQ(keys(sprites), (std::vector<std::string>{"area-name:0:shadow", "area-name:0"}));
    EXPECT_EQ(sprites[1].x, kRight - 32);
    EXPECT_EQ(sprites[1].tile, assets.glyphs[zorlduh::kGlyphOrder.find('L')].tile);
}

// "West Wing" is 9 characters; its space draws nothing, so 8 glyphs and 8 shadows.
TEST(AreaName, ANameWithASpaceDrawsEveryLetterAndNothingForTheSpace) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state{.player = TopDownMovement::Walker{{.x = 40, .y = 24}}};
    state.camera = WorldKit::Camera{Layout::DungeonOne, Room::WestWing};
    state.camera.place(config.world, state.player.collision(config.player).bounds());

    run(state, config, {}, 2 * 9 - 1);
    EXPECT_FALSE(state.nameReveal.done(9));
    run(state, config, {}, 1);
    EXPECT_TRUE(state.nameReveal.done(9));
    const std::vector<retropp::Sprite> sprites = zorlduh::AreaName(state, assets);
    EXPECT_EQ(sprites.size(), 16u);
    EXPECT_EQ(keys(sprites).back(), "area-name:8");
    for (const std::string& key : keys(sprites)) {
        EXPECT_NE(key.rfind("area-name:4", 0), 0u) << key;   // the space
    }
}

}  // namespace
