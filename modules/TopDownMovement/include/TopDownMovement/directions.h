// The four directions a walker is steered by, read from the engine's input through the game's own
// actions.
//
//     const TopDownMovement::DirectionalInput move = TopDownMovement::readDirections(
//         input, {.up = Action::Up, .down = Action::Down, .left = Action::Left, .right = Action::Right});
//
// A game reads them once per tick, at its input boundary, and hands the plain value to whatever walks.
#pragma once

#include "retropp/input.h"

namespace TopDownMovement {

// One flag per direction.
struct Directions {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;

    [[nodiscard]] constexpr bool operator==(const Directions&) const noexcept = default;
};

// The directions held this tick, and the ones pressed since the last tick. A walker moves by what is
// held and turns by the order in which directions were pressed.
struct DirectionalInput {
    Directions held{};
    Directions pressed{};

    [[nodiscard]] constexpr bool operator==(const DirectionalInput&) const noexcept = default;
};

// One of the game's actions, written as the game's own enumerator.
struct ActionName {
    retropp::ActionId id = 0;

    template <retropp::ActionLike A>
    constexpr ActionName(A action) : id(retropp::actionId(action)) {}
};

// The game's action for each direction.
struct DirectionalActions {
    ActionName up;
    ActionName down;
    ActionName left;
    ActionName right;
};

// The directions `input` holds and has pressed this tick, through `actions`. Reads player slot 0.
[[nodiscard]] inline DirectionalInput readDirections(const retropp::InputState& input,
                                                     const DirectionalActions& actions) {
    return DirectionalInput{
        .held = {.up = input.isHeld(actions.up.id),
                 .down = input.isHeld(actions.down.id),
                 .left = input.isHeld(actions.left.id),
                 .right = input.isHeld(actions.right.id)},
        .pressed = {.up = input.justPressed(actions.up.id),
                    .down = input.justPressed(actions.down.id),
                    .left = input.justPressed(actions.left.id),
                    .right = input.justPressed(actions.right.id)},
    };
}

}  // namespace TopDownMovement
