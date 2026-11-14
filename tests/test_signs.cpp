// The things the player reads: the meadow's wooden sign and the lake's stone rock over the real maps —
// where they stand, which one the player faces from each side, reading one through the tick with the
// world held still while the message is up, the pages turned with Item to the close, and the message
// box drawn in the border of what is read. No renderer is needed: the maps are decoded from the bytes
// baked into the test binary, and the sheets, font and borders are handles the test makes up.

#include <gtest/gtest.h>

#include <span>
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
#include "WindowKit/border.h"

#include "assets/assets.h"
#include "game/areas.h"
#include "game/player.h"
#include "game/signs.h"
#include "game/sim.h"
#include "render/dialog.h"
#include "render/layers.h"
#include "render/layout.h"

namespace {

using retropp::Dir;
using retropp::IntRect;
using retropp::Vec2i;
using zorlduh::Area;
using zorlduh::Layout;
using zorlduh::Material;
using zorlduh::WorldCell;

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

zorlduh::Config headlessConfig(const zorlduh::Assets& assets) {
    zorlduh::Config config = zorlduh::gameConfig(assets, zorlduh::kView, zorlduh::kAreaDepths, zorlduh::kDialogBox,
                                                 zorlduh::kMenuBoxes);
    config.player.timing = kHz60;
    return config;
}

// The player at `at` facing `facing`, in the meadow.
zorlduh::GameState standing(const zorlduh::Config& config, Vec2i at, Dir facing) {
    zorlduh::GameState state{.player = TopDownMovement::Walker{at, facing}};
    state.camera.place(config.world, state.player.collision(config.player).bounds());
    return state;
}

// Ticks with nothing pressed until the message's box has opened.
void untilOpen(zorlduh::GameState& state, const zorlduh::Config& config) {
    for (int i = 0; i < 100 && state.dialog.animation.phase() != WindowKit::Phase::Open; ++i) {
        zorlduh::tick(state, zorlduh::Input{}, config);
    }
}

TopDownMovement::DirectionalInput Hold(TopDownMovement::Directions held) {
    return TopDownMovement::DirectionalInput{.held = held, .pressed = held};
}

// The player's 10×8 collision box when it stands at (x, y): 3 px in, 8 px down.
IntRect boxAt(int x, int y) {
    return IntRect{.x = x + 3, .y = y + 8, .width = 10, .height = 8};
}

// Below the meadow's sign, cell (6, 4) — pixels 48–55 × 32–39: the box's top edge against the sign's
// bottom, its middle under the sign.
constexpr Vec2i kBelowTheSign{.x = 44, .y = 32};

// ── Where they stand ─────────────────────────────────────────────────────────────────────────

TEST(Signs, TheMeadowHasAWoodenSignAndTheLakeAStoneRockEachSolidOnItsMaps) {
    const std::span<const zorlduh::Sign> all = zorlduh::signs();
    ASSERT_EQ(all.size(), 8u);  // these two and the shrine's six
    EXPECT_EQ(all[0].area, Area::Meadow);
    EXPECT_EQ(all[0].cell, (Vec2i{6, 4}));
    EXPECT_EQ(all[0].material, Material::Wood);
    EXPECT_EQ(all[1].area, Area::Lake);
    EXPECT_EQ(all[1].cell, (Vec2i{28, 2}));
    EXPECT_EQ(all[1].material, Material::Stone);

    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    const zorlduh::AreaMaps& meadow = assets.areas.overworld.meadow;
    const zorlduh::AreaMaps& lake = assets.areas.overworld.lake;
    EXPECT_EQ(meadow.tiles.at(6, 4), static_cast<std::uint16_t>(WorldCell::Sign));
    EXPECT_EQ(meadow.collision.at(6, 4), static_cast<std::uint16_t>(zorlduh::CollisionCell::Solid));
    EXPECT_EQ(lake.tiles.at(28, 2), static_cast<std::uint16_t>(WorldCell::Rock));
    EXPECT_EQ(lake.collision.at(28, 2), static_cast<std::uint16_t>(zorlduh::CollisionCell::Solid));
    // The player walking up into the sign stops against it.
    zorlduh::GameState state = standing(config, {.x = 44, .y = 48}, Dir::Up);
    for (int i = 0; i < 40; ++i) {
        zorlduh::tick(state, zorlduh::Input{.move = Hold({.up = true})}, config);
    }
    EXPECT_EQ(state.player.position(), kBelowTheSign);
}

// ── Which one the player faces ───────────────────────────────────────────────────────────────

TEST(Signs, TheSignFacedIsTheOneOnTheCellJustPastTheSideOfTheBoxItFaces) {
    const zorlduh::Sign* sign = &zorlduh::signs()[0];
    EXPECT_EQ(zorlduh::signFaced(Layout::Overworld, Area::Meadow, boxAt(44, 32), Dir::Up), sign);    // below
    EXPECT_EQ(zorlduh::signFaced(Layout::Overworld, Area::Meadow, boxAt(44, 16), Dir::Down), sign);  // above
    EXPECT_EQ(zorlduh::signFaced(Layout::Overworld, Area::Meadow, boxAt(35, 24), Dir::Right), sign); // left
    EXPECT_EQ(zorlduh::signFaced(Layout::Overworld, Area::Meadow, boxAt(53, 24), Dir::Left), sign);  // right
    EXPECT_EQ(zorlduh::signFaced(Layout::Overworld, Area::Meadow, boxAt(44, 32), Dir::Down), nullptr);
    EXPECT_EQ(zorlduh::signFaced(Layout::Overworld, Area::Lake, boxAt(44, 32), Dir::Up), nullptr);
    // Below the lake's rock, cell (28, 2).
    EXPECT_EQ(zorlduh::signFaced(Layout::Overworld, Area::Lake, boxAt(220, 16), Dir::Up), &zorlduh::signs()[1]);
}

// ── Reading one ──────────────────────────────────────────────────────────────────────────────

TEST(Signs, ItemFacingTheSignOpensItsMessageAndTheWorldHoldsStill) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = standing(config, kBelowTheSign, Dir::Up);
    zorlduh::tick(state, zorlduh::Input{.item = true}, config);
    ASSERT_TRUE(state.dialog.showing());
    EXPECT_EQ(state.dialog.animation.phase(), WindowKit::Phase::Opening);
    EXPECT_EQ(state.dialog.page(), 0u);
    EXPECT_EQ(state.dialog.shown(), 0u);  // the press that opens it is not its first confirm
    EXPECT_EQ(state.reading, Material::Wood);
    // Through the opening and the typing, the world holds still.
    for (int i = 0; i < 30; ++i) {
        zorlduh::tick(state, zorlduh::Input{.move = Hold({.left = true})}, config);
    }
    EXPECT_EQ(state.player.position(), kBelowTheSign);
    EXPECT_EQ(state.player.facing(), Dir::Up);
}

TEST(Signs, ItemShowsEachPageTurnsItAndClosesTheMessageAfterTheLast) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = standing(config, kBelowTheSign, Dir::Up);
    zorlduh::tick(state, zorlduh::Input{.item = true}, config);
    // 144×32 inside the box's 8 px border: 18 characters by 4 lines, four pages of the meadow's sign.
    EXPECT_EQ(state.dialog.pageCount(), 4u);
    zorlduh::tick(state, zorlduh::Input{.item = true}, config);  // opens the box at once
    EXPECT_EQ(state.dialog.animation.phase(), WindowKit::Phase::Open);
    EXPECT_EQ(state.dialog.shown(), 0u);
    for (std::size_t page = 0; page < 4; ++page) {
        EXPECT_EQ(state.dialog.page(), page);
        zorlduh::tick(state, zorlduh::Input{.item = true}, config);  // shows the page whole
        EXPECT_TRUE(state.dialog.pageDone());
        zorlduh::tick(state, zorlduh::Input{.item = true}, config);  // turns it, or starts the close after the last
    }
    EXPECT_TRUE(state.dialog.showing());
    EXPECT_EQ(state.dialog.animation.phase(), WindowKit::Phase::Closing);
    zorlduh::tick(state, zorlduh::Input{.item = true}, config);  // closes the box at once
    EXPECT_FALSE(state.dialog.showing());
    zorlduh::tick(state, zorlduh::Input{.move = Hold({.left = true})}, config);
    EXPECT_EQ(state.player.position(), (Vec2i{kBelowTheSign.x - 1, kBelowTheSign.y}));
}

TEST(Signs, ItemFacingNothingOrDuringAScrollOpensNothing) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState away = standing(config, kBelowTheSign, Dir::Down);
    zorlduh::tick(away, zorlduh::Input{.item = true}, config);
    EXPECT_FALSE(away.dialog.showing());

    zorlduh::GameState walking = standing(config, zorlduh::kPlayerStart, Dir::Right);
    while (!walking.camera.transitioning()) {
        zorlduh::tick(walking, zorlduh::Input{.move = Hold({.right = true})}, config);
    }
    zorlduh::tick(walking, zorlduh::Input{.item = true}, config);
    EXPECT_FALSE(walking.dialog.showing());
}

// ── The message box ──────────────────────────────────────────────────────────────────────────

TEST(Signs, TheMessageBoxIsDrawnInTheBorderOfWhatIsRead) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = standing(config, kBelowTheSign, Dir::Up);
    const retropp::DrawLayer closed = zorlduh::Dialog(state, assets, 21);
    EXPECT_EQ(std::string_view(closed.key), "dialog");
    EXPECT_EQ(closed.z, 21);
    EXPECT_TRUE(std::get<retropp::SpriteContent>(closed.content).sprites.empty());

    zorlduh::tick(state, zorlduh::Input{.item = true}, config);
    untilOpen(state, config);
    const retropp::DrawLayer wood = zorlduh::Dialog(state, assets, 21);
    const std::span<const retropp::Sprite> woodSprites = std::get<retropp::SpriteContent>(wood.content).sprites;
    const retropp::Sprite* corner = nullptr;
    for (const retropp::Sprite& sprite : woodSprites) {
        if (std::string_view(sprite.key) == "dialog:box:top-left:160x48") {
            corner = &sprite;
        }
    }
    ASSERT_NE(corner, nullptr);
    EXPECT_EQ(corner->x, 0);
    EXPECT_EQ(corner->y, 96);
    EXPECT_EQ(corner->atlas, assets.borders.wood.sheet.atlasId);

    // The rock's message is in stone.
    zorlduh::GameState atTheRock{.player = TopDownMovement::Walker{{.x = 220, .y = 16}, Dir::Up},
                                 .camera = WorldKit::Camera{Layout::Overworld, Area::Lake}};
    atTheRock.camera.place(config.world, atTheRock.player.collision(config.player).bounds());
    zorlduh::tick(atTheRock, zorlduh::Input{.item = true}, config);
    ASSERT_TRUE(atTheRock.dialog.showing());
    EXPECT_EQ(atTheRock.reading, Material::Stone);
    EXPECT_EQ(atTheRock.dialog.pageCount(), 2u);
    untilOpen(atTheRock, config);
    const retropp::DrawLayer stone = zorlduh::Dialog(atTheRock, assets, 21);
    ASSERT_FALSE(std::get<retropp::SpriteContent>(stone.content).sprites.empty());
    EXPECT_EQ(std::get<retropp::SpriteContent>(stone.content).sprites.front().atlas,
              assets.borders.stone.sheet.atlasId);
    EXPECT_GT(zorlduh::DialogZ(config.world), zorlduh::HudZ(config.world));
}

}  // namespace
