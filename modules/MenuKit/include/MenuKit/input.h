// What the player asks of a menu each tick, read from the engine's input through the game's own actions.
//
//     const MenuKit::MenuInput menu = MenuKit::readMenuInput(
//         input, {.up = Action::Up, .down = Action::Down, .left = Action::Left, .right = Action::Right,
//                 .confirm = Action::Accept, .cancel = Action::Back});
//
// A game reads it once per tick, at its input boundary, and hands the plain value to Menu::advance
// (menu.h), so the simulation and its tests need no engine input.
#pragma once

#include "retropp/input.h"

namespace MenuKit {

// One flag per direction.
struct Directions {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;

    [[nodiscard]] constexpr bool operator==(const Directions&) const noexcept = default;
};

// What the player asked of a menu this tick: the directions held and the ones pressed since the last
// tick, and whether confirm and cancel were pressed.
struct MenuInput {
    Directions held{};
    Directions pressed{};
    bool confirm = false;
    bool cancel = false;

    [[nodiscard]] constexpr bool operator==(const MenuInput&) const noexcept = default;
};

// One of the game's actions, written as the game's own enumerator.
struct ActionName {
    retropp::ActionId id = 0;

    template <retropp::ActionLike A>
    constexpr ActionName(A action) : id(retropp::actionId(action)) {}
};

// The game's action for each thing a menu reads.
struct MenuActions {
    ActionName up;
    ActionName down;
    ActionName left;
    ActionName right;
    ActionName confirm;
    ActionName cancel;
};

// What `input` holds and has pressed this tick, through `actions`: `held` from isHeld, `pressed`,
// `confirm` and `cancel` from justPressed. Reads player slot 0.
[[nodiscard]] inline MenuInput readMenuInput(const retropp::InputState& input, const MenuActions& actions) {
    return MenuInput{
        .held = {.up = input.isHeld(actions.up.id),
                 .down = input.isHeld(actions.down.id),
                 .left = input.isHeld(actions.left.id),
                 .right = input.isHeld(actions.right.id)},
        .pressed = {.up = input.justPressed(actions.up.id),
                    .down = input.justPressed(actions.down.id),
                    .left = input.justPressed(actions.left.id),
                    .right = input.justPressed(actions.right.id)},
        .confirm = input.justPressed(actions.confirm.id),
        .cancel = input.justPressed(actions.cancel.id),
    };
}

}  // namespace MenuKit
