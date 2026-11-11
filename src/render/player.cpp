#include "render/player.h"

#include "TopDownMovement/walker_sprite.h"

namespace zorlduh {

LayerKit::Sprites Player(const GameState& state, const TopDownMovement::WalkerConfig& player) {
    return {TopDownMovement::WalkerSprite("player", state.player, player)};
}

}  // namespace zorlduh
