#include "game/player.h"

namespace zorlduh {

TopDownMovement::WalkerConfig PlayerConfig(const Assets& assets) {
    return TopDownMovement::WalkerConfig{
        .speed = 1,
        .collision = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8},
        .walk = assets.playerWalk,
    };
}

}  // namespace zorlduh
