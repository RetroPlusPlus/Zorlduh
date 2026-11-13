// A message's box opening and closing in the game, over the real maps: its length at the Game Boy Color
// cadence, Item finishing the opening and the closing, and the box drawn growing from its middle with
// nothing typed in it until it is open. No renderer is needed: the maps are decoded from the bytes
// baked into the test binary, and the sheets, font and borders are handles the test makes up.

#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <variant>

#include "retropp/atlas_manifest.h"
#include "retropp/draw_state.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/input_actions.h"
#include "retropp/palette.h"
#include "retropp/timing.h"

#include "TextKit/font.h"
#include "TopDownMovement/walker.h"
#include "WindowKit/animation.h"
#include "WorldKit/camera.h"

#include "assets/assets.h"
#include "game/areas.h"
#include "game/signs.h"
#include "game/sim.h"
#include "render/dialog.h"
#include "render/layers.h"
#include "render/layout.h"

namespace {

using retropp::Dir;
using retropp::Vec2i;
using WindowKit::Phase;

constexpr retropp::TimingProfile kHz60{.tickPeriodNs = retropp::TickPeriodNs::Hz60};

retropp::AtlasManifest tileset(retropp::AtlasId id, int width, int height, int count = 0) {
    return retropp::AtlasManifest{
        .atlasId = id,
        .slots = retropp::sliceLayout({.width = width, .height = height}, {.width = 8, .height = 8},
                                      retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown, count),
        .kind = retropp::ContentKind::Tileset,
    };
}

// The real maps, with the sheets, palettes, font and borders as handles the test makes up. The config's
// world points at the maps, so the assets outlive it.
zorlduh::Assets headlessAssets() {
    zorlduh::Assets assets;
    assets.terrain = tileset(retropp::AtlasId{7}, 56, 8);
    assets.forest = tileset(retropp::AtlasId{9}, 32, 8);
    assets.overlay = tileset(retropp::AtlasId{11}, 32, 8);
    assets.font = TextKit::Font{.sheet = tileset(retropp::AtlasId{13}, 128, 24, 43),
                                .glyphs = zorlduh::kGlyphOrder,
                                .palette = retropp::PaletteId{2}};
    assets.borders = zorlduh::Borders{
        .wood = {.sheet = tileset(retropp::AtlasId{15}, 24, 32, 10), .palette = retropp::PaletteId{3}},
        .stone = {.sheet = tileset(retropp::AtlasId{17}, 24, 32, 10), .palette = retropp::PaletteId{5}},
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

// The game's config over the made-up assets. The message's animation resolves its time at the Game Boy
// Color cadence, the engine's own until a game makes another active.
zorlduh::Config headlessConfig(const zorlduh::Assets& assets) {
    zorlduh::Config config = zorlduh::gameConfig(assets, zorlduh::kView, zorlduh::kAreaDepths, zorlduh::kDialogBox);
    config.player.timing = kHz60;
    config.messageAnimation.timing = retropp::TimingProfile::GameBoyColor;
    return config;
}

// Below the meadow's sign, facing it, with Item pressed: its message is starting.
zorlduh::GameState readingTheSign(const zorlduh::Config& config) {
    zorlduh::GameState state{.player = TopDownMovement::Walker{{.x = 44, .y = 32}, Dir::Up}};
    state.camera.place(config.world, state.player.collision(config.player).bounds());
    zorlduh::tick(state, zorlduh::Input{.item = true}, config);
    return state;
}

void wait(zorlduh::GameState& state, const zorlduh::Config& config, int ticks) {
    for (int i = 0; i < ticks; ++i) {
        zorlduh::tick(state, zorlduh::Input{}, config);
    }
}

void press(zorlduh::GameState& state, const zorlduh::Config& config) {
    zorlduh::tick(state, zorlduh::Input{.item = true}, config);
}

TEST(SignsAnimation, AMessagesBoxOpensOverEighteenTicksAndTypesOnceOpen) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    EXPECT_EQ(config.messageAnimation.duration, zorlduh::kMessageAnimationTime);
    zorlduh::GameState state = readingTheSign(config);
    ASSERT_TRUE(state.dialog.showing());
    EXPECT_EQ(state.dialog.animation.phase(), Phase::Opening);
    EXPECT_EQ(state.dialog.animation.length(), 18u);
    wait(state, config, 17);
    EXPECT_EQ(state.dialog.animation.phase(), Phase::Opening);
    EXPECT_EQ(state.dialog.shown(), 0u);
    wait(state, config, 1);
    EXPECT_EQ(state.dialog.animation.phase(), Phase::Open);
    wait(state, config, 4);
    EXPECT_GT(state.dialog.shown(), 0u);
}

TEST(SignsAnimation, ItemWhileTheBoxOpensOpensItAtOnceWithNothingTyped) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = readingTheSign(config);
    wait(state, config, 3);
    press(state, config);
    EXPECT_EQ(state.dialog.animation.phase(), Phase::Open);
    EXPECT_EQ(state.dialog.shown(), 0u);
    EXPECT_FALSE(state.dialog.pageDone());
}

TEST(SignsAnimation, TheLastPagesItemClosesTheBoxOverEighteenTicks) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = readingTheSign(config);
    press(state, config);  // opens the box at once
    while (!state.dialog.lastPage()) {
        press(state, config);  // shows the page whole
        press(state, config);  // turns it
    }
    press(state, config);  // shows the last page whole
    press(state, config);  // starts the close
    EXPECT_EQ(state.dialog.animation.phase(), Phase::Closing);
    wait(state, config, 17);
    EXPECT_TRUE(state.dialog.showing());
    wait(state, config, 1);
    EXPECT_FALSE(state.dialog.showing());
    EXPECT_EQ(state.dialog.animation.phase(), Phase::Closed);
}

TEST(SignsAnimation, TheMessageBoxGrowsFromItsMiddleWithNoTextUntilItIsOpen) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = readingTheSign(config);
    wait(state, config, 4);
    const auto look = [&](bool open) {
        const retropp::DrawLayer layer = zorlduh::Dialog(state, assets, 21);
        const std::span<const retropp::Sprite> sprites = std::get<retropp::SpriteContent>(layer.content).sprites;
        const retropp::Sprite* corner = nullptr;
        int glyphs = 0;
        for (const retropp::Sprite& sprite : sprites) {
            const std::string_view key(sprite.key);
            if (key.starts_with("dialog:box:top-left:")) {
                corner = &sprite;
            } else if (key.starts_with("dialog:0:")) {
                ++glyphs;
            }
        }
        ASSERT_NE(corner, nullptr);
        if (open) {
            EXPECT_EQ(corner->x, zorlduh::kDialogBox.x);
            EXPECT_EQ(corner->y, zorlduh::kDialogBox.y);
            EXPECT_GT(glyphs, 0);
        } else {
            EXPECT_GT(corner->x, zorlduh::kDialogBox.x);
            EXPECT_GT(corner->y, zorlduh::kDialogBox.y);
            EXPECT_LT(corner->x, zorlduh::kDialogBox.x + zorlduh::kDialogBox.width / 2);
            EXPECT_LT(corner->y, zorlduh::kDialogBox.y + zorlduh::kDialogBox.height / 2);
            EXPECT_EQ(glyphs, 0);
        }
    };
    look(false);
    wait(state, config, 14 + 4);  // open, then a few characters typed
    look(true);
}

// The shrine's six signs, on their grid: each solid on the map, read from below it, and its message box
// halfway open in its own style, with nothing typed in it yet.
TEST(SignsAnimation, TheShrinesSixSignsEachOpenTheirBoxInTheirOwnStyle) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    const struct {
        Vec2i cell;
        zorlduh::MessageStyle style;
        std::string_view first;  // the message's first word
    } expected[] = {
        {{3, 2}, zorlduh::MessageStyle::Grow, "Grow."},   {{6, 2}, zorlduh::MessageStyle::Clip, "Clip."},
        {{9, 2}, zorlduh::MessageStyle::Squash, "Squash."}, {{3, 6}, zorlduh::MessageStyle::Fade, "Fade."},
        {{6, 6}, zorlduh::MessageStyle::Slide, "Slide."}, {{9, 6}, zorlduh::MessageStyle::Pop, "Pop."},
    };
    const zorlduh::AreaMaps& shrine = assets.areas.overworld.shrine;
    for (const auto& sign : expected) {
        SCOPED_TRACE(std::string(sign.first));
        EXPECT_EQ(shrine.tiles.at(sign.cell.x, sign.cell.y), static_cast<std::uint16_t>(zorlduh::WorldCell::Sign));
        EXPECT_EQ(shrine.collision.at(sign.cell.x, sign.cell.y),
                  static_cast<std::uint16_t>(zorlduh::CollisionCell::Solid));

        // Below the sign, its collision box's middle under the sign's cell, facing up.
        zorlduh::GameState state{
            .player = TopDownMovement::Walker{{.x = sign.cell.x * 8 - 4, .y = sign.cell.y * 8}, Dir::Up},
            .camera = WorldKit::Camera{zorlduh::Layout::Overworld, zorlduh::Area::Shrine}};
        state.camera.place(config.world, state.player.collision(config.player).bounds());
        press(state, config);
        ASSERT_TRUE(state.dialog.showing());
        EXPECT_EQ(state.readingStyle, sign.style);
        EXPECT_EQ(state.reading, zorlduh::Material::Wood);
        EXPECT_TRUE(state.dialog.text().starts_with(sign.first)) << state.dialog.text();

        wait(state, config, 9);  // halfway open
        const retropp::DrawLayer layer = zorlduh::Dialog(state, assets, 21);
        const std::span<const retropp::Sprite> sprites = std::get<retropp::SpriteContent>(layer.content).sprites;
        const retropp::Sprite* corner = nullptr;
        for (const retropp::Sprite& sprite : sprites) {
            const std::string_view key(sprite.key);
            EXPECT_TRUE(key.starts_with("dialog:box:")) << key;  // the box alone: nothing typed while it opens
            if (key.starts_with("dialog:box:top-left:")) {
                corner = &sprite;
            }
        }
        const bool whole =
            corner != nullptr && corner->x == zorlduh::kDialogBox.x && corner->y == zorlduh::kDialogBox.y;
        switch (sign.style) {
        case zorlduh::MessageStyle::Grow:
            ASSERT_NE(corner, nullptr);
            EXPECT_GT(corner->x, zorlduh::kDialogBox.x);
            EXPECT_GT(corner->y, zorlduh::kDialogBox.y);
            break;
        case zorlduh::MessageStyle::Clip:
            EXPECT_TRUE(whole);
            ASSERT_FALSE(layer.regions.empty());
            EXPECT_EQ(std::string_view(layer.regions.back().key), "clip");
            break;
        case zorlduh::MessageStyle::Squash:
            EXPECT_TRUE(whole);
            EXPECT_FALSE(layer.transform.isIdentity());
            break;
        case zorlduh::MessageStyle::Fade:
            EXPECT_TRUE(whole);
            EXPECT_GT(layer.alpha, 0.0f);
            EXPECT_LT(layer.alpha, 1.0f);
            break;
        case zorlduh::MessageStyle::Slide:
            EXPECT_TRUE(whole);
            EXPECT_LT(layer.scroll.y, 0);
            EXPECT_GT(layer.scroll.y, -zorlduh::kDialogBox.height);
            break;
        case zorlduh::MessageStyle::Pop:
            EXPECT_TRUE(whole);
            EXPECT_TRUE(layer.regions.empty());
            EXPECT_TRUE(layer.transform.isIdentity());
            EXPECT_FLOAT_EQ(layer.alpha, 1.0f);
            EXPECT_EQ(layer.scroll, (retropp::LayerScroll{}));
            break;
        }
    }
}

}  // namespace
