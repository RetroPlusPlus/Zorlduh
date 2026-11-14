// The MenuKit module: a menu tree written with designated initializers, copied and moved; reading a
// menu's input through a run loop; opening, moving, wrapping and stopping in a list and on a grid,
// pages and scrolling, the repeat; confirm and cancel, the one confirm rule while a window opens or
// closes, close and back; the cursor kept inside a menu whose items change; each item's cell; the
// labels, the cursor and the arrows as sprites; every open menu as a layer. The borders and fonts are
// built over made-up manifests and handles. The cursor — the border's arrow turned a quarter — is also
// drawn through a renderer on a GPU device and read back.

#include <gtest/gtest.h>

#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "retropp/atlas_manifest.h"
#include "retropp/clock.h"
#include "retropp/draw_state.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/input.h"
#include "retropp/palette.h"
#include "retropp/renderer.h"
#include "retropp/run_loop.h"
#include "retropp/timing.h"
#include "retropp/viewport.h"

#include "LayerKit/types.h"
#include "MenuKit/input.h"
#include "MenuKit/menu.h"
#include "MenuKit/menu_config.h"
#include "MenuKit/menu_windows.h"
#include "TextKit/font.h"
#include "WindowKit/animation.h"
#include "WindowKit/border.h"

namespace {

using MenuKit::Directions;
using MenuKit::Item;
using MenuKit::Level;
using MenuKit::Menu;
using MenuKit::MenuConfig;
using MenuKit::MenuInput;
using MenuKit::Step;
using MenuKit::Value;
using retropp::IntRect;
using retropp::Sprite;

constexpr retropp::PaletteId kBorderColors{4};
constexpr retropp::PaletteId kText{6};
constexpr retropp::PaletteId kSelected{7};
constexpr retropp::PaletteId kDim{8};

enum class Choice : std::uint32_t { Play, Red, Green, One, Two };

// A border sheet of `cell`-sized cells, three to a row and four rows down, carved to `slots` slots.
WindowKit::Border border(int cell, std::size_t slots) {
    return WindowKit::Border{
        .sheet = retropp::AtlasManifest{
            .atlasId = retropp::AtlasId{9},
            .slots = retropp::sliceLayout({.width = 3 * cell, .height = 4 * cell}, {.width = cell, .height = cell},
                                          retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
                                          static_cast<int>(slots)),
            .kind = retropp::ContentKind::Tileset,
        },
        .palette = kBorderColors,
    };
}

// A font of 8-pixel steps over a 128×24 sheet of 8×8 cells, uppercase, digits and punctuation, through
// `palette`.
TextKit::Font font(retropp::PaletteId palette = kText) {
    return TextKit::Font{
        .sheet = retropp::AtlasManifest{
            .atlasId = retropp::AtlasId{5},
            .slots = retropp::sliceLayout({.width = 128, .height = 24}, {.width = 8, .height = 8},
                                          retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown, 43),
            .kind = retropp::ContentKind::Tileset,
        },
        .glyphs = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?'-:",
        .palette = palette,
    };
}

MenuKit::Look look(const WindowKit::Border& b = border(8, 10)) { return MenuKit::Look{.border = b, .font = font()}; }

// A menu of `count` items labeled ITEM0, ITEM1, … and valued by their place.
MenuConfig list(std::size_t count, MenuConfig config = {}) {
    if (config.box == IntRect{}) {
        config.box = {.x = 0, .y = 0, .width = 80, .height = 48};
    }
    for (std::size_t n = 0; n < count; ++n) {
        config.items.push_back(Item{.label = "ITEM" + std::to_string(n), .value = n});
    }
    return config;
}

// A three-deep tree: a first menu, a menu it opens, and a menu that one opens.
MenuConfig tree() {
    return MenuConfig{
        .box = {.x = 0, .y = 0, .width = 64, .height = 48},
        .items = {
            {.label = "PLAY", .value = Choice::Play},
            {.label = "COLORS",
             .opens = MenuConfig{
                 .box = {.x = 8, .y = 8, .width = 64, .height = 40},
                 .columns = 2,
                 .items = {
                     {.label = "RED", .value = Choice::Red},
                     {.label = "GREEN", .value = Choice::Green},
                     {.label = "MORE",
                      .opens = MenuConfig{
                          .box = {.x = 16, .y = 16, .width = 48, .height = 32},
                          .items = {{.label = "ONE", .value = Choice::One},
                                    {.label = "TWO", .value = Choice::Two, .enabled = false}},
                      }},
                 },
             }},
        },
    };
}

WindowKit::Animation ticks(std::uint32_t count) {
    WindowKit::Animation animation;
    animation.ticks = count;
    return animation;
}

// Input that presses and holds `directions` this tick.
MenuInput press(Directions directions) { return MenuInput{.held = directions, .pressed = directions}; }
// Input that holds `directions`, none newly pressed.
MenuInput hold(Directions directions) { return MenuInput{.held = directions}; }
constexpr MenuInput kConfirm{.confirm = true};
constexpr MenuInput kCancel{.cancel = true};
constexpr Directions kUp{.up = true};
constexpr Directions kDown{.down = true};
constexpr Directions kLeft{.left = true};
constexpr Directions kRight{.right = true};

bool nothing(const Step& step) {
    return !step.chosen && !step.refused && !step.moved && !step.opened && !step.cancelled && !step.closed;
}

// A menu open on `config`, its first advance not yet run.
Menu opened(const MenuConfig& config) {
    Menu menu;
    menu.open(config);
    return menu;
}

std::size_t cursor(const Menu& menu) { return menu.levels().back().cursor; }
std::size_t top(const Menu& menu) { return menu.levels().back().top; }

// The cursor after pressing each direction in turn on `config`, from item `start`.
std::vector<std::size_t> walk(const MenuConfig& config, std::size_t start, std::initializer_list<Directions> moves) {
    Menu menu = opened(config);
    for (std::size_t n = 0; n < start; ++n) {
        menu.advance(config, press(kRight));  // a single column moves only up and down, so walk by index
    }
    std::vector<std::size_t> at;
    for (const Directions& d : moves) {
        menu.advance(config, press(d));
        at.push_back(cursor(menu));
    }
    return at;
}

// A level with its window open, the cursor and the first row showing as given.
Level openLevel(std::size_t cursorAt = 0, std::size_t topRow = 0) {
    Level level{.cursor = cursorAt, .top = topRow};
    level.animation.open();
    return level;
}

const Sprite* find(std::span<const Sprite> sprites, std::string_view key) {
    for (const Sprite& sprite : sprites) {
        if (std::string_view(sprite.key) == key) {
            return &sprite;
        }
    }
    return nullptr;
}

std::vector<std::string> keys(std::span<const Sprite> sprites) {
    std::vector<std::string> out;
    for (const Sprite& sprite : sprites) {
        out.emplace_back(std::string_view(sprite.key));
    }
    return out;
}

std::span<const Sprite> spritesOf(const retropp::DrawLayer& layer) {
    return std::get<retropp::SpriteContent>(layer.content).sprites;
}

// ── The config ───────────────────────────────────────────────────────────────────────────────

TEST(MenuConfig, ATreeIsWrittenWithDesignatedInitializersAndCopiesAndMoves) {
    const MenuConfig root = tree();
    ASSERT_EQ(root.items.size(), 2u);
    ASSERT_TRUE(root.items[1].opens.has_value());
    const MenuConfig& colors = *root.items[1].opens;
    EXPECT_EQ(colors.columns, 2);
    ASSERT_EQ(colors.items.size(), 3u);
    ASSERT_TRUE(colors.items[2].opens.has_value());
    const MenuConfig& more = *colors.items[2].opens;
    ASSERT_EQ(more.items.size(), 2u);
    EXPECT_EQ(more.items[1].value, Value{Choice::Two});
    EXPECT_FALSE(more.items[1].enabled);
    EXPECT_EQ(more.box, (IntRect{.x = 16, .y = 16, .width = 48, .height = 32}));

    MenuConfig copy = root;
    copy.items[1].opens->items[2].opens->items[0].label = "UNO";
    EXPECT_EQ(root.items[1].opens->items[2].opens->items[0].label, "ONE");
    EXPECT_EQ(copy.items[1].opens->items[2].opens->items[0].label, "UNO");

    const MenuConfig moved = std::move(copy);
    EXPECT_EQ(moved.items[1].opens->items[2].opens->items[0].label, "UNO");
    EXPECT_EQ(moved.items[0].value, Value{Choice::Play});
    EXPECT_FALSE(moved.items[0].opens.has_value());

    // configOf reaches each menu of the tree as it is opened.
    Menu menu = opened(root);
    menu.advance(root, press(kDown));
    menu.advance(root, kConfirm);
    menu.advance(root, press(kDown));
    menu.advance(root, kConfirm);
    ASSERT_EQ(menu.levels().size(), 3u);
    EXPECT_EQ(&MenuKit::configOf(root, menu, 0), &root);
    EXPECT_EQ(&MenuKit::configOf(root, menu, 1), &*root.items[1].opens);
    EXPECT_EQ(&MenuKit::configOf(root, menu, 2), &*root.items[1].opens->items[2].opens);
}

// ── Reading the input ────────────────────────────────────────────────────────────────────────

enum class Action : std::uint8_t { Up, Down, Left, Right, Accept, Back, Jump };

class ManualClock final : public retropp::Clock {
public:
    [[nodiscard]] std::chrono::nanoseconds now() const noexcept override { return now_; }
    void advance(std::chrono::nanoseconds by) noexcept { now_ += by; }

private:
    std::chrono::nanoseconds now_{};
};

TEST(ReadMenuInput, HeldPressedConfirmAndCancelComeFromTheGamesOwnActions) {
    ManualClock clock;
    retropp::RunLoop loop{clock, retropp::TimingProfile{.tickPeriodNs = retropp::TickPeriodNs::Hz60}};
    std::vector<MenuInput> read;
    loop.simTick([&](const retropp::InputState& input) {
        read.push_back(MenuKit::readMenuInput(input, {.up = Action::Up, .down = Action::Down, .left = Action::Left,
                                                      .right = Action::Right, .confirm = Action::Accept,
                                                      .cancel = Action::Back}));
    });
    loop.renderLoop([] {});
    loop.advance();  // the first call starts the clock
    const std::initializer_list<std::initializer_list<Action>> held{
        {Action::Left}, {Action::Left, Action::Up, Action::Accept}, {Action::Accept, Action::Back, Action::Jump}, {}};
    for (const auto& actions : held) {
        retropp::InputSample sample;
        for (const Action action : actions) {
            sample.players[0].held.set(retropp::actionId(action), true);
        }
        loop.setRawInput(sample);
        clock.advance(loop.tickPeriod());
        loop.advance();
    }
    ASSERT_EQ(read.size(), 4u);
    EXPECT_EQ(read[0], (MenuInput{.held = {.left = true}, .pressed = {.left = true}}));
    EXPECT_EQ(read[1], (MenuInput{.held = {.up = true, .left = true}, .pressed = {.up = true}, .confirm = true}));
    EXPECT_EQ(read[2], (MenuInput{.cancel = true}));  // Accept still held: not pressed again
    EXPECT_EQ(read[3], MenuInput{});
}

// ── Opening ──────────────────────────────────────────────────────────────────────────────────

TEST(Menu, OpenShowsOneLevelWithTheCursorOnTheFirstItemAndItsWindowOpening) {
    Menu menu;
    EXPECT_FALSE(menu.showing());
    EXPECT_TRUE(menu.levels().empty());
    menu.open(list(3, {.animation = ticks(4)}));
    ASSERT_TRUE(menu.showing());
    ASSERT_EQ(menu.levels().size(), 1u);
    EXPECT_EQ(menu.levels()[0].cursor, 0u);
    EXPECT_EQ(menu.levels()[0].top, 0u);
    EXPECT_EQ(menu.levels()[0].animation.phase(), WindowKit::Phase::Opening);
    EXPECT_EQ(menu.levels()[0].animation.length(), 4u);

    // Nothing while showing.
    menu.open(list(5));
    EXPECT_EQ(menu.levels().size(), 1u);
    EXPECT_EQ(menu.levels()[0].animation.length(), 4u);

    // Nothing for a menu with no items.
    Menu empty;
    empty.open(MenuConfig{});
    EXPECT_FALSE(empty.showing());
    EXPECT_TRUE(nothing(empty.advance(MenuConfig{}, kConfirm)));
}

// ── Moving ───────────────────────────────────────────────────────────────────────────────────

TEST(Menu, UpAndDownMoveARowAndOnlyAMoveIsReported) {
    const MenuConfig config = list(3);
    Menu menu = opened(config);
    Step step = menu.advance(config, press(kDown));
    EXPECT_TRUE(step.moved);
    EXPECT_EQ(cursor(menu), 1u);
    step = menu.advance(config, press(kDown));
    EXPECT_EQ(cursor(menu), 2u);
    step = menu.advance(config, press(kUp));
    EXPECT_TRUE(step.moved);
    EXPECT_EQ(cursor(menu), 1u);
    EXPECT_TRUE(nothing(menu.advance(config, {})));
    // Left and right in one column go nowhere.
    EXPECT_TRUE(nothing(menu.advance(config, press(kRight))));
    EXPECT_EQ(cursor(menu), 1u);
}

TEST(Menu, PastAnEdgeTheCursorComesRoundWithWrapsAndStaysWithout) {
    EXPECT_EQ(walk(list(3), 0, {kUp, kDown}), (std::vector<std::size_t>{2, 0}));
    EXPECT_EQ(walk(list(3, {.wraps = false}), 0, {kUp, kDown, kDown, kDown}),
              (std::vector<std::size_t>{0, 1, 2, 2}));
    Menu stopped = opened(list(3, {.wraps = false}));
    EXPECT_TRUE(nothing(stopped.advance(list(3, {.wraps = false}), press(kUp))));

    // A grid three across and two down.
    const MenuConfig grid = list(6, {.columns = 3});
    EXPECT_EQ(walk(grid, 0, {kLeft, kRight, kDown, kDown, kUp}), (std::vector<std::size_t>{2, 0, 3, 0, 3}));
    EXPECT_EQ(walk(grid, 2, {kRight}), (std::vector<std::size_t>{0}));
    const MenuConfig held = list(6, {.columns = 3, .wraps = false});
    EXPECT_EQ(walk(held, 0, {kLeft, kUp, kRight, kRight, kRight, kDown, kDown}),
              (std::vector<std::size_t>{0, 0, 1, 2, 2, 5, 5}));
}

TEST(Menu, AMoveIntoAShortLastRowLandsOnItsLastItem) {
    // Three across: 0 1 2 / 3 4.
    const MenuConfig grid = list(5, {.columns = 3});
    EXPECT_EQ(walk(grid, 2, {kDown}), (std::vector<std::size_t>{4}));
    EXPECT_EQ(walk(grid, 2, {kUp}), (std::vector<std::size_t>{4}));
    EXPECT_EQ(walk(grid, 0, {kDown, kRight, kRight}), (std::vector<std::size_t>{3, 4, 3}));
    EXPECT_EQ(walk(grid, 0, {kDown, kLeft}), (std::vector<std::size_t>{3, 4}));
    const MenuConfig stops = list(5, {.columns = 3, .wraps = false});
    EXPECT_EQ(walk(stops, 0, {kDown, kRight, kRight}), (std::vector<std::size_t>{3, 4, 4}));
}

TEST(Menu, SeveralDirectionsPressedAtOnceTakeTheFirstOfUpDownLeftRight) {
    const MenuConfig grid = list(6, {.columns = 3});
    EXPECT_EQ(walk(grid, 0, {{.up = true, .down = true, .right = true}}), (std::vector<std::size_t>{3}));
    EXPECT_EQ(walk(grid, 0, {{.down = true, .right = true}}), (std::vector<std::size_t>{3}));
    EXPECT_EQ(walk(grid, 0, {{.left = true, .right = true}}), (std::vector<std::size_t>{2}));
}

TEST(Menu, PagesShowThePageTheCursorIsOn) {
    const MenuConfig config = list(10, {.rows = 4, .overflow = MenuKit::Overflow::Pages});
    Menu menu = opened(config);
    std::vector<std::size_t> tops;
    for (int n = 0; n < 9; ++n) {
        menu.advance(config, press(kDown));
        tops.push_back(top(menu));
    }
    EXPECT_EQ(tops, (std::vector<std::size_t>{0, 0, 0, 4, 4, 4, 4, 8, 8}));
    EXPECT_EQ(cursor(menu), 9u);
    menu.advance(config, press(kDown));  // wraps to the first page
    EXPECT_EQ(cursor(menu), 0u);
    EXPECT_EQ(top(menu), 0u);
    menu.advance(config, press(kUp));    // and back to the last
    EXPECT_EQ(cursor(menu), 9u);
    EXPECT_EQ(top(menu), 8u);
    for (int n = 0; n < 2; ++n) {
        menu.advance(config, press(kUp));
    }
    EXPECT_EQ(cursor(menu), 7u);
    EXPECT_EQ(top(menu), 4u);
}

TEST(Menu, ScrollFollowsTheCursorARowAtATime) {
    const MenuConfig config = list(6, {.rows = 3, .overflow = MenuKit::Overflow::Scroll});
    Menu menu = opened(config);
    std::vector<std::size_t> tops;
    for (const Directions& d : {kDown, kDown, kDown, kDown, kDown, kUp, kUp, kUp, kUp}) {
        menu.advance(config, press(d));
        tops.push_back(top(menu));
    }
    EXPECT_EQ(tops, (std::vector<std::size_t>{0, 0, 1, 2, 3, 3, 3, 2, 1}));
    menu.advance(config, press(kDown));  // from 1 to 2: in view
    EXPECT_EQ(top(menu), 1u);
    // Wrapping from the last row to the first brings the first row into view.
    const MenuConfig wraps = list(6, {.rows = 3, .overflow = MenuKit::Overflow::Scroll});
    Menu round = opened(wraps);
    round.advance(wraps, press(kUp));
    EXPECT_EQ(cursor(round), 5u);
    EXPECT_EQ(top(round), 3u);
    round.advance(wraps, press(kDown));
    EXPECT_EQ(top(round), 0u);

    // A list that fits never moves the rows.
    const MenuConfig fits = list(3, {.rows = 3, .overflow = MenuKit::Overflow::Scroll});
    Menu still = opened(fits);
    for (const Directions& d : {kDown, kDown, kDown, kUp}) {
        still.advance(fits, press(d));
        EXPECT_EQ(top(still), 0u);
    }
}

TEST(Menu, RowsOfZeroOrMoreThanTheMenuHasShowEveryRow) {
    for (const int rows : {0, 9}) {
        const MenuConfig config = list(5, {.rows = rows});
        Menu menu = opened(config);
        for (int n = 0; n < 5; ++n) {
            menu.advance(config, press(kDown));
            EXPECT_EQ(top(menu), 0u) << rows;
        }
        const Level level = openLevel();
        EXPECT_EQ(MenuKit::itemBox(config, level, border(8, 10), 4),
                  (IntRect{.x = 8, .y = 8 + 4 * 6, .width = 64, .height = 6}))
            << rows;
    }
}

// ── The repeat ───────────────────────────────────────────────────────────────────────────────

// The ticks, counted from the press at 0, on which a held Down moved the cursor.
std::vector<int> repeats(const MenuConfig& config, int held) {
    Menu menu = opened(config);
    menu.advance(config, press(kDown));
    std::vector<int> moved;
    for (int tick = 1; tick <= held; ++tick) {
        if (menu.advance(config, hold(kDown)).moved) {
            moved.push_back(tick);
        }
    }
    return moved;
}

TEST(Menu, AHeldDirectionRepeatsAfterTheDelayThenOncePerEvery) {
    // No `every`: nothing repeats.
    EXPECT_TRUE(repeats(list(20), 30).empty());
    // A delay of 4 ticks, then every 2.
    const MenuConfig config = list(20, {.repeat = {.delayTicks = 4, .everyTicks = 2}});
    EXPECT_EQ(repeats(config, 9), (std::vector<int>{4, 6, 8}));
    // A delay of 0 repeats from `every`.
    EXPECT_EQ(repeats(list(20, {.repeat = {.everyTicks = 3}}), 9), (std::vector<int>{3, 6, 9}));
    // Times resolve to ticks through `timing`: 400 ms and 100 ms at 60 Hz are 24 and 6 ticks.
    const MenuConfig timed = list(40, {.repeat = {.delay = std::chrono::milliseconds{400},
                                                  .every = std::chrono::milliseconds{100},
                                                  .timing = {.tickPeriodNs = retropp::TickPeriodNs::Hz60}}});
    EXPECT_EQ(repeats(timed, 36), (std::vector<int>{24, 30, 36}));

    // Released, or pressed again, the count starts over.
    Menu menu = opened(config);
    menu.advance(config, press(kDown));
    for (int tick = 0; tick < 3; ++tick) {
        menu.advance(config, hold(kDown));
    }
    menu.advance(config, {});
    menu.advance(config, hold(kDown));  // held again but not pressed: no repeat
    menu.advance(config, hold(kDown));
    menu.advance(config, hold(kDown));
    menu.advance(config, hold(kDown));
    EXPECT_EQ(cursor(menu), 1u);
    menu.advance(config, press(kDown));
    EXPECT_EQ(cursor(menu), 2u);
    for (int tick = 0; tick < 3; ++tick) {
        EXPECT_FALSE(menu.advance(config, hold(kDown)).moved);
    }
    EXPECT_TRUE(menu.advance(config, hold(kDown)).moved);
    // Another direction pressed takes over.
    menu.advance(config, press(kUp));
    EXPECT_EQ(cursor(menu), 2u);
    for (int tick = 0; tick < 3; ++tick) {
        EXPECT_FALSE(menu.advance(config, hold(kUp)).moved);
    }
    EXPECT_TRUE(menu.advance(config, hold(kUp)).moved);
    EXPECT_EQ(cursor(menu), 1u);
}

// ── Confirm and cancel ───────────────────────────────────────────────────────────────────────

TEST(Menu, ConfirmOnAnItemWithAValueChoosesItAndChangesNothing) {
    const MenuConfig config = list(3);
    Menu menu = opened(config);
    menu.advance(config, press(kDown));
    const Step step = menu.advance(config, kConfirm);
    ASSERT_TRUE(step.chosen.has_value());
    EXPECT_EQ(*step.chosen, Value{1});
    EXPECT_FALSE(step.moved || step.opened || step.refused || step.cancelled || step.closed);
    ASSERT_EQ(menu.levels().size(), 1u);
    EXPECT_EQ(cursor(menu), 1u);
    EXPECT_EQ(menu.levels()[0].animation.phase(), WindowKit::Phase::Open);
}

TEST(Menu, ConfirmOnADisabledItemIsRefusedAndTheCursorLandsOnIt) {
    MenuConfig config = list(3);
    config.items[1].enabled = false;
    Menu menu = opened(config);
    EXPECT_TRUE(menu.advance(config, press(kDown)).moved);
    EXPECT_EQ(cursor(menu), 1u);
    const Step step = menu.advance(config, kConfirm);
    EXPECT_TRUE(step.refused);
    EXPECT_FALSE(step.chosen.has_value());
}

TEST(Menu, ConfirmOnAnItemThatOpensAMenuOpensItOverTheOneOnTop) {
    MenuConfig config = tree();
    config.items[1].opens->animation = ticks(4);
    config.items.push_back(Item{.label = "BOTH", .value = 7, .opens = list(2)});
    config.items.push_back(Item{.label = "EMPTY", .value = 8, .opens = MenuConfig{}});
    config.items.push_back(Item{.label = "NONE"});

    Menu menu = opened(config);
    menu.advance(config, press(kDown));
    Step step = menu.advance(config, kConfirm);
    EXPECT_TRUE(step.opened);
    ASSERT_EQ(menu.levels().size(), 2u);
    EXPECT_EQ(menu.levels()[1].from, 1u);
    EXPECT_EQ(menu.levels()[1].cursor, 0u);
    EXPECT_EQ(menu.levels()[1].animation.phase(), WindowKit::Phase::Opening);
    EXPECT_EQ(menu.levels()[0].cursor, 1u);

    // An item with a value and a menu opens the menu and reports no value.
    Menu both = opened(config);
    both.advance(config, press(kUp));
    both.advance(config, press(kUp));
    both.advance(config, press(kUp));
    ASSERT_EQ(cursor(both), 2u);
    step = both.advance(config, kConfirm);
    EXPECT_TRUE(step.opened);
    EXPECT_FALSE(step.chosen.has_value());
    // An item whose menu is empty, and one with neither, do nothing.
    for (const std::size_t at : {std::size_t{3}, std::size_t{4}}) {
        Menu none = opened(config);
        while (cursor(none) != at) {
            none.advance(config, press(kDown));
        }
        EXPECT_TRUE(nothing(none.advance(config, kConfirm))) << at;
        EXPECT_EQ(none.levels().size(), 1u) << at;
    }
}

TEST(Menu, CancelOverAnotherMenuClosesItAndTheOneBeneathTakesInputAgain) {
    MenuConfig config = tree();
    config.items[1].opens->animation = ticks(2);
    Menu menu = opened(config);
    menu.advance(config, press(kDown));
    menu.advance(config, kConfirm);
    menu.advance(config, {});
    menu.advance(config, {});
    ASSERT_EQ(menu.levels()[1].animation.phase(), WindowKit::Phase::Open);

    const Step step = menu.advance(config, kCancel);
    EXPECT_TRUE(step.cancelled);
    EXPECT_FALSE(step.closed);
    ASSERT_EQ(menu.levels().size(), 2u);
    EXPECT_EQ(menu.levels()[1].animation.phase(), WindowKit::Phase::Closing);
    EXPECT_FALSE(menu.advance(config, press(kDown)).moved);  // the closing window takes the press; nothing moves
    menu.advance(config, {});
    ASSERT_EQ(menu.levels().size(), 1u);  // gone on the tick it closed
    EXPECT_EQ(cursor(menu), 1u);
    EXPECT_TRUE(menu.advance(config, press(kDown)).moved);
    EXPECT_EQ(cursor(menu), 0u);
}

TEST(Menu, CancelOnTheFirstMenuClosesItUnlessItStays) {
    const MenuConfig instant = list(3);
    Menu menu = opened(instant);
    Step step = menu.advance(instant, kCancel);
    EXPECT_TRUE(step.cancelled);
    EXPECT_TRUE(step.closed);
    EXPECT_FALSE(menu.showing());

    const MenuConfig slow = list(3, {.animation = ticks(2)});
    Menu closing = opened(slow);
    closing.advance(slow, {});
    closing.advance(slow, {});
    step = closing.advance(slow, kCancel);
    EXPECT_TRUE(step.cancelled);
    EXPECT_FALSE(step.closed);
    EXPECT_FALSE(closing.advance(slow, {}).closed);
    EXPECT_TRUE(closing.advance(slow, {}).closed);
    EXPECT_FALSE(closing.showing());

    const MenuConfig stays = list(3, {.closesOnCancel = false});
    Menu staying = opened(stays);
    EXPECT_TRUE(nothing(staying.advance(stays, kCancel)));
    EXPECT_TRUE(staying.showing());
    EXPECT_EQ(staying.levels()[0].animation.phase(), WindowKit::Phase::Open);
}

TEST(Menu, ConfirmFinishesAnOpeningOrAClosingAndChoosesNothing) {
    const MenuConfig config = list(3, {.animation = ticks(4)});
    Menu menu = opened(config);
    menu.advance(config, {});
    EXPECT_FALSE(menu.advance(config, press(kDown)).moved);  // no move before it is open
    EXPECT_EQ(cursor(menu), 0u);
    const Step opening = menu.advance(config, kConfirm);
    EXPECT_TRUE(nothing(opening));
    EXPECT_EQ(menu.levels()[0].animation.phase(), WindowKit::Phase::Open);
    EXPECT_TRUE(menu.advance(config, press(kDown)).moved);

    menu.advance(config, kCancel);
    ASSERT_EQ(menu.levels()[0].animation.phase(), WindowKit::Phase::Closing);
    const Step closing = menu.advance(config, kConfirm);
    EXPECT_TRUE(closing.closed);
    EXPECT_FALSE(closing.chosen.has_value());
    EXPECT_FALSE(menu.showing());

    // The tick an opening ends on reads input.
    const MenuConfig quick = list(3, {.animation = ticks(2)});
    Menu reads = opened(quick);
    reads.advance(quick, {});
    EXPECT_TRUE(reads.advance(quick, press(kDown)).moved);
}

TEST(Menu, CloseClosesEveryWindowAndReadsNoInput) {
    MenuConfig config = tree();
    config.animation = ticks(2);
    config.items[1].opens->animation = ticks(2);
    Menu menu = opened(config);
    menu.advance(config, kConfirm);  // opens the first at once
    menu.advance(config, press(kDown));
    menu.advance(config, kConfirm);
    menu.advance(config, kConfirm);  // opens the second at once
    ASSERT_EQ(menu.levels().size(), 2u);
    menu.close();
    EXPECT_EQ(menu.levels()[0].animation.phase(), WindowKit::Phase::Closing);
    EXPECT_EQ(menu.levels()[1].animation.phase(), WindowKit::Phase::Closing);
    Step step = menu.advance(config, press(kDown));
    EXPECT_FALSE(step.moved || step.closed);
    EXPECT_EQ(menu.levels()[1].cursor, 0u);
    step = menu.advance(config, {});
    EXPECT_TRUE(step.closed);
    EXPECT_FALSE(menu.showing());

    // Confirm closes every window at once.
    Menu skipped = opened(config);
    skipped.advance(config, kConfirm);
    skipped.close();
    EXPECT_TRUE(skipped.advance(config, kConfirm).closed);

    // Instant windows: closed on the next advance.
    const MenuConfig instant = tree();
    Menu quick = opened(instant);
    quick.close();
    EXPECT_TRUE(quick.showing());
    EXPECT_TRUE(quick.advance(instant, {}).closed);
    EXPECT_FALSE(quick.showing());
    quick.close();  // nothing while not showing
    EXPECT_FALSE(quick.showing());
}

TEST(Menu, BackClosesTheMenuOnTopWhateverCancelWouldDo) {
    MenuConfig config = tree();
    config.closesOnCancel = false;
    Menu menu = opened(config);
    menu.advance(config, press(kDown));
    menu.advance(config, kConfirm);
    ASSERT_EQ(menu.levels().size(), 2u);
    menu.back();
    Step step = menu.advance(config, {});
    EXPECT_FALSE(step.closed);
    EXPECT_FALSE(step.cancelled);
    ASSERT_EQ(menu.levels().size(), 1u);
    menu.back();
    step = menu.advance(config, {});
    EXPECT_TRUE(step.closed);
    EXPECT_FALSE(menu.showing());
}

TEST(Menu, TheCursorIsKeptInsideTheItemsAndAPathTheTreeLostThrows) {
    MenuConfig config = list(5);
    Menu menu = opened(config);
    for (int n = 0; n < 4; ++n) {
        menu.advance(config, press(kDown));
    }
    ASSERT_EQ(cursor(menu), 4u);
    config.items.resize(2);
    menu.advance(config, {});
    EXPECT_EQ(cursor(menu), 1u);
    EXPECT_THROW((void)MenuKit::configOf(config, menu, 1), std::out_of_range);

    MenuConfig root = tree();
    Menu nested = opened(root);
    nested.advance(root, press(kDown));
    nested.advance(root, kConfirm);
    ASSERT_EQ(nested.levels().size(), 2u);
    root.items[1].opens.reset();
    EXPECT_THROW((void)MenuKit::configOf(root, nested, 1), std::out_of_range);
    EXPECT_THROW((void)nested.advance(root, {}), std::out_of_range);
}

// ── Each item's cell ─────────────────────────────────────────────────────────────────────────

TEST(ItemBox, IsTheItemsShareOfTheRoomAmongTheRowsShowing) {
    const WindowKit::Border eight = border(8, 10);
    // One column: the room 64×32 at (8, 8), three rows of 10 (32 / 3).
    const MenuConfig column = list(3);
    EXPECT_EQ(MenuKit::itemBox(column, openLevel(), eight, 0), (IntRect{.x = 8, .y = 8, .width = 64, .height = 10}));
    EXPECT_EQ(MenuKit::itemBox(column, openLevel(), eight, 2), (IntRect{.x = 8, .y = 28, .width = 64, .height = 10}));
    // A grid two across: the room 74×24 at (18, 28), cells 37×12.
    const MenuConfig grid = list(4, {.box = {.x = 10, .y = 20, .width = 90, .height = 40}, .columns = 2});
    EXPECT_EQ(MenuKit::itemBox(grid, openLevel(), eight, 3), (IntRect{.x = 55, .y = 40, .width = 37, .height = 12}));
    // A border of 16-pixel pieces leaves a smaller room.
    const MenuConfig large = list(2, {.box = {.x = 0, .y = 0, .width = 96, .height = 64}});
    EXPECT_EQ(MenuKit::itemBox(large, openLevel(), border(16, 9), 1),
              (IntRect{.x = 16, .y = 32, .width = 64, .height = 16}));
    // Rows showing from the second: row 1 is the room's first.
    const MenuConfig scrolled = list(6, {.rows = 2});
    EXPECT_EQ(MenuKit::itemBox(scrolled, openLevel(0, 1), eight, 1), (IntRect{.x = 8, .y = 8, .width = 64, .height = 16}));
    // A row not showing, an item past the end, a menu with none: no size.
    EXPECT_EQ(MenuKit::itemBox(scrolled, openLevel(), eight, 3), IntRect{});
    EXPECT_EQ(MenuKit::itemBox(scrolled, openLevel(0, 1), eight, 0), IntRect{});
    EXPECT_EQ(MenuKit::itemBox(column, openLevel(), eight, 3), IntRect{});
    EXPECT_EQ(MenuKit::itemBox(MenuConfig{}, openLevel(), eight, 0), IntRect{});
}

// ── The labels, the cursor and the arrows ────────────────────────────────────────────────────

TEST(MenuItems, EachLabelPastTheArrowCenteredDownItsCellInItsFont) {
    MenuConfig config = list(3);
    config.items[1].enabled = false;
    MenuKit::Look styled = look();
    styled.selected = font(kSelected);
    styled.disabled = font(kDim);

    const LayerKit::Sprites sprites = MenuKit::MenuItems("m", config, openLevel(), styled, 2);
    // Cells 10 high from y 8: a label 8 high sits 1 down.
    const Sprite* first = find(sprites, "m:item:0:0");
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->x, 16);
    EXPECT_EQ(first->y, 9);
    EXPECT_EQ(first->z, 2);
    EXPECT_EQ(find(sprites, "m:item:1:0")->y, 19);
    EXPECT_EQ(find(sprites, "m:item:2:4")->x, 16 + 4 * 8);
    EXPECT_EQ(find(sprites, "m:item:2:4")->y, 29);
    // Under the cursor, selected; disabled, dim; else plain.
    EXPECT_EQ(first->palette, kSelected);
    EXPECT_EQ(find(sprites, "m:item:1:0")->palette, kDim);
    EXPECT_EQ(find(sprites, "m:item:2:0")->palette, kText);
    // The cursor on a disabled item: dim comes first.
    EXPECT_EQ(find(MenuKit::MenuItems("m", config, openLevel(1), styled), "m:item:1:0")->palette, kDim);
    EXPECT_EQ(find(MenuKit::MenuItems("m", config, openLevel(1), styled), "m:item:0:0")->palette, kText);
    // A look without them draws every label in its font.
    for (const Sprite& sprite : MenuKit::MenuItems("m", config, openLevel(), look())) {
        EXPECT_EQ(sprite.palette, kText) << std::string_view(sprite.key);
    }
    // A sheet without the arrow slot: labels start at the cell's edge.
    EXPECT_EQ(find(MenuKit::MenuItems("m", config, openLevel(), look(border(8, 9))), "m:item:0:0")->x, 8);
    // An empty label draws nothing; only the rows showing are drawn.
    config.items[2].label.clear();
    EXPECT_EQ(find(MenuKit::MenuItems("m", config, openLevel(), styled), "m:item:2:0"), nullptr);
    const LayerKit::Sprites paged = MenuKit::MenuItems("m", list(6, {.rows = 2}), openLevel(2, 1), look());
    EXPECT_EQ(find(paged, "m:item:0:0"), nullptr);
    EXPECT_NE(find(paged, "m:item:1:0"), nullptr);
    EXPECT_NE(find(paged, "m:item:2:0"), nullptr);
    EXPECT_EQ(find(paged, "m:item:3:0"), nullptr);
    // Nothing unless the window is open.
    Level opening = Level{};
    opening.animation = ticks(4);
    opening.animation.open();
    EXPECT_TRUE(MenuKit::MenuItems("m", config, opening, styled).empty());
    EXPECT_TRUE(MenuKit::MenuItems("m", config, Level{}, styled).empty());
}

TEST(MenuCursor, IsTheArrowTurnedAtTheCursorsCellKeyedByTheItem) {
    const MenuConfig config = list(3);
    const MenuKit::Look l = look();
    const LayerKit::Sprites sprites = MenuKit::MenuCursor("m", config, openLevel(1), l, 1);
    ASSERT_EQ(sprites.size(), 1u);
    const Sprite& arrow = sprites[0];
    EXPECT_EQ(std::string_view(arrow.key), "m:cursor:1");
    EXPECT_EQ(arrow.x, 8);
    EXPECT_EQ(arrow.y, 18 + 1);  // the cell from 18, 10 high, the arrow 8
    EXPECT_EQ(arrow.z, 1);
    EXPECT_EQ(arrow.tile, l.border.sheet.slots[WindowKit::kArrowSlot].tile);
    EXPECT_EQ(arrow.size, l.border.sheet.slots[WindowKit::kArrowSlot].dimensions);
    EXPECT_EQ(arrow.atlas, l.border.sheet.atlasId);
    EXPECT_EQ(arrow.palette, kBorderColors);
    EXPECT_EQ(arrow.rotation, retropp::Rotation::Rot270);
    EXPECT_FALSE(arrow.flipX || arrow.flipY);
    // A move is a new key.
    Menu menu = opened(config);
    menu.advance(config, press(kDown));
    menu.advance(config, press(kDown));
    EXPECT_EQ(keys(MenuKit::MenuCursor("m", config, menu.levels()[0], l)), (std::vector<std::string>{"m:cursor:2"}));
    // No arrow slot, no cursor; nothing unless open.
    EXPECT_TRUE(MenuKit::MenuCursor("m", config, openLevel(), look(border(8, 9))).empty());
    EXPECT_TRUE(MenuKit::MenuCursor("m", config, Level{}, l).empty());
}

TEST(MenuArrows, SayWhereMoreRowsLieInTheRoomsRightCorners) {
    const MenuKit::Look l = look();
    const MenuConfig pages = list(10, {.rows = 4});
    // The room is 64×32 at (8, 8): arrows at x 64, the up at y 8 and the down at y 32.
    const LayerKit::Sprites first = MenuKit::MenuArrows("m", pages, openLevel(0, 0), l, 1);
    EXPECT_EQ(keys(first), (std::vector<std::string>{"m:down"}));
    EXPECT_EQ(first[0].x, 64);
    EXPECT_EQ(first[0].y, 32);
    EXPECT_EQ(first[0].z, 1);
    EXPECT_FALSE(first[0].flipY);
    EXPECT_EQ(first[0].rotation, retropp::Rotation::None);
    EXPECT_EQ(first[0].tile, l.border.sheet.slots[WindowKit::kArrowSlot].tile);
    const LayerKit::Sprites middle = MenuKit::MenuArrows("m", pages, openLevel(4, 4), l);
    EXPECT_EQ(keys(middle), (std::vector<std::string>{"m:up", "m:down"}));
    EXPECT_EQ(middle[0].x, 64);
    EXPECT_EQ(middle[0].y, 8);
    EXPECT_TRUE(middle[0].flipY);
    EXPECT_EQ(keys(MenuKit::MenuArrows("m", pages, openLevel(8, 8), l)), (std::vector<std::string>{"m:up"}));
    // A menu that fits has none.
    EXPECT_TRUE(MenuKit::MenuArrows("m", list(3), openLevel(), l).empty());
    // Scrolling: by the first row showing.
    const MenuConfig scroll = list(6, {.rows = 3, .overflow = MenuKit::Overflow::Scroll});
    EXPECT_EQ(keys(MenuKit::MenuArrows("m", scroll, openLevel(3, 1), l)), (std::vector<std::string>{"m:up", "m:down"}));
    EXPECT_EQ(keys(MenuKit::MenuArrows("m", scroll, openLevel(5, 3), l)), (std::vector<std::string>{"m:up"}));
    EXPECT_TRUE(MenuKit::MenuArrows("m", pages, openLevel(), look(border(8, 9))).empty());
    EXPECT_TRUE(MenuKit::MenuArrows("m", pages, Level{}, l).empty());
}

// ── Every open menu ──────────────────────────────────────────────────────────────────────────

TEST(MenuWindows, OneLayerPerOpenMenuEachAboveTheOneBeneath) {
    MenuConfig config = tree();
    const MenuKit::Look l = look();
    Menu menu;
    EXPECT_TRUE(MenuKit::MenuWindows("pause", 30, config, menu, l).empty());

    menu.open(config);
    menu.advance(config, press(kDown));
    menu.advance(config, kConfirm);
    const LayerKit::Layers layers = MenuKit::MenuWindows("pause", 30, config, menu, l, {.alpha = 0.5f});
    ASSERT_EQ(layers.size(), 2u);
    EXPECT_EQ(std::string_view(layers[0].key), "pause:0");
    EXPECT_EQ(layers[0].z, 30);
    EXPECT_EQ(std::string_view(layers[1].key), "pause:1");
    EXPECT_EQ(layers[1].z, 31);
    EXPECT_FLOAT_EQ(layers[0].alpha, 0.5f);
    EXPECT_FLOAT_EQ(layers[1].alpha, 0.5f);
    // Each at its own config's box.
    EXPECT_EQ(find(spritesOf(layers[0]), "pause:0:box:top-left:64x48")->x, 0);
    const Sprite* corner = find(spritesOf(layers[1]), "pause:1:box:top-left:64x40");
    ASSERT_NE(corner, nullptr);
    EXPECT_EQ(corner->x, 8);
    EXPECT_EQ(corner->y, 8);
    // Every sprite carries its menu's key, so two menus share none.
    EXPECT_NE(find(spritesOf(layers[0]), "pause:0:cursor:1"), nullptr);
    EXPECT_NE(find(spritesOf(layers[1]), "pause:1:cursor:0"), nullptr);
    EXPECT_NE(find(spritesOf(layers[1]), "pause:1:item:0:0"), nullptr);
    for (const Sprite& sprite : spritesOf(layers[0])) {
        EXPECT_TRUE(std::string_view(sprite.key).starts_with("pause:0:")) << std::string_view(sprite.key);
    }
    for (const Sprite& sprite : spritesOf(layers[1])) {
        EXPECT_TRUE(std::string_view(sprite.key).starts_with("pause:1:")) << std::string_view(sprite.key);
    }

    // A window that moves holds its box alone.
    config.items[1].opens->animation = ticks(4);
    Menu slow = opened(config);
    slow.advance(config, press(kDown));
    slow.advance(config, kConfirm);
    slow.advance(config, {});
    const LayerKit::Layers moving = MenuKit::MenuWindows("title", 40, config, slow, l);
    ASSERT_EQ(moving.size(), 2u);
    ASSERT_FALSE(spritesOf(moving[1]).empty());
    for (const Sprite& sprite : spritesOf(moving[1])) {
        EXPECT_TRUE(std::string_view(sprite.key).starts_with("title:1:box:")) << std::string_view(sprite.key);
    }
}

// ── The cursor, drawn (through a renderer on a GPU device) ──────────────────────────────────────

class CursorOnDevice : public ::testing::Test {
protected:
    static inline SDL_GPUDevice* device_ = nullptr;
    static inline std::string error_;

    static void SetUpTestSuite() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            error_ = std::string("SDL_Init failed: ") + SDL_GetError();
            return;
        }
        device_ = SDL_CreateGPUDevice(
            SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB,
            /*debug_mode=*/false, /*name=*/nullptr);
        if (device_ == nullptr) {
            error_ = std::string("SDL_CreateGPUDevice failed: ") + SDL_GetError();
        }
    }

    static void TearDownTestSuite() {
        if (device_ != nullptr) {
            SDL_DestroyGPUDevice(device_);
            device_ = nullptr;
        }
        SDL_Quit();
    }

    void SetUp() override {
        if (device_ == nullptr) {
            FAIL() << "no GPU device: " << error_ << ". The cursor is read back from a renderer, which needs one.";
        }
    }
};

// A border sheet of ten 8×8 slots whose arrow slot is a triangle pointing down — its base along the
// top row, its point at the bottom of the cell's top half. The cursor turns it to point right: its base
// down the left column, its point at the right of the cell's left half.
TEST_F(CursorOnDevice, TheArrowTurnedAQuarterPointsRight) {
    constexpr int kSize = 8;
    retropp::Renderer renderer{device_, nullptr, retropp::ViewportResolution{kSize, kSize}};
    constexpr std::array<retropp::Rgba8, 3> kColors{{{0, 0, 0, 0}, {220, 30, 30, 255}, {30, 30, 220, 255}}};
    const retropp::PaletteId palette = renderer.uploadPalette(std::span<const retropp::Rgba8>(kColors));

    constexpr int kSheetWidth = 3 * kSize;
    constexpr int kSheetHeight = 4 * kSize;
    std::array<std::uint8_t, kSheetWidth * kSheetHeight> sheet{};
    sheet.fill(0);
    const int arrowTop = 3 * kSize;  // slot 9: the first cell of the fourth row
    for (int row = 0; row < kSize / 2; ++row) {
        for (int column = row; column < kSize - row; ++column) {
            sheet[static_cast<std::size_t>((arrowTop + row) * kSheetWidth + column)] = 1;
        }
    }
    const WindowKit::Border sheetBorder{
        .sheet = renderer.uploadAtlas(sheet.data(), kSheetWidth, kSheetHeight, {.width = kSize, .height = kSize},
                                      retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
                                      static_cast<int>(WindowKit::kArrowSlot) + 1,
                                      retropp::TransparentIndices::of({0})),
        .palette = palette,
    };
    ASSERT_EQ(sheetBorder.sheet.slots.size(), WindowKit::kArrowSlot + 1);

    std::array<std::uint8_t, kSize * kSize> ground{};
    ground.fill(2);
    const retropp::AtlasId groundSheet = renderer.uploadAtlas(ground.data(), kSize, kSize).atlasId;
    std::vector<retropp::TileCell> cells{retropp::TileCell{.atlas = groundSheet, .tile = 0, .palette = palette}};
    retropp::DrawLayer back{.key = "ground"};
    back.z = 0;
    back.size = retropp::PixelSize{kSize, kSize};
    back.content = retropp::TileContent{.widthInTiles = 1, .heightInTiles = 1,
                                        .cells = std::span<const retropp::TileCell>(cells)};

    // A one-item menu whose room is the whole viewport: the cursor's cell is the viewport.
    const MenuConfig config{.box = {.x = -kSize, .y = -kSize, .width = 3 * kSize, .height = 3 * kSize},
                            .items = {{.label = "A"}}};
    Menu menu = opened(config);
    const LayerKit::Sprites cursorSprites =
        MenuKit::MenuCursor("menu", config, menu.levels()[0], {.border = sheetBorder, .font = font()});
    ASSERT_EQ(cursorSprites.size(), 1u);
    EXPECT_EQ(cursorSprites[0].x, 0);
    EXPECT_EQ(cursorSprites[0].y, 0);
    retropp::DrawLayer front{.key = "menu"};
    front.z = 10;
    front.size = retropp::PixelSize{kSize, kSize};
    front.content = retropp::SpriteContent{.sprites = std::span<const retropp::Sprite>(cursorSprites)};

    retropp::FrameDrawState frame;
    frame.layers.push_back(back);
    frame.layers.push_back(front);
    const std::vector<retropp::Rgba8> pixels = renderer.captureViewport(frame);
    ASSERT_EQ(pixels.size(), static_cast<std::size_t>(kSize * kSize));

    const auto isArrow = [](retropp::Rgba8 c) { return c.r > 180 && c.b < 80; };
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const bool expected = x < kSize / 2 && y >= x && y < kSize - x;
            const retropp::Rgba8 c = pixels[static_cast<std::size_t>(y * kSize + x)];
            EXPECT_EQ(isArrow(c), expected) << x << "," << y << ": " << +c.r << " " << +c.g << " " << +c.b;
        }
    }
}

}  // namespace
