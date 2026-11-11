// The simulation: the game's whole state and the tick that advances it. The tick changes the state;
// everything on screen only reads it.
//
//     zorlduh::GameState state{.world = world, .player = TopDownMovement::Walker{zorlduh::kPlayerStart}};
//     zorlduh::tick(state, zorlduh::readInput(in), player);   // once per tick
#pragma once

#include "game/world.h"
#include "TopDownMovement/directions.h"
#include "TopDownMovement/walker.h"

namespace zorlduh {

// What the player asked for this tick.
struct Input {
    TopDownMovement::DirectionalInput move{};  // the directions held and pressed
};

struct GameState {
    World world;
    TopDownMovement::Walker player;
};

// One tick: the player walks the world as `player` describes.
void tick(GameState& state, const Input& input, const TopDownMovement::WalkerConfig& player);

}  // namespace zorlduh
