// The open menus drawn: each as a WindowKit window holding its items' labels, the cursor and the
// arrows that say more rows lie above or below.
//
//     const MenuKit::Look look{.border = plainBorder, .font = font, .selected = gold, .disabled = gray};
//     for (retropp::DrawLayer& layer : MenuKit::MenuWindows("pause", kMenuZ, config.pause, state.pause, look)) {
//         frame.layers.push_back(std::move(layer));
//     }
//
// This is the one MenuKit header that builds draw types. The sprites are in the pixels of the layer the
// menus are drawn on, the pixels each MenuConfig::box is in. The arrow is the border sheet's
// WindowKit::kArrowSlot, the slot a message box shows its arrow from, drawn as the sheet has it — so it
// points down — flipped to point up, and turned a quarter to point right as the cursor.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "retropp/atlas_manifest.h"
#include "retropp/draw_state.h"
#include "retropp/geometry.h"

#include "LayerKit/layer_options.h"
#include "LayerKit/types.h"

#include "TextKit/font.h"
#include "TextKit/text.h"

#include "WindowKit/animate.h"
#include "WindowKit/animation.h"
#include "WindowKit/border.h"
#include "WindowKit/window.h"

#include "MenuKit/menu.h"
#include "MenuKit/menu_config.h"

namespace MenuKit {

// How a menu is drawn.
struct Look {
    WindowKit::Border border;                  // each menu's window, its cursor and its arrows
    TextKit::Font font;                        // an item's label
    std::optional<TextKit::Font> selected;     // the label under the cursor; none draws it in `font`
    std::optional<TextKit::Font> disabled;     // a disabled item's label; none draws it in `font`
    WindowKit::Style style{};                  // how each window opens and closes
};

namespace detail {

// The border's arrow slot, or nullptr for a sheet without one.
[[nodiscard]] inline const retropp::AssetSlot* arrowOf(const WindowKit::Border& border) noexcept {
    return border.sheet.slots.size() > WindowKit::kArrowSlot ? &border.sheet.slots[WindowKit::kArrowSlot] : nullptr;
}

// Whether a menu's contents are drawn: its window is open and it has items.
[[nodiscard]] inline bool contentsShown(const MenuConfig& config, const Level& level) noexcept {
    return level.animation.phase() == WindowKit::Phase::Open && !config.items.empty();
}

}  // namespace detail

// The labels of the items among the rows showing, each through TextKit::Text keyed "<key>:item:<n>" at
// `z`: in the look's `disabled` font for a disabled item, else its `selected` font for the item under
// the cursor, else its `font` — each falling back to `font` when the look has none. A label starts the
// arrow slot's width in from its cell's left edge (at the edge for a sheet without that slot), and is
// centered down the cell, its measured height taken from the cell's (integer division; never above the
// cell's top). An empty label draws nothing. Nothing unless the window is open. `config` is the
// level's own (configOf).
[[nodiscard]] inline LayerKit::Sprites MenuItems(std::string_view key, const MenuConfig& config, const Level& level,
                                                 const Look& look, std::int32_t z = 0) {
    if (!detail::contentsShown(config, level)) {
        return {};
    }
    const retropp::AssetSlot* arrow = detail::arrowOf(look.border);
    const int indent = arrow != nullptr ? arrow->dimensions.width : 0;
    const std::size_t cursor = detail::cursorIn(config, level);
    const std::size_t columns = detail::columnsOf(config);
    const std::size_t first = level.top * columns;
    const std::size_t end = std::min((level.top + detail::rowsShowing(config)) * columns, config.items.size());

    LayerKit::Sprites sprites;
    for (std::size_t n = first; n < end; ++n) {
        const Item& item = config.items[n];
        if (item.label.empty()) {
            continue;
        }
        const TextKit::Font& font = !item.enabled ? look.disabled.value_or(look.font)
                                    : n == cursor ? look.selected.value_or(look.font)
                                                  : look.font;
        const retropp::IntRect cell = itemBox(config, level, look.border, n);
        const int height = TextKit::measure(item.label, font).height;
        const retropp::Vec2i at{.x = cell.x + indent, .y = cell.y + std::max(0, (cell.height - height) / 2)};
        LayerKit::Sprites label =
            TextKit::Text(std::string(key) + ":item:" + std::to_string(n), item.label, at, font, {.z = z});
        sprites.insert(sprites.end(), std::make_move_iterator(label.begin()), std::make_move_iterator(label.end()));
    }
    return sprites;
}

// The cursor: the border's arrow turned a quarter to point right (retropp::Rotation::Rot270), at its
// slot's size through the border's palette, against the left edge of the cursor's cell and centered down
// it (integer division), at `z`. Keyed "<key>:cursor:<item>", so on the tick the cursor moves it is a new
// sprite, put straight where it is told. Nothing unless the window is open, and nothing for a sheet
// without the arrow slot.
[[nodiscard]] inline LayerKit::Sprites MenuCursor(std::string_view key, const MenuConfig& config, const Level& level,
                                                  const Look& look, std::int32_t z = 0) {
    const retropp::AssetSlot* arrow = detail::arrowOf(look.border);
    if (!detail::contentsShown(config, level) || arrow == nullptr) {
        return {};
    }
    const std::size_t cursor = detail::cursorIn(config, level);
    const retropp::IntRect cell = itemBox(config, level, look.border, cursor);
    if (cell.width == 0 && cell.height == 0) {
        return {};
    }
    return {retropp::Sprite{
        .key = std::string(key) + ":cursor:" + std::to_string(cursor),
        .x = cell.x,
        .y = cell.y + (cell.height - arrow->dimensions.height) / 2,
        .z = z,
        .size = arrow->dimensions,
        .atlas = look.border.sheet.atlasId,
        .tile = arrow->tile,
        .palette = look.border.palette,
        .rotation = retropp::Rotation::Rot270,
    }};
}

// The arrows that say more rows lie beyond the ones showing, at `z`: the arrow flipped to point up
// (flipY), keyed "<key>:up", in the top-right corner of the room the border leaves, while rows lie above
// (Level::top above 0); the arrow as the sheet has it, keyed "<key>:down", in the room's bottom-right
// corner, while rows lie below. Each at its slot's size through the border's palette. Nothing unless the
// window is open, and nothing for a sheet without the arrow slot.
[[nodiscard]] inline LayerKit::Sprites MenuArrows(std::string_view key, const MenuConfig& config, const Level& level,
                                                  const Look& look, std::int32_t z = 0) {
    const retropp::AssetSlot* arrow = detail::arrowOf(look.border);
    if (!detail::contentsShown(config, level) || arrow == nullptr) {
        return {};
    }
    const retropp::IntRect room = WindowKit::inside(config.box, look.border);
    const int x = room.x + room.width - arrow->dimensions.width;
    const auto sprite = [&](std::string_view name, int y, bool up) {
        return retropp::Sprite{
            .key = std::string(key) + ":" + std::string(name),
            .x = x,
            .y = y,
            .z = z,
            .size = arrow->dimensions,
            .atlas = look.border.sheet.atlasId,
            .tile = arrow->tile,
            .palette = look.border.palette,
            .flipY = up,
        };
    };
    LayerKit::Sprites sprites;
    if (level.top > 0) {
        sprites.push_back(sprite("up", room.y, true));
    }
    if (level.top + detail::rowsShowing(config) < detail::rowCount(config)) {
        sprites.push_back(sprite("down", room.y + room.height - arrow->dimensions.height, false));
    }
    return sprites;
}

// Every open menu, one layer each, the first at the bottom: menu i as WindowKit::Window keyed
// "<key>:<i>" at `z + i`, over its config's box in the look's border, opening and closing with its
// level's animation in the look's style, and once open holding MenuItems, then MenuCursor and MenuArrows
// one above them — each keyed by the layer's key, so no two menus share a sprite key. `options` goes onto
// every layer. No layers while nothing is showing.
//
// A game calls it once per key per frame, as every LayerKit layer is, and leaves the depths from `z` up
// free for as many menus as its deepest path through the tree opens. Throws std::out_of_range when
// `root` no longer holds the menus open (configOf).
[[nodiscard]] inline LayerKit::Layers MenuWindows(std::string_view key, std::int32_t z, const MenuConfig& root,
                                                  const Menu& menu, const Look& look,
                                                  LayerKit::LayerOptions options = {}) {
    LayerKit::Layers layers;
    const std::span<const Level> levels = menu.levels();
    for (std::size_t i = 0; i < levels.size(); ++i) {
        const MenuConfig& config = configOf(root, menu, i);
        const Level& level = levels[i];
        const std::string layerKey = std::string(key) + ":" + std::to_string(i);
        layers.push_back(WindowKit::Window(layerKey, z + static_cast<std::int32_t>(i), config.box, look.border,
                                           level.animation, look.style,
                                           {MenuItems(layerKey, config, level, look),
                                            MenuCursor(layerKey, config, level, look, 1),
                                            MenuArrows(layerKey, config, level, look, 1)},
                                           options));
    }
    return layers;
}

}  // namespace MenuKit
