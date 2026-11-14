#include "input/input.h"

#include "MenuKit/input.h"
#include "TopDownMovement/directions.h"

namespace zorlduh {

retropp::ActionMap actionMap() {
    retropp::ActionMap map{
        {Action::Attack, {SDL_SCANCODE_X, retropp::PadButton::FaceLabelB}},
        {Action::Item, {SDL_SCANCODE_Z, retropp::PadButton::FaceLabelA}},
        {Action::Start, {SDL_SCANCODE_RETURN, retropp::PadButton::Start}},
    };
    map.add(retropp::presets::directional(Action::Up, Action::Down, Action::Left, Action::Right));
    return map;
}

Input readInput(const retropp::InputState& input) {
    return Input{
        .move = TopDownMovement::readDirections(input, {.up = Action::Up,
                                                 .down = Action::Down,
                                                 .left = Action::Left,
                                                 .right = Action::Right}),
        .item = input.justPressed(Action::Item),
        .start = input.justPressed(Action::Start),
        .menu = MenuKit::readMenuInput(input, {.up = Action::Up,
                                               .down = Action::Down,
                                               .left = Action::Left,
                                               .right = Action::Right,
                                               .confirm = Action::Item,
                                               .cancel = Action::Attack}),
    };
}

}  // namespace zorlduh
