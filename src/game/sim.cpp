#include "game/sim.h"

namespace zorlduh {

void tick(GameState& state, const Input& input, const TopDownMovement::WalkerConfig& player) {
    state.player.walk(player, input.move, state.world);
}

}  // namespace zorlduh
