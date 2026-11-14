// MenuConfig — a menu as a game declares it: where its window is, how its items are laid out, and the
// items, each of which may open a menu of its own.
//
//     const MenuKit::MenuConfig pause{
//         .box = {.x = 8, .y = 24, .width = 88, .height = 48},
//         .items = {{.label = "RESUME", .value = Choice::Resume},
//                   {.label = "OPTIONS", .opens = MenuKit::MenuConfig{.box = optionsBox, .items = {...}}},
//                   {.label = "QUIT", .value = Choice::Quit}},
//     };
//
// A config is a value the game owns and hands to Menu::advance each tick and to MenuWindows each frame
// (menu.h, menu_windows.h). The header names no draw type and returns none, so a game's simulation can
// hold one.
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "retropp/geometry.h"
#include "retropp/timing.h"
#include "retropp/tween.h"

#include "WindowKit/animation.h"

namespace MenuKit {

// One of the game's own enumerators (or integers), naming an item: reported when the item is chosen.
struct Value {
    std::uint32_t id = 0;

    template <typename E>
        requires std::is_enum_v<E> || std::is_integral_v<E>
    constexpr Value(E value) noexcept : id(static_cast<std::uint32_t>(value)) {}

    [[nodiscard]] constexpr bool operator==(const Value&) const noexcept = default;
};

// What a menu does with more rows of items than it shows at once.
enum class Overflow : std::uint8_t {
    Pages,   // the rows showing are a page; moving past its last row turns to the next
    Scroll,  // the rows showing follow the cursor one row at a time
};

// Moving again while a direction stays held: after `delay`, then once per `every`. Each is a time,
// resolved to ticks through `timing` to the nearest tick, or a count of ticks. With no `every` nothing
// repeats; with no delay the first repeat comes after one `every`.
//
// `timing` is retropp::TweenPlayer<float>::defaultTiming as it stands when the value is built, which
// EngineConfig::setActive sets to the game's own; build the repeat after setActive runs.
struct Repeat {
    std::chrono::nanoseconds delay{};  // how long a direction is held before it repeats
    std::chrono::nanoseconds every{};  // how long between one repeat and the next
    std::uint32_t delayTicks = 0;      // the delay in ticks; not 0, `delay` is not read
    std::uint32_t everyTicks = 0;      // the time between repeats in ticks; not 0, `every` is not read
    retropp::TimingProfile timing = retropp::TweenPlayer<float>::defaultTiming;  // resolves the times to ticks
};

namespace detail {

// The delay in ticks: `delayTicks` when it is not 0, else `delay` through `timing`.
[[nodiscard]] inline std::uint64_t delayLength(const Repeat& repeat) noexcept {
    return repeat.delayTicks != 0 ? repeat.delayTicks : repeat.timing.ticksForDuration(repeat.delay);
}

// The ticks between repeats: `everyTicks` when it is not 0, else `every` through `timing`. 0 is no repeat.
[[nodiscard]] inline std::uint64_t everyLength(const Repeat& repeat) noexcept {
    return repeat.everyTicks != 0 ? repeat.everyTicks : repeat.timing.ticksForDuration(repeat.every);
}

}  // namespace detail

struct Item;

// A menu: where its window is, how its items are laid out, and the items.
//
// Items are laid out on a grid, `columns` across, filled a row at a time: item n is at column
// n % columns of row n / columns. A menu shows `rows` rows at once, or every row it has when `rows` is 0
// or more than it has.
struct MenuConfig {
    retropp::IntRect box{};            // its window, in the pixels of the layer it is drawn on
    int columns = 1;                   // items across; below 1 is read as 1
    int rows = 0;                      // rows showing at once; 0 is every row the items take
    Overflow overflow = Overflow::Pages;
    bool wraps = true;                 // moving past an edge comes round to the other; false stops there
    bool closesOnCancel = true;        // read on the first menu: false is a menu cancel does not close
    Repeat repeat{};                   // moving again while a direction is held; none unless given a length
    WindowKit::Animation animation{};  // how its window opens and closes; instant unless given a length
    std::vector<Item> items;
};

// One item: what it says, what the game calls it, and the menu it opens, if it opens one. An item that
// opens a menu with items opens it on confirm, and its value is not reported; an item that opens an empty
// menu, or that has neither a menu nor a value, does nothing on confirm.
struct Item {
    std::string label;                 // drawn in the menu's font; empty draws nothing
    std::optional<Value> value;        // reported as Step::chosen when the item is chosen
    bool enabled = true;               // false: the cursor lands on it and confirm is refused
    std::optional<MenuConfig> opens;   // the menu confirm opens over this one
};

}  // namespace MenuKit
