#include "game/menus.h"

#include <array>
#include <string>
#include <utility>

#include "game/sim.h"

namespace zorlduh {

namespace {

constexpr std::array<Warp, 17> kWarps{{
    {Layout::Overworld, Area::Meadow},
    {Layout::Overworld, Area::Lake},
    {Layout::Overworld, Area::Field},
    {Layout::Overworld, Area::Shrine},
    {Layout::Overworld, Area::Cave},
    {Layout::DungeonOne, Room::Entrance},
    {Layout::DungeonOne, Room::Hall},
    {Layout::DungeonOne, Room::WestWing},
    {Layout::DungeonOne, Room::EastWing},
    {Layout::DungeonOne, Room::Treasury},
    {Layout::SeamlessForest, Woods::Edge},
    {Layout::SeamlessForest, Woods::Glade},
    {Layout::SeamlessForest, Woods::Thicket},
    {Layout::SeamlessForest, Woods::Bower},
    {Layout::SeamlessForest, Woods::Hollow},
    {Layout::SeamlessForest, Woods::Brook},
    {Layout::SeamlessForest, Woods::Clearing},
}};

// A menu in `box` that opens and closes with `animation` and repeats as every menu of the game does.
MenuKit::MenuConfig menu(retropp::IntRect box, const WindowKit::Animation& animation) {
    return MenuKit::MenuConfig{
        .box = box,
        .repeat = {.delay = kMenuRepeatDelay, .every = kMenuRepeatEvery},
        .animation = animation,
    };
}

MenuKit::MenuConfig optionsMenu(const MenuBoxes& boxes, const WindowKit::Animation& animation) {
    MenuKit::MenuConfig text = menu(boxes.text, animation);
    text.items = {
        {.label = "SLOW", .value = MenuItem::TextSlow},
        {.label = "NORMAL", .value = MenuItem::TextNormal},
        {.label = "FAST", .value = MenuItem::TextFast},
    };

    MenuKit::MenuConfig styles = menu(boxes.styles, animation);
    styles.columns = 2;
    styles.rows = 2;
    styles.overflow = MenuKit::Overflow::Scroll;
    styles.wraps = false;
    styles.items = {
        {.label = "GROW", .value = MenuItem::StyleGrow},
        {.label = "CLIP", .value = MenuItem::StyleClip},
        {.label = "SQUASH", .value = MenuItem::StyleSquash},
        {.label = "FADE", .value = MenuItem::StyleFade},
        {.label = "SLIDE", .value = MenuItem::StyleSlide},
        {.label = "POP", .value = MenuItem::StylePop},
    };

    MenuKit::MenuConfig options = menu(boxes.options, animation);
    options.items = {
        {.label = "TEXT", .opens = std::move(text)},
        {.label = "WINDOWS", .opens = std::move(styles)},
    };
    return options;
}

MenuKit::MenuConfig warpMenu(const MenuBoxes& boxes, const WindowKit::Animation& animation) {
    MenuKit::MenuConfig warp = menu(boxes.warp, animation);
    warp.rows = 5;
    warp.overflow = MenuKit::Overflow::Pages;
    for (std::size_t n = 0; n < kWarps.size(); ++n) {
        warp.items.push_back(MenuKit::Item{
            .label = std::string(areaName(kWarps[n].layout, kWarps[n].area)),
            .value = static_cast<std::uint32_t>(MenuItem::Warp) + static_cast<std::uint32_t>(n),
        });
    }
    return warp;
}

}  // namespace

std::span<const Warp> warps() noexcept {
    return kWarps;
}

std::optional<std::size_t> warpOf(MenuKit::Value value) noexcept {
    const auto first = static_cast<std::uint32_t>(MenuItem::Warp);
    if (value.id < first || value.id - first >= kWarps.size()) {
        return std::nullopt;
    }
    return value.id - first;
}

MenuKit::MenuConfig titleMenu(const MenuBoxes& boxes, const WindowKit::Animation& animation) {
    MenuKit::MenuConfig title = menu(boxes.title, animation);
    title.closesOnCancel = false;
    title.items = {
        {.label = "START", .value = MenuItem::Start},
        {.label = "CONTINUE", .value = MenuItem::Continue, .enabled = false},
        {.label = "OPTIONS", .opens = optionsMenu(boxes, animation)},
    };
    return title;
}

MenuKit::MenuConfig pauseMenu(const MenuBoxes& boxes, const WindowKit::Animation& animation) {
    MenuKit::MenuConfig pause = menu(boxes.pause, animation);
    pause.items = {
        {.label = "RESUME", .value = MenuItem::Resume},
        {.label = "OPTIONS", .opens = optionsMenu(boxes, animation)},
        {.label = "WARP", .opens = warpMenu(boxes, animation)},
        {.label = "TITLE", .value = MenuItem::Title},
    };
    return pause;
}

}  // namespace zorlduh
