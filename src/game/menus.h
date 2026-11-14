// The game's menus: the title menu and the pause menu, every menu opened from them, and the places the
// pause menu's WARP puts the player.
//
//     const MenuKit::MenuConfig title = zorlduh::titleMenu(kMenuBoxes, animation);
//     if (step.chosen == MenuKit::Value{zorlduh::MenuItem::Start}) { /* begin play */ }
//
// What each menu holds is decided here; what a choice does is the tick's (sim.h). The header names no
// draw type, so the simulation holds the menus.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "retropp/geometry.h"

#include "MenuKit/menu_config.h"
#include "WindowKit/animation.h"

#include "game/areas.h"

namespace zorlduh {

// What an item of the game's menus is called, reported when it is chosen. A warp's value is Warp plus
// its place in warps().
enum class MenuItem : std::uint32_t {
    Start,
    Continue,
    Resume,
    Title,
    TextSlow,
    TextNormal,
    TextFast,
    StyleGrow,
    StyleClip,
    StyleSquash,
    StyleFade,
    StyleSlide,
    StylePop,
    Warp = 100,
};

// Where each menu's window is, on screen.
struct MenuBoxes {
    retropp::IntRect title;    // START, CONTINUE, OPTIONS
    retropp::IntRect pause;    // RESUME, OPTIONS, WARP, TITLE
    retropp::IntRect options;  // TEXT, WINDOWS
    retropp::IntRect text;     // how fast a message types
    retropp::IntRect styles;   // how the menus' windows open and close
    retropp::IntRect warp;     // every area, a page at a time
};

// An area the player can be put in.
struct Warp {
    Layout layout;
    int area;
};

// Every area of every layout, layout by layout and each in its own order: seventeen.
[[nodiscard]] std::span<const Warp> warps() noexcept;

// The place in warps() of the warp `value` names, or std::nullopt when it names none.
[[nodiscard]] std::optional<std::size_t> warpOf(MenuKit::Value value) noexcept;

// Every menu below opens and closes with `animation`, in the box of its name in `boxes`, and repeats a
// held direction after kMenuRepeatDelay, then every kMenuRepeatEvery (sim.h). OPTIONS, in both the title
// and the pause menu, opens TEXT — SLOW, NORMAL, FAST — and WINDOWS — GROW, CLIP, SQUASH, FADE, SLIDE,
// POP, two across with two rows showing, scrolling and not wrapping.

// The title menu: START, CONTINUE — disabled — and OPTIONS. Cancel does not close it.
[[nodiscard]] MenuKit::MenuConfig titleMenu(const MenuBoxes& boxes, const WindowKit::Animation& animation);

// The pause menu: RESUME, OPTIONS, WARP — one item per area, named as the HUD names it, five rows a page
// — and TITLE.
[[nodiscard]] MenuKit::MenuConfig pauseMenu(const MenuBoxes& boxes, const WindowKit::Animation& animation);

}  // namespace zorlduh
