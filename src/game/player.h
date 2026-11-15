// The player: an input walker and the position it walks, kept together in one value; and what the
// player is as a walker — how fast it moves, what of it collides, and its walk animations.
//
//     zorlduh::Player player{zorlduh::kPlayerStart};
//     const TopDownMovement::WalkerConfig config = zorlduh::PlayerConfig(assets);
//     player.walk(config, input.move, solids);   // once per tick
#pragma once

#include "retropp/geometry.h"
#include "retropp/input_actions.h"

#include "assets/assets.h"
#include "TopDownMovement/input_walker.h"
#include "TopDownMovement/walker.h"

namespace zorlduh {

// Where the player starts: its 16×16 frame centered in the start room.
inline constexpr retropp::Vec2i kPlayerStart{.x = 72, .y = 56};

// The player: an input walker bound to the position it holds. A copy holds its own position and walks
// it, so the game's state copies whole.
//
// The component that draws the player (render/player.h) is the function zorlduh::Player, which hides
// this type's name wherever both are seen; a player is then written as a braced value,
// `.player = {kPlayerStart}`, which is why the constructor below is not explicit.
struct Player : TopDownMovement::InputWalker {
    retropp::Vec2i at;   // the player's top-left, in the current area's pixels

    // A player standing at (0, 0), facing down.
    Player();

    // A player standing at `start`, facing `facing`.
    Player(retropp::Vec2i start, retropp::Dir facing = retropp::Dir::Down);

    Player(const Player& other);
    Player& operator=(const Player& other);
};

// One pixel per tick on each axis held; the collision box is the bottom half of the 16×16 frame, 3 px
// in from each side; the walk animations are the player sheet's rows.
[[nodiscard]] TopDownMovement::WalkerConfig PlayerConfig(const Assets& assets);

}  // namespace zorlduh
