// The player as a walker: how fast it moves, what of it collides, and its walk animations.
//
//     const TopDownMovement::WalkerConfig player = zorlduh::PlayerConfig(assets);
#pragma once

#include "retropp/geometry.h"

#include "assets/assets.h"
#include "TopDownMovement/walker.h"

namespace zorlduh {

// Where the player starts: its 16×16 frame centered in the start room.
inline constexpr retropp::Vec2i kPlayerStart{.x = 72, .y = 56};

// One pixel per tick on each axis held; the collision box is the bottom half of the 16×16 frame, 3 px
// in from each side; the walk animations are the player sheet's rows.
[[nodiscard]] TopDownMovement::WalkerConfig PlayerConfig(const Assets& assets);

}  // namespace zorlduh
