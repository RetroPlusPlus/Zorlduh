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
// The route is the game's own retropp::SpritePath, kept in its state: the engine's pacing, wait and
// sentinel nodes, sequence modes and interrupt stack all apply, through the engine's own surface. What
// this header adds is the walker on it: its bound position, the facing chosen from the route's travel,
// and the walk animation's stride. Nothing collides on a route.
#pragma once

#include <cmath>
#include <utility>
#include <vector>

#include "retropp/animation.h"
#include "retropp/geometry.h"
#include "retropp/sprite_path.h"

#include "TopDownMovement/walker.h"

namespace TopDownMovement {

// A route over `nodes` that departs from where `walker` stands: a SpritePath whose `start` is the
// walker's position, resolved at its first tick so that its position is the walker's before the first
// follow.
[[nodiscard]] inline retropp::SpritePath routeFrom(const Walker& walker, std::vector<retropp::SpritePathNode> nodes) {
    const retropp::Vec2i at = walker.position();
    retropp::SpritePath route{.nodes = std::move(nodes),
                              .start = retropp::Vec2{static_cast<float>(at.x), static_cast<float>(at.y)}};
    route.advance(retropp::PlaybackMode::single(), 0);
    return route;
}

// One tick on `route`: advances it once under `mode` — the playback mode of its own nodes, as
// SpritePath::advance takes it — and moves `walker` to where it is now, its float position rounded with
// std::lround, facing along how far the route moved this tick (Walker::moveTo). So the walker walks while
// the route moves, however slowly, and stands on frame 0 through a wait node.
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
