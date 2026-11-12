// A walker: anything that walks a top-down world in eight directions, collides with what is solid,
// faces one of four ways, and plays a walk animation for the way it faces.
//
//     const TopDownMovement::WalkerConfig hero{
//         .speed = 1,
//         .collision = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8},
//         .walk = {.up = walkUp, .down = walkDown, .left = walkLeft, .right = walkRight},
//     };
//
//     TopDownMovement::Walker walker{{72, 56}};
//     walker.walk(hero, TopDownMovement::readDirections(input, actions), walls);   // once per tick
//
// The config says what the walker is; the Walker value is where it is now. One config serves any number
// of walkers. The walker draws through TopDownMovement/walker_sprite.h.
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string_view>

#include "retropp/animation.h"
#include "retropp/draw_state.h"
#include "retropp/geometry.h"
#include "retropp/input_actions.h"
#include "retropp/timing.h"

#include "TopDownMovement/collision.h"
#include "TopDownMovement/directions.h"

namespace TopDownMovement {

// One animation per facing.
struct DirectionalAnimations {
    retropp::Animation up;
    retropp::Animation down;
    retropp::Animation left;
    retropp::Animation right;
};

// What a walker is. Every position in it is in the walker's own pixels, relative to its top-left.
struct WalkerConfig {
    // Pixels moved per tick on each axis a direction is held on.
    int speed = 1;

    // What the walker collides with.
    CollisionShape collision{};

    // Points in the walker's art, published on its sprite and readable through Walker::anchor. The
    // table is the game's and must outlive every use of the config.
    std::span<const retropp::Anchor> anchors{};

    // The animation for each facing, played while the walker moves. A walker standing still shows
    // frame 0 of its facing's animation.
    DirectionalAnimations walk{};

    // The cadence the animations' durations are resolved against. The default is
    // retropp::AnimationPlayer::defaultTiming as it stands when the config is built, which
    // EngineConfig::setActive sets to the game's own; build the config after setActive runs.
    retropp::TimingProfile timing = retropp::AnimationPlayer::defaultTiming;
};

class Walker {
public:
    // A walker at (0, 0) facing down.
    Walker() = default;

    // A walker with its top-left at `position`, in world pixels.
    explicit Walker(retropp::Vec2i position, retropp::Dir facing = retropp::Dir::Down)
        : position_(position), facing_(facing == retropp::Dir::None ? retropp::Dir::Down : facing) {}

    // Puts the walker's top-left at `position` without walking there: nothing collides on the way, and
    // the walker stands.
    void place(retropp::Vec2i position) noexcept {
        position_ = position;
        strideTicks_ = 0;
    }

    // One tick of walking.
    //
    // Opposite directions cancel. The walker moves along x, then along y, one pixel at a time up to
    // `config.speed`, taking each pixel only if its collision shape there covers nothing `solids` calls
    // solid, so it stops flush against a wall and slides along one when walking into it diagonally.
    //
    // Facing follows the directions held, whether or not the walker moves, so a walker pushing into a
    // wall turns to face it. Holding one axis, it faces that way. Holding two directions on different
    // axes, it keeps its facing if that is one of the two, and otherwise faces the one pressed more
    // recently; when both were pressed on the same tick, the horizontal one. Holding nothing, or only
    // opposite directions, leaves the facing as it was.
    //
    // The facing's animation advances while the walker moves, keeping its place across a turn, and
    // returns to frame 0 when it stands.
    template <SolidQuery Q>
    void walk(const WalkerConfig& config, const DirectionalInput& input, const Q& solids) {
        ++pressClock_;
        recordPress(input.pressed.up, retropp::Dir::Up);
        recordPress(input.pressed.down, retropp::Dir::Down);
        recordPress(input.pressed.left, retropp::Dir::Left);
        recordPress(input.pressed.right, retropp::Dir::Right);

        const int dx = (input.held.right ? 1 : 0) - (input.held.left ? 1 : 0);
        const int dy = (input.held.down ? 1 : 0) - (input.held.up ? 1 : 0);
        const bool movedX = stepAxis(config, solids, retropp::Vec2i{.x = dx, .y = 0});
        const bool movedY = stepAxis(config, solids, retropp::Vec2i{.x = 0, .y = dy});

        const retropp::Dir horizontal = dx > 0 ? retropp::Dir::Right : retropp::Dir::Left;
        const retropp::Dir vertical = dy > 0 ? retropp::Dir::Down : retropp::Dir::Up;
        if (dx != 0 && dy != 0) {
            if (facing_ != horizontal && facing_ != vertical) {
                facing_ = pressedAt(vertical) > pressedAt(horizontal) ? vertical : horizontal;
            }
        } else if (dx != 0 || dy != 0) {
            facing_ = dx != 0 ? horizontal : vertical;
        }

        if (!movedX && !movedY) {
            strideTicks_ = 0;
            return;
        }
        ++strideTicks_;
    }

    // The walker's top-left, in world pixels.
    [[nodiscard]] retropp::Vec2i position() const noexcept { return position_; }

    // The way the walker faces: Up, Down, Left or Right.
    [[nodiscard]] retropp::Dir facing() const noexcept { return facing_; }

    // Whether the walker moved on its last tick.
    [[nodiscard]] bool moving() const noexcept { return strideTicks_ > 0; }

    // The walker's collision shape where it stands, in world pixels.
    [[nodiscard]] CollisionShape collision(const WalkerConfig& config) const {
        return config.collision.at(position_);
    }

    // The anchor labeled `label` in `config.anchors`, in world pixels. Throws std::out_of_range when the
    // table has no such anchor.
    [[nodiscard]] retropp::Point anchor(const WalkerConfig& config, std::string_view label) const {
        const auto found = std::find_if(config.anchors.begin(), config.anchors.end(),
                                        [&](const retropp::Anchor& a) { return a.label == label; });
        if (found == config.anchors.end()) {
            throw std::out_of_range("Walker::anchor: no anchor with that label");
        }
        return retropp::Point{static_cast<float>(position_.x) + found->x,
                              static_cast<float>(position_.y) + found->y};
    }

    // The frame of the facing's animation the walker shows now. Throws std::logic_error when that
    // animation has no frames.
    [[nodiscard]] const retropp::AnimationFrame& frame(const WalkerConfig& config) const {
        const retropp::Animation& animation = animationFor(config, facing_);
        if (animation.count() == 0) {
            throw std::logic_error("Walker::frame: the facing's walk animation has no frames");
        }
        return retropp::sampleAnimationFrame(animation, strideTicks_, config.timing,
                                             retropp::PlaybackMode::loopIndefinitely());
    }

private:
    // Moves up to `config.speed` pixels by `unit`, stopping at the first pixel that collides. Returns
    // whether the walker moved at all.
    template <SolidQuery Q>
    bool stepAxis(const WalkerConfig& config, const Q& solids, retropp::Vec2i unit) {
        if (unit.x == 0 && unit.y == 0) {
            return false;
        }
        bool moved = false;
        for (int i = 0; i < config.speed; ++i) {
            const retropp::Vec2i next{.x = position_.x + unit.x, .y = position_.y + unit.y};
            if (config.collision.hits(solids, next)) {
                break;
            }
            position_ = next;
            moved = true;
        }
        return moved;
    }

    void recordPress(bool pressed, retropp::Dir direction) noexcept {
        if (pressed) {
            pressedAt_[slot(direction)] = pressClock_;
        }
    }

    [[nodiscard]] std::uint64_t pressedAt(retropp::Dir direction) const noexcept {
        return pressedAt_[slot(direction)];
    }

    [[nodiscard]] static std::size_t slot(retropp::Dir direction) noexcept {
        switch (direction) {
            case retropp::Dir::Up: return 0;
            case retropp::Dir::Down: return 1;
            case retropp::Dir::Left: return 2;
            case retropp::Dir::Right:
            default: return 3;
        }
    }

    [[nodiscard]] static const retropp::Animation& animationFor(const WalkerConfig& config,
                                                                retropp::Dir facing) noexcept {
        switch (facing) {
            case retropp::Dir::Up: return config.walk.up;
            case retropp::Dir::Left: return config.walk.left;
            case retropp::Dir::Right: return config.walk.right;
            case retropp::Dir::Down:
            default: return config.walk.down;
        }
    }

    retropp::Vec2i position_{};
    retropp::Dir facing_ = retropp::Dir::Down;
    std::uint64_t strideTicks_ = 0;              // ticks walked since the walker last stood
    std::uint64_t pressClock_ = 0;               // walk calls so far
    std::array<std::uint64_t, 4> pressedAt_{};   // the walk call each direction was last pressed on
};

}  // namespace TopDownMovement
