// The player on screen: its walker's sprite, keyed "player", in world pixels for the actors layer.
//
//     LayerKit::SpriteLayer(kActorsLayer, kActorsZ, {Player(state, player)}, {.scroll = camera});
#pragma once

#include "game/sim.h"
#include "LayerKit/types.h"
#include "TopDownMovement/walker.h"

namespace zorlduh {

[[nodiscard]] LayerKit::Sprites Player(const GameState& state, const TopDownMovement::WalkerConfig& player);

}  // namespace zorlduh
