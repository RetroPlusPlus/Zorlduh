// Walker: one walking character in a top-down world. It moves a position the game owns, stops at
// solids, faces one of four directions, shows one of four or eight sprite directions, and plays a walk
// animation for the direction its sprite faces.
//
//     const TopDownMovement::WalkerConfig wandererConfig{
//         .speed = 1,
//         .collision = retropp::IntRect{.x = 1, .y = 4, .width = 6, .height = 4},
//         .walk = {.up = walkUp, .down = walkDown, .left = walkLeft, .right = walkRight},
//     };
//
//     retropp::Vec2i wandererAt{76, 24};
//     TopDownMovement::Walker wanderer{wandererAt};                              // bound once
//     wanderer.walk(wandererConfig, TopDownMovement::Directions{.right = true}, walls);   // once per tick
//
// A walker is moved three ways: by a heading (Walker::walk), by input (InputWalker, input_walker.h)
// and along the engine's SpritePath (follow, route.h). A WalkerConfig describes a kind of walker; a
// Walker is the state of one. One config serves any number of walkers. WalkerSprite
// (walker_sprite.h) returns a walker's sprite.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
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

// One animation per sprite direction. The four diagonals are read only when the config's `sprites` is
// Ways::Eight.
struct DirectionalAnimations {
    retropp::Animation up;
    retropp::Animation down;
    retropp::Animation left;
    retropp::Animation right;
    retropp::Animation upLeft;
    retropp::Animation upRight;
    retropp::Animation downLeft;
    retropp::Animation downRight;
};

// Describes a kind of walker. Every position in it is in the walker's own pixels, relative to its
// top-left.
struct WalkerConfig {
    // Pixels moved per tick on each axis the walker heads along. At 0 or less the walker turns and does
    // not move.
    int speed = 1;

    // How the walker may move: Ways::Eight moves along both axes at once, Ways::Four along one.
    Ways movement = Ways::Eight;

    // How many directions its sprite has: Ways::Four shows the way it faces, Ways::Eight shows the
    // diagonals with animations of their own.
    Ways sprites = Ways::Four;

    // The pixels of the walker that collide. An empty shape collides with nothing.
    CollisionShape collision{};

    // Points in the walker's art, published on its sprite and readable through Walker::anchor. The
    // table is the game's and must outlive every use of the config.
    std::span<const retropp::Anchor> anchors{};

    // The animation for each sprite direction, played while the walker moves. A walker standing still
    // shows frame 0 of its sprite direction's animation.
    DirectionalAnimations walk{};

    // The cadence the animations' durations are resolved against. The default is
    // retropp::AnimationPlayer::defaultTiming as it stands when the config is built, which
    // EngineConfig::setActive sets to the game's own; build the config after setActive runs.
    retropp::TimingProfile timing = retropp::AnimationPlayer::defaultTiming;
};

// The state of one walking character. A Walker is bound to a position it does not own: it reads and
// writes that position, and stores only its facing, its sprite's facing and its place in the walk
// animation.
class Walker {
public:
    // A walker bound to `position`, its top-left in world pixels. The position outlives the walker and
    // stays at its address while the walker is bound to it. A facing of Dir::None is Dir::Down.
    explicit Walker(retropp::Vec2i& position, retropp::Dir facing = retropp::Dir::Down) noexcept
        : position_(&position) {
        face(facing == retropp::Dir::None ? retropp::Dir::Down : facing);
    }

    // Binds the walker to another position. A copy of a walker walks the same position as the
    // original until it is bound to its own: whatever keeps a walker and its position together binds the
    // copy to the copied position.
    void bind(retropp::Vec2i& position) noexcept { position_ = &position; }

    // Writes `position` to the bound position and resets the walk animation to frame 0. Collision is
    // not tested, and the facing is unchanged.
    void place(retropp::Vec2i position) noexcept {
        *position_ = position;
        strideTicks_ = 0;
    }

    // One tick toward `heading`, against `solids`.
    //
    // Movement. Opposite directions cancel. The walker moves along x, then along y, one pixel at a time
    // up to `config.speed`. It takes a pixel only if its collision shape there touches nothing `solids`
    // reports as solid, so it stops against a wall and, heading diagonally into one, moves along it.
    // Under Ways::Four a heading on both axes is reduced to one before moving: the direction the walker
    // faces if that is one of the two, otherwise the horizontal one.
    //
    // Facing. With a heading on one axis the walker faces that direction, whether or not it moves, so a
    // walker heading into a wall turns to face it. With a heading on both axes it keeps its facing if
    // that is one of the two directions, and otherwise faces the horizontal one. With no heading both
    // facings are unchanged.
    //
    // Animation. The walk animation advances on a tick the walker moves, keeps its place when the walker
    // turns, and returns to frame 0 on a tick it does not move.
    template <SolidQuery Q>
    void walk(const WalkerConfig& config, Directions heading, const Q& solids) {
        int dx = (heading.right ? 1 : 0) - (heading.left ? 1 : 0);
        int dy = (heading.down ? 1 : 0) - (heading.up ? 1 : 0);
        if (config.movement == Ways::Four && dx != 0 && dy != 0) {
            if (facing_ == verticalOf(dy)) {
                dx = 0;
            } else {
                dy = 0;
            }
        }
        stepToward(config, dx, dy, dx != 0 ? horizontalOf(dx) : retropp::Dir::None, solids);
    }

    // One tick of being moved by something other than the walker's own step, such as a route: the
    // walker is put at `to`, nothing colliding, and faces along `travel`, how far it moved this tick.
    // The walk animation advances while `travel` is not zero and returns to frame 0 when it is; a zero
    // `travel` leaves both facings as they were.
    //
    // The sprite faces the nearest of the eight directions to `travel`: a diagonal when its smaller
    // component is at least 0.4142 of its larger. The walker faces along the axis of the larger
    // component; when the two are equal, it keeps its facing if that is one of the two, and otherwise
    // faces the horizontal one. `config.movement` is not read: a route goes where it is drawn.
    void moveTo(const WalkerConfig& config, retropp::Vec2i to, retropp::Vec2 travel) {
        (void)config;
        *position_ = to;
        if (travel.x == 0.0f && travel.y == 0.0f) {
            strideTicks_ = 0;
            return;
        }
        const float across = std::abs(travel.x);
        const float down = std::abs(travel.y);
        const int dx = travel.x > 0.0f ? 1 : travel.x < 0.0f ? -1 : 0;
        const int dy = travel.y > 0.0f ? 1 : travel.y < 0.0f ? -1 : 0;
        constexpr float kDiagonal = 0.41421356f;   // tan 22.5°: halfway between a cardinal and a diagonal
        const bool diagonal = std::min(across, down) >= kDiagonal * std::max(across, down);
        sprite_ = diagonal ? facingOf(dx, dy) : across > down ? facingOf(dx, 0) : facingOf(0, dy);
        if (across > down) {
            facing_ = horizontalOf(dx);
        } else if (down > across) {
            facing_ = verticalOf(dy);
        } else if (facing_ != horizontalOf(dx) && facing_ != verticalOf(dy)) {
            facing_ = horizontalOf(dx);
        }
        ++strideTicks_;
    }

    // Turns the walker to `facing` without moving it. Both its facing and its sprite's are set; the walk
    // animation keeps its place. Dir::None changes nothing.
    void face(retropp::Dir facing) noexcept {
        if (facing == retropp::Dir::None) {
            return;
        }
        facing_ = facing;
        sprite_ = facingOf(facing);
    }

    // Turns the walker's sprite to `facing` without moving it. A diagonal leaves the walker's own facing
    // if that is one of its two directions, and otherwise faces it the horizontal one.
    void face(Facing facing) noexcept {
        sprite_ = facing;
        const retropp::Dir horizontal = horizontalPart(facing);
        const retropp::Dir vertical = verticalPart(facing);
        if (horizontal == retropp::Dir::None || vertical == retropp::Dir::None) {
            facing_ = horizontal != retropp::Dir::None ? horizontal : vertical;
        } else if (facing_ != horizontal && facing_ != vertical) {
            facing_ = horizontal;
        }
    }

    // The walker's top-left, in world pixels: the position it is bound to.
    [[nodiscard]] retropp::Vec2i position() const noexcept { return *position_; }

    // The way the walker faces: Up, Down, Left or Right.
    [[nodiscard]] retropp::Dir facing() const noexcept { return facing_; }

    // The way the walker's sprite faces under `config`: its facing when `config.sprites` is Ways::Four,
    // and the last of the eight directions it headed in when it is Ways::Eight.
    [[nodiscard]] Facing spriteFacing(const WalkerConfig& config) const noexcept {
        return config.sprites == Ways::Eight ? sprite_ : facingOf(facing_);
    }

    // Whether the walk animation is running: true after a walk that moved the walker or a moveTo with
    // a non-zero travel, false after a walk that did not move it, a moveTo with zero travel, or place.
    [[nodiscard]] bool moving() const noexcept { return strideTicks_ > 0; }

    // A copy of the walker's collision shape moved to its position, in world pixels.
    [[nodiscard]] CollisionShape collision(const WalkerConfig& config) const {
        return config.collision.at(*position_);
    }

    // The anchor labeled `label` in `config.anchors`, in world pixels. Throws std::out_of_range when the
    // table has no such anchor.
    [[nodiscard]] retropp::Point anchor(const WalkerConfig& config, std::string_view label) const {
        const auto found = std::find_if(config.anchors.begin(), config.anchors.end(),
                                        [&](const retropp::Anchor& a) { return a.label == label; });
        if (found == config.anchors.end()) {
            throw std::out_of_range("Walker::anchor: no anchor with that label");
        }
        return retropp::Point{static_cast<float>(position_->x) + found->x,
                              static_cast<float>(position_->y) + found->y};
    }

    // The frame of the walk animation for the way the sprite faces, as the walker shows it now. Throws
    // std::logic_error when that animation has no frames.
    [[nodiscard]] const retropp::AnimationFrame& frame(const WalkerConfig& config) const {
        const retropp::Animation& animation = animationFor(config, spriteFacing(config));
        if (animation.count() == 0) {
            throw std::logic_error("Walker::frame: the walk animation for the sprite's direction has no frames");
        }
        return retropp::sampleAnimationFrame(animation, strideTicks_, config.timing,
                                             retropp::PlaybackMode::loopIndefinitely());
    }

protected:
    // One tick toward (dx, dy), each -1, 0 or 1: moves, turns and animates as walk does. When both are
    // non-zero and the walker faces neither of their directions, it faces `diagonal`. `config.movement`
    // is not read; a derived class reduces a two-axis step to one axis itself. For a derived class that
    // decides the walker's direction its own way, as InputWalker does.
    template <SolidQuery Q>
    void stepToward(const WalkerConfig& config, int dx, int dy, retropp::Dir diagonal, const Q& solids) {
        if (dx != 0 || dy != 0) {
            sprite_ = facingOf(dx, dy);
            if (dx != 0 && dy != 0) {
                if (facing_ != horizontalOf(dx) && facing_ != verticalOf(dy)) {
                    facing_ = diagonal;
                }
            } else {
                facing_ = dx != 0 ? horizontalOf(dx) : verticalOf(dy);
            }
        }
        const bool movedX = stepAxis(config, solids, retropp::Vec2i{.x = dx, .y = 0});
        const bool movedY = stepAxis(config, solids, retropp::Vec2i{.x = 0, .y = dy});
        if (!movedX && !movedY) {
            strideTicks_ = 0;
            return;
        }
        ++strideTicks_;
    }

    [[nodiscard]] static retropp::Dir horizontalOf(int dx) noexcept {
        return dx > 0 ? retropp::Dir::Right : retropp::Dir::Left;
    }

    [[nodiscard]] static retropp::Dir verticalOf(int dy) noexcept {
        return dy > 0 ? retropp::Dir::Down : retropp::Dir::Up;
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
            const retropp::Vec2i next{.x = position_->x + unit.x, .y = position_->y + unit.y};
            if (config.collision.hits(solids, next)) {
                break;
            }
            *position_ = next;
            moved = true;
        }
        return moved;
    }

    // The Facing of a step of (dx, dy), each -1, 0 or 1, not both 0.
    [[nodiscard]] static Facing facingOf(int dx, int dy) noexcept {
        if (dy < 0) {
            return dx < 0 ? Facing::UpLeft : dx > 0 ? Facing::UpRight : Facing::Up;
        }
        if (dy > 0) {
            return dx < 0 ? Facing::DownLeft : dx > 0 ? Facing::DownRight : Facing::Down;
        }
        return dx < 0 ? Facing::Left : Facing::Right;
    }

    [[nodiscard]] static Facing facingOf(retropp::Dir facing) noexcept {
        switch (facing) {
            case retropp::Dir::Up: return Facing::Up;
            case retropp::Dir::Left: return Facing::Left;
            case retropp::Dir::Right: return Facing::Right;
            case retropp::Dir::Down:
            default: return Facing::Down;
        }
    }

    [[nodiscard]] static retropp::Dir horizontalPart(Facing facing) noexcept {
        switch (facing) {
            case Facing::UpLeft:
            case Facing::Left:
            case Facing::DownLeft: return retropp::Dir::Left;
            case Facing::UpRight:
            case Facing::Right:
            case Facing::DownRight: return retropp::Dir::Right;
            default: return retropp::Dir::None;
        }
    }

    [[nodiscard]] static retropp::Dir verticalPart(Facing facing) noexcept {
        switch (facing) {
            case Facing::UpLeft:
            case Facing::Up:
            case Facing::UpRight: return retropp::Dir::Up;
            case Facing::DownLeft:
            case Facing::Down:
            case Facing::DownRight: return retropp::Dir::Down;
            default: return retropp::Dir::None;
        }
    }

    [[nodiscard]] static const retropp::Animation& animationFor(const WalkerConfig& config, Facing facing) noexcept {
        switch (facing) {
            case Facing::Up: return config.walk.up;
            case Facing::UpRight: return config.walk.upRight;
            case Facing::Right: return config.walk.right;
            case Facing::DownRight: return config.walk.downRight;
            case Facing::DownLeft: return config.walk.downLeft;
            case Facing::Left: return config.walk.left;
            case Facing::UpLeft: return config.walk.upLeft;
            case Facing::Down:
            default: return config.walk.down;
        }
    }

    retropp::Vec2i* position_;                   // the position the walker is bound to
    retropp::Dir facing_ = retropp::Dir::Down;   // Up, Down, Left or Right
    Facing sprite_ = Facing::Down;               // the last of the eight directions it headed in
    std::uint64_t strideTicks_ = 0;              // ticks walked since the walker last stood
};

}  // namespace TopDownMovement
