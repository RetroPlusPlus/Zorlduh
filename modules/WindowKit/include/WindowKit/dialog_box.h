// The message box — a Dialog drawn: the page up, typed as far as it has got, and the arrow, in a
// window.
//
//     frame.layers.push_back(WindowKit::DialogBox("message", kMessageZ, box, border, font, state.dialog));
//
// Hand Dialog::open the size of inside(box, border) for the same box and border, so the pages the
// dialog cut are the pages the box has room for.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "retropp/draw_state.h"
#include "retropp/geometry.h"

#include "LayerKit/layer_options.h"
#include "LayerKit/sprite_layer.h"

#include "TextKit/font.h"
#include "TextKit/text.h"

#include "WindowKit/animate.h"
#include "WindowKit/animation.h"
#include "WindowKit/border.h"
#include "WindowKit/dialog.h"
#include "WindowKit/window.h"

namespace WindowKit {

// The page up as sprites, typed as far as the dialog has got: TextKit::Text keyed "<key>:<page>" — so
// each character "<key>:<page>:<n>", and a new page's glyphs are new sprites — laid out from `at`.
// Nothing unless the dialog's box is open.
[[nodiscard]] inline std::vector<retropp::Sprite> DialogText(std::string_view key, const Dialog& dialog,
                                                             retropp::Vec2i at, const TextKit::Font& font,
                                                             std::int32_t z = 0) {
    if (!dialog.showing() || dialog.animation.phase() != Phase::Open) {
        return {};
    }
    return TextKit::Text(std::string(key) + ":" + std::to_string(dialog.page()), dialog.text(), at, font,
                         {.shown = dialog.shown(), .z = z});
}

// The arrow, its bottom-right corner at the bottom-right corner of `area`, keyed "<key>:arrow:<page>":
// the border sheet's kArrowSlot, at its slot's size, through the border's palette. Nothing unless the
// dialog's box is open, dialog.arrowOn() and the sheet has that slot.
[[nodiscard]] inline std::vector<retropp::Sprite> DialogArrow(std::string_view key, const Dialog& dialog,
                                                              retropp::IntRect area, const Border& border,
                                                              std::int32_t z = 0) {
    if (dialog.animation.phase() != Phase::Open || !dialog.arrowOn() || border.sheet.slots.size() <= kArrowSlot) {
        return {};
    }
    const retropp::AssetSlot& slot = border.sheet.slots[kArrowSlot];
    return {retropp::Sprite{
        .key = std::string(key) + ":arrow:" + std::to_string(dialog.page()),
        .x = area.x + area.width - slot.dimensions.width,
        .y = area.y + area.height - slot.dimensions.height,
        .z = z,
        .size = slot.dimensions,
        .atlas = border.sheet.atlasId,
        .tile = slot.tile,
        .palette = border.palette,
    }};
}

// The message box: a Window over `box` holding the page and the arrow in inside(box, border), the
// arrow above the text, opening and closing with the dialog's animation in `style` — grow from the
// box's middle unless given another. While the box opens or closes it is the box alone, and nothing on
// the opening's first tick (window.h). While no message is showing it is a sprite layer keyed `key` at
// `z` with no sprites, so a game adds it every frame.
[[nodiscard]] inline retropp::DrawLayer DialogBox(std::string_view key, std::int32_t z, retropp::IntRect box,
                                                  const Border& border, const TextKit::Font& font,
                                                  const Dialog& dialog, const Style& style = {},
                                                  LayerKit::LayerOptions options = {}) {
    if (!dialog.showing()) {
        return LayerKit::SpriteLayer(key, z, {}, std::move(options));
    }
    const retropp::IntRect area = inside(box, border);
    return Window(key, z, box, border, dialog.animation, style,
                  {DialogText(key, dialog, {.x = area.x, .y = area.y}, font),
                   DialogArrow(key, dialog, area, border, 1)},
                  std::move(options));
}

}  // namespace WindowKit
