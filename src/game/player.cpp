#include "game/player.h"

namespace zorlduh {

Player::Player() : Player(retropp::Vec2i{}) {}

// The walker is bound to `at` before `at` is given its value; nothing reads the position until then.
Player::Player(retropp::Vec2i start, retropp::Dir facing) : TopDownMovement::InputWalker(at, facing), at(start) {}

Player::Player(const Player& other) : TopDownMovement::InputWalker(other), at(other.at) {
    bind(at);
}

Player& Player::operator=(const Player& other) {
    TopDownMovement::InputWalker::operator=(other);
    at = other.at;
    bind(at);
    return *this;
}

TopDownMovement::WalkerConfig PlayerConfig(const Assets& assets) {
    return TopDownMovement::WalkerConfig{
        .speed = 1,
        .collision = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8},
        .walk = assets.playerWalk,
    };
}

}  // namespace zorlduh
