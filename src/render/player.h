// The player on screen: its walker's sprite, keyed "player", in the current area's pixels, for the
// sprite layer at the area's actor slot.
//
//     WorldKit::Actors(state.camera, world, kActorSlot, {Player(state, player)});
#pragma once

#include "game/sim.h"
#include "LayerKit/types.h"
#include "TopDownMovement/walker.h"

namespace zorlduh {

[[nodiscard]] LayerKit::Sprites Player(const GameState& state, const TopDownMovement::WalkerConfig& player);

}  // namespace zorlduh
