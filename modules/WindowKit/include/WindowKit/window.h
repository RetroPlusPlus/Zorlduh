// Window — a box and whatever a game puts inside it, as one sprite layer.
//
//     const retropp::IntRect room = WindowKit::inside(box, border);
//     frame.layers.push_back(WindowKit::Window("status", kHudZ, box, border,
//                                              {TextKit::Text("status:name", "HERO", {room.x, room.y}, font)}));
//
// The children are placed by the game, in the layer's pixels; inside(box, border) is the room the border
// leaves. The layer is LayerKit's SpriteLayer, so it is called once per key per frame, as every
// LayerKit layer is. A window that opens and closes over time takes its Animation and a Style:
//
//     WindowKit::Window("menu", kMenuZ, box, border, state.menu, WindowKit::Style::grow(), {Items(...)});
#pragma once

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "retropp/draw_state.h"
#include "retropp/geometry.h"

#include "LayerKit/layer_options.h"
#include "LayerKit/sprite_layer.h"
#include "LayerKit/types.h"

#include "WindowKit/animate.h"
#include "WindowKit/animation.h"
#include "WindowKit/border.h"
#include "WindowKit/box.h"

namespace WindowKit {

// A window as one sprite layer keyed `key` at `z`: the box, keyed "<key>:box", then each child in
// order. The box's sprites are at the lowest z there is, so the box is beneath every child whatever
// the child's own z. Every option passes onto the layer untouched.
[[nodiscard]] inline retropp::DrawLayer Window(std::string_view key, std::int32_t z, retropp::IntRect box,
                                               const Border& border,
                                               std::initializer_list<LayerKit::Sprites> children,
                                               LayerKit::LayerOptions options = {}) {
    LayerKit::Sprites all = Box(std::string(key) + ":box", box, border, std::numeric_limits<std::int32_t>::min());
    for (const LayerKit::Sprites& child : children) {
        all.insert(all.end(), child.begin(), child.end());
    }
    return LayerKit::SpriteLayer(key, z, {all}, std::move(options));
}

// A window with an animation: while it opens or closes, the box alone drawn in `style` for where
// `animation` is (Animate); once open, the box and the children, as the plain Window; while closed, a
// sprite layer keyed `key` at `z` with no sprites, so a game adds it every frame.
//
// At progress 0 — an opening's first tick — the layer carries the style's options at progress 0 and no
// sprites. The engine eases a layer's scroll, alpha and transform from one tick to the next by its key,
// so on that tick it eases from whatever the layer was before, closed or in another style; with nothing
// on the layer, none of it shows, and from the next tick the box eases on from the style's start.
[[nodiscard]] inline retropp::DrawLayer Window(std::string_view key, std::int32_t z, retropp::IntRect box,
                                               const Border& border, const Animation& animation, const Style& style,
                                               std::initializer_list<LayerKit::Sprites> children,
                                               LayerKit::LayerOptions options = {}) {
    switch (animation.phase()) {
        case Phase::Closed:
            return LayerKit::SpriteLayer(key, z, {}, std::move(options));
        case Phase::Open:
            return Window(key, z, box, border, children, std::move(options));
        case Phase::Opening:
        case Phase::Closing:
            break;
    }
    Animated drawn = Animate(box, animation, style, std::move(options));
    if (animation.progress() <= 0.0f) {
        return LayerKit::SpriteLayer(key, z, {}, std::move(drawn.options));
    }
    return Window(key, z, drawn.box, border, {}, std::move(drawn.options));
}

}  // namespace WindowKit
