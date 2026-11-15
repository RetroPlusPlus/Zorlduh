// Routes — a walker moved along the engine's SpritePath.
//
//     retropp::SpritePath patrol = TopDownMovement::routeFrom(guard, {
//         {.label = "east", .move = retropp::SpritePathMove::to({136.0f, 120.0f}), .pacing = retropp::PathPacing::speed(30.0f)},
//         {.label = "wait", .move = retropp::SpritePathMove::to({136.0f, 120.0f}), .pacing = retropp::PathPacing::eased(500ms)},
//         {.label = "west", .move = retropp::SpritePathMove::to({16.0f, 120.0f}),  .pacing = retropp::PathPacing::speed(30.0f)},
//     });
//
//     TopDownMovement::follow(guard, guardConfig, patrol);   // once per tick
//
// The route is a retropp::SpritePath the game keeps in its state. The engine's pacing, wait and
// sentinel nodes, playback modes and interrupt stack all apply, through SpritePath's own members. This
// header adds the walker on the route: its position, a facing chosen from the route's travel, and its
// walk animation. A walker on a route goes exactly where the route goes, walls included.
#pragma once

#include <cmath>
#include <utility>
#include <vector>

#include "retropp/animation.h"
#include "retropp/geometry.h"
#include "retropp/sprite_path.h"

#include "TopDownMovement/walker.h"

namespace TopDownMovement {

// A route over `nodes` that starts at `walker`'s position: a SpritePath whose `start` is that position,
// resolved once so that route.position() equals it before the first follow. The route takes
// SpritePath::defaultTiming as it stands at this call.
[[nodiscard]] inline retropp::SpritePath routeFrom(const Walker& walker, std::vector<retropp::SpritePathNode> nodes) {
    const retropp::Vec2i at = walker.position();
    retropp::SpritePath route{.nodes = std::move(nodes),
                              .start = retropp::Vec2{static_cast<float>(at.x), static_cast<float>(at.y)}};
    route.advance(retropp::PlaybackMode::single(), 0);
    return route;
}

// One tick on `route`: advances it once under `mode`, the playback mode SpritePath::advance takes for
// the route's nodes, and moves `walker` to the route's position, rounded with std::lround. The walker
// faces along the distance the route moved this tick (Walker::moveTo). Its walk animation advances on
// every tick the route moves, however little, and shows frame 0 on a tick it stands still. Call it once
// per tick in place of route.advance().
inline void follow(Walker& walker, const WalkerConfig& config, retropp::SpritePath& route,
                   retropp::PlaybackMode mode = retropp::PlaybackMode::loopIndefinitely()) {
    const retropp::Vec2 before = route.position();
    route.advance(mode);
    const retropp::Vec2 after = route.position();
    walker.moveTo(config,
                  retropp::Vec2i{.x = static_cast<int>(std::lround(after.x)), .y = static_cast<int>(std::lround(after.y))},
                  retropp::Vec2{after.x - before.x, after.y - before.y});
}

}  // namespace TopDownMovement
