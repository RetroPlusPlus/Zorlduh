// SpriteLayer — gathers sprite components onto one screen-sized sprite layer.
//
//     frame.layers.push_back(SpriteLayer("actors", kActorsZ, {Player(...), Enemies(...)},
//                                        {.scroll = {cameraX, cameraY - 16}}));
//
// The children are declared as a list and drawn in that order; a sprite's own z orders it within the
// layer before that. The layer is the size of the active viewport, read from
// retropp::Renderer::defaultViewport on every call. Everything in `options` passes straight onto the
// layer.
//
// The layer's sprites are a span the renderer reads while it draws, so SpriteLayer keeps its own copy
// under `key` until the next call with that key replaces it. Each key is called once per frame: a
// second call with the same key replaces what the first kept, and the layer the first call returned
// can no longer be drawn. Calls with different keys never disturb each other. The engine already
// requires a layer's key to be unique within a frame.
#pragma once

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "retropp/draw_state.h"
#include "retropp/renderer.h"

#include "LayerKit/layer_options.h"
#include "LayerKit/types.h"

namespace LayerKit {

namespace detail {

// The sprites each sprite layer was last handed, by layer key. One table for the whole program.
inline std::map<std::string, Sprites, std::less<>>& keptSprites() {
    static std::map<std::string, Sprites, std::less<>> kept;
    return kept;
}

}  // namespace detail

[[nodiscard]] inline retropp::DrawLayer SpriteLayer(std::string_view key, std::int32_t z,
                                                    std::initializer_list<Sprites> children,
                                                    LayerOptions options = {}) {
    auto& kept = detail::keptSprites();
    auto found = kept.find(key);
    if (found == kept.end()) {
        found = kept.emplace(std::string{key}, Sprites{}).first;
    }
    Sprites& sprites = found->second;
    sprites.clear();
    for (const Sprites& child : children) {
        sprites.insert(sprites.end(), child.begin(), child.end());
    }

    return retropp::DrawLayer{
        .key = key,
        .z = z,
        .size = retropp::Renderer::defaultViewport.size(),
        .scroll = options.scroll,
        .alpha = options.alpha,
        .blend = options.blend,
        .content = retropp::SpriteContent{.sprites = std::span<const retropp::Sprite>(sprites)},
        .effects = std::move(options.effects),
        .regions = std::move(options.regions),
        .transform = options.transform,
        .transformEdge = options.transformEdge,
        .advancesEvery = options.advancesEvery,
    };
}

}  // namespace LayerKit
