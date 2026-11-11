// WalkerSprite — the sprite a walker is.
//
//     LayerKit::SpriteLayer("actors", kActorsZ, {{TopDownMovement::WalkerSprite("hero", walker, hero)}},
//                         {.scroll = camera});
//
// The sprite shows the walker's current frame (its art, size and palette) with the frame's top-left at
// the walker's position, in the world pixels a world-scrolled sprite layer places sprites in. Its `z`
// is the bottom edge of the walker's collision shape, so on a layer of walkers the one standing lower
// draws in front. The config's anchors are published on the sprite.
#pragma once

#include <stdexcept>
#include <utility>

#include "retropp/animation.h"
#include "retropp/draw_state.h"
#include "retropp/object_key.h"

#include "TopDownMovement/walker.h"

namespace TopDownMovement {

// The sprite for `walker` under `config`, keyed `key`. Throws std::logic_error when the frame names no
// art, and as Walker::frame does.
[[nodiscard]] inline retropp::Sprite WalkerSprite(retropp::ObjectKey key, const Walker& walker,
                                                  const WalkerConfig& config) {
    const retropp::AnimationFrame& frame = walker.frame(config);
    if (!frame.hasArt()) {
        throw std::logic_error("WalkerSprite: the walker's frame names no art");
    }
    const retropp::Vec2i position = walker.position();
    const retropp::IntRect bounds = config.collision.bounds();
    const int bottom = config.collision.empty() ? frame.size().height : bounds.y + bounds.height;
    return retropp::Sprite{
        .key = std::move(key),
        .x = position.x,
        .y = position.y,
        .z = position.y + bottom,
        .size = frame.size(),
        .atlas = frame.atlas(),
        .tile = frame.tile(),
        .palette = frame.palette,
        .anchors = config.anchors,
    };
}

}  // namespace TopDownMovement
