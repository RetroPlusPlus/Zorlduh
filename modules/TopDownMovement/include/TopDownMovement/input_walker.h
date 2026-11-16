// InputWalker — a walker moved by the player's input.
//
//     retropp::Vec2i heroAt{76, 68};
//     TopDownMovement::InputWalker hero{heroAt};
//     hero.walk(heroConfig, TopDownMovement::readDirections(input, steering), walls);   // once per tick
//
// It is a Walker that also records the order in which directions were pressed, and uses it when two
// are held. Its walk takes a DirectionalInput and hides Walker::walk; to walk by a heading, use a plain
// Walker.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "retropp/geometry.h"
#include "retropp/input_actions.h"

#include "TopDownMovement/collision.h"
#include "TopDownMovement/directions.h"
#include "TopDownMovement/walker.h"

namespace TopDownMovement {

class InputWalker : public Walker {
public:
    // An input walker bound to `position`, as Walker is.
    explicit InputWalker(retropp::Vec2i& position, retropp::Dir facing = retropp::Dir::Down) noexcept
        : Walker(position, facing) {}

    // One tick of walking by the directions in `input.held`, against `solids`. It moves, turns and
    // animates as Walker::walk does. `input.pressed` records which direction is newer. With directions
    // held on both axes:
    //
    //   - under Ways::Eight it moves along both, and keeps its facing if that is one of the two;
    //     otherwise it faces the one pressed more recently, or the horizontal one when both were
    //     pressed on the same tick;
    //   - under Ways::Four it moves and faces along the one pressed more recently, or the horizontal
    //     one when both were pressed on the same tick. When that one is released, it moves along the
    //     other.
    template <SolidQuery Q>
    void walk(const WalkerConfig& config, const DirectionalInput& input, const Q& solids) {
        ++pressClock_;
        recordPress(input.pressed.up, 0);
        recordPress(input.pressed.down, 1);
        recordPress(input.pressed.left, 2);
        recordPress(input.pressed.right, 3);

        int dx = (input.held.right ? 1 : 0) - (input.held.left ? 1 : 0);
        int dy = (input.held.down ? 1 : 0) - (input.held.up ? 1 : 0);
        const bool verticalLater = dx != 0 && dy != 0 && pressedAt(dy > 0 ? 1 : 0) > pressedAt(dx > 0 ? 3 : 2);
        if (config.movement == Ways::Four && dx != 0 && dy != 0) {
            if (verticalLater) {
                dx = 0;
            } else {
                dy = 0;
            }
        }
        const retropp::Dir diagonal = verticalLater ? verticalOf(dy) : horizontalOf(dx);
        stepToward(config, dx, dy, diagonal, solids);
    }

private:
    // Slots: Up 0, Down 1, Left 2, Right 3.
    void recordPress(bool pressed, std::size_t slot) noexcept {
        if (pressed) {
            pressedAt_[slot] = pressClock_;
        }
    }

    [[nodiscard]] std::uint64_t pressedAt(std::size_t slot) const noexcept { return pressedAt_[slot]; }

    std::uint64_t pressClock_ = 0;               // walk calls so far
    std::array<std::uint64_t, 4> pressedAt_{};   // the walk call each direction was last pressed on
};

}  // namespace TopDownMovement
