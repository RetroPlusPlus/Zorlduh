// Window — a box and whatever a game puts inside it, as one sprite layer.
//
//     const retropp::IntRect room = WindowKit::inside(box, border);
//     frame.layers.push_back(WindowKit::Window("status", kHudZ, box, border,
//                                              {TextKit::Text("status:name", "HERO", {room.x, room.y}, font)}));
//
// The children are placed by the game, in the layer's pixels; inside(box, border) is the room the border
// leaves. The layer is LayerKit's SpriteLayer, so it is called once per key per frame, as every
// LayerKit layer is.
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

}  // namespace WindowKit
