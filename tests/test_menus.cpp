// The game's menus over the real maps: what the title and pause menus hold; the places WARP lists; the
// title screen, where nothing in the world moves and START begins play; Start pausing the world and
// closing the pause menu; Start doing nothing during a message or a scroll; TEXT setting how fast the
// next message types and WINDOWS how the menus open and close; WARP putting the player in an area,
// clear of its solids; TITLE returning to the title screen; and the menus drawn. No renderer is needed:
// the maps are decoded from the bytes baked into the test binary, and the sheets, fonts and borders are
// handles the test makes up.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "retropp/atlas_manifest.h"
#include "retropp/draw_state.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/palette.h"
#include "retropp/timing.h"

#include "MenuKit/input.h"
#include "MenuKit/menu.h"
#include "MenuKit/menu_config.h"
#include "TextKit/font.h"
#include "TopDownMovement/walker.h"
#include "WindowKit/animation.h"
#include "WindowKit/border.h"
#include "WorldKit/camera.h"
#include "WorldKit/solids.h"

#include "assets/assets.h"
#include "game/areas.h"
#include "game/menus.h"
#include "game/player.h"
#include "game/signs.h"
#include "game/sim.h"
#include "render/layers.h"
#include "render/layout.h"
#include "render/menus.h"

namespace {

using MenuKit::MenuInput;
using retropp::Dir;
using retropp::Vec2i;
using zorlduh::Layout;
using zorlduh::MenuItem;

constexpr retropp::TimingProfile kHz60{.tickPeriodNs = retropp::TickPeriodNs::Hz60};
constexpr retropp::AtlasId kPlainSheet{19};
constexpr retropp::PaletteId kText{2};
constexpr retropp::PaletteId kPlain{21};
constexpr retropp::PaletteId kDim{23};
constexpr retropp::PaletteId kSelected{25};

retropp::AtlasManifest tileset(retropp::AtlasId id, int width, int height, int count = 0) {
    return retropp::AtlasManifest{
        .atlasId = id,
        .slots = retropp::sliceLayout({.width = width, .height = height}, {.width = 8, .height = 8},
                                      retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown, count),
        .kind = retropp::ContentKind::Tileset,
    };
}

// The real maps, with the sheets, palettes, fonts and borders as handles the test makes up. The config's
// world points at the maps, so the assets outlive it.
zorlduh::Assets headlessAssets() {
    zorlduh::Assets assets;
    assets.terrain = tileset(retropp::AtlasId{7}, 56, 8);
    assets.forest = tileset(retropp::AtlasId{9}, 32, 8);
    assets.overlay = tileset(retropp::AtlasId{11}, 32, 8);
    assets.font = TextKit::Font{.sheet = tileset(retropp::AtlasId{13}, 128, 24, 43),
                                .glyphs = zorlduh::kGlyphOrder,
                                .palette = kText};
    assets.dimFont = assets.font;
    assets.dimFont.palette = kDim;
    assets.selectedFont = assets.font;
    assets.selectedFont.palette = kSelected;
    assets.borders = zorlduh::Borders{
        .wood = {.sheet = tileset(retropp::AtlasId{15}, 24, 32, 10), .palette = retropp::PaletteId{3}},
        .stone = {.sheet = tileset(retropp::AtlasId{17}, 24, 32, 10), .palette = retropp::PaletteId{5}},
        .plain = {.sheet = tileset(kPlainSheet, 24, 32, 10), .palette = kPlain},
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
    zorlduh::Config config =
        zorlduh::gameConfig(assets, zorlduh::kView, zorlduh::kAreaDepths, zorlduh::kDialogBox, zorlduh::kMenuBoxes);
    config.player.timing = kHz60;
    return config;
}

// The player at `at` facing `facing`, in the meadow, in play.
zorlduh::GameState standing(const zorlduh::Config& config, Vec2i at = zorlduh::kPlayerStart, Dir facing = Dir::Down) {
    zorlduh::GameState state{.player = TopDownMovement::Walker{at, facing}};
    state.camera.place(config.world, state.player.collision(config.player).bounds());
    return state;
}

constexpr MenuInput kConfirm{.confirm = true};
constexpr MenuInput kDown{.held = {.down = true}, .pressed = {.down = true}};

// The menu taking input on the current screen.
const MenuKit::Menu& menuOf(const zorlduh::GameState& state) {
    return state.screen == zorlduh::Screen::Title ? state.title : state.pause;
}

// Ticks with nothing pressed until the menu on top has opened, then once with `input`.
void choose(zorlduh::GameState& state, const zorlduh::Config& config, const MenuInput& input) {
    for (int i = 0; i < 100 && menuOf(state).showing() &&
                    menuOf(state).levels().back().animation.phase() != WindowKit::Phase::Open;
         ++i) {
        zorlduh::tick(state, {}, config);
    }
    zorlduh::tick(state, {.menu = input}, config);
}

// Moves the cursor down `count` items, then confirms.
void pick(zorlduh::GameState& state, const zorlduh::Config& config, int count) {
    for (int n = 0; n < count; ++n) {
        choose(state, config, kDown);
    }
    choose(state, config, kConfirm);
}

// Ticks with nothing pressed until `done`, at most 100 ticks.
template <typename Done>
void until(zorlduh::GameState& state, const zorlduh::Config& config, Done done) {
    for (int i = 0; i < 100 && !done(); ++i) {
        zorlduh::tick(state, {}, config);
    }
}

// In play with the pause menu open.
zorlduh::GameState paused(const zorlduh::Config& config) {
    zorlduh::GameState state = standing(config);
    zorlduh::tick(state, {.start = true}, config);
    return state;
}

std::span<const retropp::Sprite> spritesOf(const retropp::DrawLayer& layer) {
    return std::get<retropp::SpriteContent>(layer.content).sprites;
}

const retropp::Sprite* find(std::span<const retropp::Sprite> sprites, std::string_view key) {
    for (const retropp::Sprite& sprite : sprites) {
        if (std::string_view(sprite.key) == key) {
            return &sprite;
        }
    }
    return nullptr;
}

// ── What the menus hold ──────────────────────────────────────────────────────────────────────

TEST(Menus, TheTitleAndPauseMenusHoldTheirItems) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);

    const MenuKit::MenuConfig& title = config.title;
    ASSERT_EQ(title.items.size(), 3u);
    EXPECT_EQ(title.items[0].label, "START");
    EXPECT_EQ(title.items[1].label, "CONTINUE");
    EXPECT_FALSE(title.items[1].enabled);
    EXPECT_EQ(title.items[2].label, "OPTIONS");
    EXPECT_FALSE(title.closesOnCancel);
    EXPECT_EQ(title.box, zorlduh::kMenuBoxes.title);

    const MenuKit::MenuConfig& pause = config.pause;
    ASSERT_EQ(pause.items.size(), 4u);
    EXPECT_EQ(pause.items[0].value, MenuKit::Value{MenuItem::Resume});
    EXPECT_EQ(pause.items[3].value, MenuKit::Value{MenuItem::Title});
    EXPECT_TRUE(pause.closesOnCancel);
    ASSERT_TRUE(pause.items[2].opens.has_value());
    const MenuKit::MenuConfig& warp = *pause.items[2].opens;
    ASSERT_EQ(warp.items.size(), 17u);
    EXPECT_EQ(warp.rows, 5);
    EXPECT_EQ(warp.overflow, MenuKit::Overflow::Pages);
    EXPECT_EQ(warp.items[0].label, "Meadow");
    EXPECT_EQ(warp.items[7].label, "West Wing");
    EXPECT_EQ(warp.items[16].label, "Clearing");

    ASSERT_TRUE(pause.items[1].opens.has_value());
    const MenuKit::MenuConfig& options = *pause.items[1].opens;
    ASSERT_EQ(options.items.size(), 2u);
    EXPECT_EQ(options.items[0].opens->items.size(), 3u);
    const MenuKit::MenuConfig& styles = *options.items[1].opens;
    ASSERT_EQ(styles.items.size(), 6u);
    EXPECT_EQ(styles.columns, 2);
    EXPECT_EQ(styles.rows, 2);
    EXPECT_EQ(styles.overflow, MenuKit::Overflow::Scroll);
    EXPECT_FALSE(styles.wraps);
    EXPECT_EQ(styles.items[4].label, "SLIDE");
    // The title menu's OPTIONS is the same menu.
    EXPECT_EQ(title.items[2].opens->items[1].opens->items.size(), 6u);
}

TEST(Menus, WarpListsEveryAreaOnceInOrder) {
    const std::span<const zorlduh::Warp> all = zorlduh::warps();
    ASSERT_EQ(all.size(), 17u);
    std::size_t n = 0;
    for (const auto& [layout, count] : {std::pair{Layout::Overworld, 5}, std::pair{Layout::DungeonOne, 5},
                                        std::pair{Layout::SeamlessForest, 7}}) {
        for (int area = 0; area < count; ++area, ++n) {
            EXPECT_EQ(all[n].layout, layout) << n;
            EXPECT_EQ(all[n].area, area) << n;
        }
    }
    for (std::size_t w = 0; w < all.size(); ++w) {
        EXPECT_EQ(zorlduh::warpOf(static_cast<std::uint32_t>(MenuItem::Warp) + static_cast<std::uint32_t>(w)), w);
    }
    EXPECT_FALSE(zorlduh::warpOf(MenuItem::Resume).has_value());
    EXPECT_FALSE(zorlduh::warpOf(static_cast<std::uint32_t>(MenuItem::Warp) + 17).has_value());
}

// ── The title screen ─────────────────────────────────────────────────────────────────────────

TEST(Menus, TheTitleScreenMovesNothingAndStartBeginsPlayOnceItsMenuHasClosed) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = standing(config);
    state.screen = zorlduh::Screen::Title;
    for (int i = 0; i < 30; ++i) {
        zorlduh::tick(state, {.move = {.held = {.right = true}, .pressed = {.right = true}}, .start = true}, config);
    }
    EXPECT_EQ(state.player.position(), zorlduh::kPlayerStart);
    EXPECT_EQ(state.nameReveal.shown(), 0u);
    ASSERT_TRUE(state.title.showing());
    EXPECT_FALSE(state.pause.showing());

    choose(state, config, kConfirm);  // START
    EXPECT_EQ(state.screen, zorlduh::Screen::Title);
    EXPECT_EQ(state.title.levels()[0].animation.phase(), WindowKit::Phase::Closing);
    until(state, config, [&] { return state.screen == zorlduh::Screen::Playing; });
    EXPECT_EQ(state.screen, zorlduh::Screen::Playing);
    EXPECT_FALSE(state.title.showing());
    zorlduh::tick(state, {.move = {.held = {.right = true}, .pressed = {.right = true}}}, config);
    EXPECT_EQ(state.player.position().x, zorlduh::kPlayerStart.x + 1);
}

// ── The pause menu ───────────────────────────────────────────────────────────────────────────

TEST(Menus, StartPausesTheWorldAndStartOrResumeClosesTheMenu) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = paused(config);
    ASSERT_TRUE(state.pause.showing());
    for (int i = 0; i < 20; ++i) {
        zorlduh::tick(state, {.move = {.held = {.right = true}, .pressed = {.right = true}}}, config);
    }
    EXPECT_EQ(state.player.position(), zorlduh::kPlayerStart);

    zorlduh::tick(state, {.start = true}, config);
    until(state, config, [&] { return !state.pause.showing(); });
    EXPECT_FALSE(state.pause.showing());
    zorlduh::tick(state, {.move = {.held = {.right = true}, .pressed = {.right = true}}}, config);
    EXPECT_EQ(state.player.position().x, zorlduh::kPlayerStart.x + 1);

    zorlduh::GameState resumed = paused(config);
    choose(resumed, config, kConfirm);  // RESUME
    until(resumed, config, [&] { return !resumed.pause.showing(); });
    EXPECT_FALSE(resumed.pause.showing());
}

TEST(Menus, StartDoesNothingDuringAMessageOrAScroll) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    // Below the meadow's sign, facing it.
    zorlduh::GameState reading = standing(config, {.x = 44, .y = 32}, Dir::Up);
    zorlduh::tick(reading, {.item = true}, config);
    ASSERT_TRUE(reading.dialog.showing());
    zorlduh::tick(reading, {.start = true}, config);
    EXPECT_FALSE(reading.pause.showing());

    // Walking out of the meadow's east door.
    zorlduh::GameState walking = standing(config, {.x = 140, .y = 56}, Dir::Right);
    for (int i = 0; i < 40 && !walking.camera.transitioning(); ++i) {
        zorlduh::tick(walking, {.move = {.held = {.right = true}}}, config);
    }
    ASSERT_TRUE(walking.camera.transitioning());
    zorlduh::tick(walking, {.start = true}, config);
    EXPECT_FALSE(walking.pause.showing());
}

TEST(Menus, TextSetsHowFastTheNextMessageTypesAndStepsBack) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = standing(config, {.x = 44, .y = 32}, Dir::Up);
    zorlduh::tick(state, {.start = true}, config);
    pick(state, config, 1);  // OPTIONS
    pick(state, config, 0);  // TEXT
    pick(state, config, 2);  // FAST
    EXPECT_EQ(state.messageTicksPerGlyph, 1u);
    until(state, config, [&] { return state.pause.levels().size() == 2; });
    ASSERT_EQ(state.pause.levels().size(), 2u);  // back at OPTIONS

    zorlduh::tick(state, {.start = true}, config);
    until(state, config, [&] { return !state.pause.showing(); });
    zorlduh::tick(state, {.item = true}, config);
    ASSERT_TRUE(state.dialog.showing());
    until(state, config, [&] { return state.dialog.animation.phase() == WindowKit::Phase::Open; });
    const std::size_t typed = state.dialog.shown();
    for (int i = 0; i < 4; ++i) {
        zorlduh::tick(state, {}, config);
    }
    EXPECT_EQ(state.dialog.shown(), typed + 4);
}

TEST(Menus, WindowsSetsHowTheMenusOpenAndCloseAndStepsBack) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = paused(config);
    pick(state, config, 1);  // OPTIONS
    pick(state, config, 1);  // WINDOWS
    pick(state, config, 2);  // SLIDE: down twice in two columns, the third row scrolled into view
    EXPECT_EQ(state.menuStyle, zorlduh::MessageStyle::Slide);
    ASSERT_EQ(state.pause.levels().size(), 3u);
    EXPECT_EQ(state.pause.levels()[2].top, 1u);
    EXPECT_EQ(state.pause.levels()[2].animation.phase(), WindowKit::Phase::Closing);
    zorlduh::tick(state, {}, config);
    const LayerKit::Layers layers = zorlduh::PauseMenu(state, config, assets, zorlduh::MenuZ(config.world));
    ASSERT_EQ(layers.size(), 3u);
    EXPECT_NE(layers[2].scroll, retropp::LayerScroll{});  // the styles' window slides away
    EXPECT_EQ(layers[1].scroll, retropp::LayerScroll{});
    until(state, config, [&] { return state.pause.levels().size() == 2; });
    EXPECT_EQ(state.pause.levels().size(), 2u);
}

TEST(Menus, WarpPutsThePlayerInTheAreaChosen) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = paused(config);
    pick(state, config, 2);  // WARP
    pick(state, config, 9);  // Treasury
    EXPECT_EQ(state.camera.layout(), Layout::DungeonOne);
    EXPECT_EQ(state.camera.area(), zorlduh::Room::Treasury);
    EXPECT_FALSE(state.camera.solids(config.world).solidBox(state.player.collision(config.player).bounds()));
    ASSERT_EQ(state.events.size(), 1u);
    const auto* changed = std::get_if<zorlduh::AreaChanged>(&state.events[0]);
    ASSERT_NE(changed, nullptr);
    EXPECT_EQ(changed->layout, Layout::DungeonOne);
    EXPECT_EQ(changed->area, zorlduh::Room::Treasury);
    for (const MenuKit::Level& level : state.pause.levels()) {
        EXPECT_EQ(level.animation.phase(), WindowKit::Phase::Closing);
    }
    until(state, config, [&] { return !state.pause.showing(); });
    EXPECT_FALSE(state.pause.showing());
}

TEST(Menus, EveryWarpLandsClearOfItsAreasSolids) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    const std::span<const zorlduh::Warp> all = zorlduh::warps();
    for (std::size_t n = 0; n < all.size(); ++n) {
        zorlduh::GameState state = paused(config);
        pick(state, config, 2);
        pick(state, config, static_cast<int>(n));
        EXPECT_EQ(state.camera.layout(), all[n].layout) << n;
        EXPECT_EQ(state.camera.area(), all[n].area) << n;
        EXPECT_FALSE(state.camera.solids(config.world).solidBox(state.player.collision(config.player).bounds())) << n;
    }
}

TEST(Menus, TitleReturnsToTheTitleScreenKeepingTheSettings) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = standing(config, {.x = 100, .y = 70}, Dir::Left);
    state.messageTicksPerGlyph = 4;
    state.menuStyle = zorlduh::MessageStyle::Fade;
    zorlduh::tick(state, {.start = true}, config);
    pick(state, config, 3);  // TITLE
    EXPECT_EQ(state.screen, zorlduh::Screen::Title);
    EXPECT_EQ(state.player.position(), zorlduh::kPlayerStart);
    EXPECT_EQ(state.camera.layout(), zorlduh::kStartLayout);
    EXPECT_EQ(state.camera.area(), zorlduh::kStartArea);
    EXPECT_FALSE(state.pause.showing());
    EXPECT_FALSE(state.dialog.showing());
    EXPECT_EQ(state.messageTicksPerGlyph, 4u);
    EXPECT_EQ(state.menuStyle, zorlduh::MessageStyle::Fade);
    zorlduh::tick(state, {}, config);
    EXPECT_TRUE(state.title.showing());
}

// ── Drawn ────────────────────────────────────────────────────────────────────────────────────

TEST(Menus, ThePauseMenuAndTheTitleScreenAreDrawnInThePlainBorder) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    const std::int32_t z = zorlduh::MenuZ(config.world);
    zorlduh::GameState state = standing(config);
    EXPECT_TRUE(zorlduh::PauseMenu(state, config, assets, z).empty());

    zorlduh::tick(state, {.start = true}, config);
    pick(state, config, 1);  // OPTIONS
    until(state, config, [&] { return state.pause.levels().back().animation.phase() == WindowKit::Phase::Open; });
    const LayerKit::Layers layers = zorlduh::PauseMenu(state, config, assets, z);
    ASSERT_EQ(layers.size(), 2u);
    EXPECT_EQ(std::string_view(layers[0].key), "pause:0");
    EXPECT_EQ(layers[0].z, z);
    EXPECT_EQ(std::string_view(layers[1].key), "pause:1");
    EXPECT_EQ(layers[1].z, z + 1);
    for (const retropp::Sprite& sprite : spritesOf(layers[1])) {
        if (std::string_view(sprite.key).starts_with("pause:1:box:")) {
            EXPECT_EQ(sprite.atlas, kPlainSheet) << std::string_view(sprite.key);
            EXPECT_EQ(sprite.palette, kPlain) << std::string_view(sprite.key);
        }
    }
    EXPECT_EQ(find(spritesOf(layers[1]), "pause:1:item:0:0")->palette, kSelected);  // TEXT, under the cursor
    EXPECT_EQ(find(spritesOf(layers[1]), "pause:1:item:1:0")->palette, kText);      // WINDOWS
    EXPECT_EQ(find(spritesOf(layers[0]), "pause:0:item:1:0")->palette, kSelected);  // OPTIONS, beneath
    EXPECT_EQ(find(spritesOf(layers[0]), "pause:0:item:0:0")->palette, kText);      // RESUME

    zorlduh::GameState title = standing(config);
    title.screen = zorlduh::Screen::Title;
    until(title, config, [&] {
        return title.title.showing() && title.title.levels()[0].animation.phase() == WindowKit::Phase::Open;
    });
    const LayerKit::Layers screen = zorlduh::TitleScreen(title, config, assets, 40, 50);
    ASSERT_EQ(screen.size(), 2u);
    EXPECT_EQ(std::string_view(screen[0].key), "title:name");
    EXPECT_EQ(screen[0].z, 40);
    EXPECT_EQ(spritesOf(screen[0]).front().y, zorlduh::kTitleNameY);
    EXPECT_EQ(std::string_view(screen[1].key), "title:0");
    EXPECT_EQ(screen[1].z, 50);
    EXPECT_EQ(find(spritesOf(screen[1]), "title:0:item:1:0")->palette, kDim);       // CONTINUE
    EXPECT_EQ(find(spritesOf(screen[1]), "title:0:item:0:0")->palette, kSelected);  // START
}

}  // namespace
