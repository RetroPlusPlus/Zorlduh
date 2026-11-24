// What moves an actor: a mover of the game's own type, described by its shape, and what drives it.
//
//     const ActorKit::Actor hero{
//         .mover = ActorKit::movedBy<TopDownMovement::InputWalker>(heroWalk, &Input::move),   // the pad
//     };
//     const ActorKit::Actor hen{
//         .ownValues = {{Heading, retropp::Dir::None}},
//         .mover     = ActorKit::movedBy<TopDownMovement::Walker>(henWalk, Heading),           // a value
//     };
//
// The mover's type is the game's. movedBy is instantiated in the game's code, where that type is seen,
// and ActorKit makes one mover per actor spawned, bound to that actor's position, walks it every tick by
// its drive, and reads back its facing, its collision box and its frame.
#pragma once

#include <concepts>
#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include "retropp/animation.h"
#include "retropp/geometry.h"
#include "retropp/input_actions.h"

#include "ScriptKit/value.h"

#include "WorldKit/solids.h"

namespace ActorKit {

// What is solid to a walking actor, as ActorKit hands it to the mover's walk: the cells of its area's
// collision map that WorldKit reports solid; every rectangle in `others`; and, while `confined` is true,
// everything outside `bounds`, the area's own rectangle. Valid during the walk it is handed to.
//
// tick fills `others` with the box of every other solid actor in the area that lies apart from the
// walking actor's own box as its walk begins — two solid actors that overlap walk apart —, and for an actor whose
// confinement is Confinement::Layout, the strip beyond each edge opening that leads to another layout,
// as wide as the opening's span. It sets `confined` for an actor whose confinement is
// Confinement::Area, the player (Actors::player) excepted.
class Solids {
public:
    Solids(WorldKit::AreaSolids area, retropp::IntRect bounds, bool confined,
           std::span<const retropp::IntRect> others) noexcept
        : area_(area), bounds_(bounds), confined_(confined), others_(others) {}

    // Whether any pixel of `box` is solid. A box of zero width or height is open.
    [[nodiscard]] bool solidBox(retropp::IntRect box) const noexcept {
        if (box.width <= 0 || box.height <= 0) {
            return false;
        }
        if (area_.solidBox(box)) {
            return true;
        }
        if (confined_ && (box.x < bounds_.x || box.y < bounds_.y || box.x + box.width > bounds_.x + bounds_.width ||
                          box.y + box.height > bounds_.y + bounds_.height)) {
            return true;
        }
        for (const retropp::IntRect& other : others_) {
            if (box.x < other.x + other.width && other.x < box.x + box.width && box.y < other.y + other.height &&
                other.y < box.y + box.height) {
                return true;
            }
        }
        return false;
    }

private:
    WorldKit::AreaSolids area_;
    retropp::IntRect bounds_;
    bool confined_;
    std::span<const retropp::IntRect> others_;
};

namespace detail {

// What a mover's drive reads on one tick: the game's input value, with its type, and the actor's values.
struct DriveInput {
    const void* input = nullptr;
    const std::type_info* type = nullptr;
    const ScriptKit::Values* values = nullptr;
};

// One actor's mover: a mover of the game's type, bound to the actor's position.
class MoverState {
public:
    virtual ~MoverState() = default;

    // A copy of this mover bound to `position`.
    [[nodiscard]] virtual std::unique_ptr<MoverState> copyFor(retropp::Vec2i& position) const = 0;
    virtual void walk(const DriveInput& drive, const Solids& solids) = 0;
    [[nodiscard]] virtual retropp::Dir facing() const = 0;
    // The bounds of the mover's collision shape, relative to the actor's position.
    [[nodiscard]] virtual retropp::IntRect box() const = 0;
    [[nodiscard]] virtual const retropp::AnimationFrame& frame() const = 0;
};

// What movedBy captured: the mover's type, its config and its drive.
class MoverKind {
public:
    virtual ~MoverKind() = default;

    [[nodiscard]] virtual std::unique_ptr<MoverState> make(retropp::Vec2i& position) const = 0;
    // The value the drive reads, when it is a Value<retropp::Dir>; 0 otherwise.
    [[nodiscard]] virtual ScriptKit::ValueId reads() const noexcept = 0;
    // Whether the drive reads the actor's values at all.
    [[nodiscard]] virtual bool readsValues() const noexcept = 0;
};

// A heading of one direction, handed to a mover's walk: it converts to any heading type with `up`,
// `down`, `left` and `right` flags, setting the one flag `direction` names. Dir::None leaves all four false.
struct Heading {
    retropp::Dir direction = retropp::Dir::None;

    template <class H>
        requires std::default_initializable<H> && requires(H heading) {
            heading.up = true;
            heading.down = true;
            heading.left = true;
            heading.right = true;
        }
    constexpr operator H() const noexcept {
        H heading{};
        heading.up = direction == retropp::Dir::Up;
        heading.down = direction == retropp::Dir::Down;
        heading.left = direction == retropp::Dir::Left;
        heading.right = direction == retropp::Dir::Right;
        return heading;
    }
};

template <class Drive>
inline constexpr bool isDirValue = std::same_as<Drive, ScriptKit::Value<retropp::Dir>>;

template <class Drive>
inline constexpr bool isInputMember = std::is_member_object_pointer_v<Drive>;

template <class Drive>
inline constexpr bool isValuesFunction =
    !isDirValue<Drive> && !isInputMember<Drive> && std::is_invocable_v<const Drive&, const ScriptKit::Values&>;

// The class a member pointer is a member of.
template <class Member>
struct MemberOf;
template <class D, class In>
struct MemberOf<D In::*> {
    using type = In;
};

template <class M, class Config, class Drive>
class MoverOf final : public MoverState {
public:
    MoverOf(std::shared_ptr<const Config> config, const Drive& drive, retropp::Vec2i& position)
        : config_(std::move(config)), drive_(drive), position_(&position), mover_(position) {}

    MoverOf(const MoverOf& other, retropp::Vec2i& position)
        : config_(other.config_), drive_(other.drive_), position_(&position), mover_(other.mover_) {
        mover_.bind(position);
    }

    [[nodiscard]] std::unique_ptr<MoverState> copyFor(retropp::Vec2i& position) const override {
        return std::make_unique<MoverOf>(*this, position);
    }

    void walk(const DriveInput& drive, const Solids& solids) override {
        if constexpr (isInputMember<Drive>) {
            using In = typename MemberOf<Drive>::type;
            if (drive.type == nullptr || *drive.type != typeid(In)) {
                throw std::logic_error("ActorKit: a mover is driven by a member of an input type other than the "
                                       "one handed to tick");
            }
            mover_.walk(*config_, static_cast<const In*>(drive.input)->*drive_, solids);
        } else if constexpr (isDirValue<Drive>) {
            mover_.walk(*config_, Heading{drive.values->get(drive_)}, solids);
        } else {
            mover_.walk(*config_, drive_(*drive.values), solids);
        }
    }

    [[nodiscard]] retropp::Dir facing() const override { return mover_.facing(); }

    [[nodiscard]] retropp::IntRect box() const override {
        retropp::IntRect bounds = mover_.collision(*config_).bounds();
        bounds.x -= position_->x;
        bounds.y -= position_->y;
        return bounds;
    }

    [[nodiscard]] const retropp::AnimationFrame& frame() const override { return mover_.frame(*config_); }

private:
    std::shared_ptr<const Config> config_;
    Drive drive_;
    retropp::Vec2i* position_;
    M mover_;
};

template <class M, class Config, class Drive>
class MoverKindOf final : public MoverKind {
public:
    MoverKindOf(Config config, Drive drive)
        : config_(std::make_shared<const Config>(std::move(config))), drive_(std::move(drive)) {}

    [[nodiscard]] std::unique_ptr<MoverState> make(retropp::Vec2i& position) const override {
        return std::make_unique<MoverOf<M, Config, Drive>>(config_, drive_, position);
    }

    [[nodiscard]] ScriptKit::ValueId reads() const noexcept override {
        if constexpr (isDirValue<Drive>) {
            return drive_.id();
        } else {
            return 0;
        }
    }

    [[nodiscard]] bool readsValues() const noexcept override { return !isInputMember<Drive>; }

private:
    std::shared_ptr<const Config> config_;
    Drive drive_;
};

}  // namespace detail

// What moves an actor: made by movedBy, empty for an actor that stays where it is put. Copies share
// what movedBy captured; each actor spawned gets a mover of its own.
class Mover {
public:
    Mover() = default;

    [[nodiscard]] explicit operator bool() const noexcept { return kind_ != nullptr; }
    [[nodiscard]] const detail::MoverKind* kind() const noexcept { return kind_.get(); }

private:
    template <class M, class Config, class Drive>
    friend Mover movedBy(Config config, Drive drive);

    explicit Mover(std::shared_ptr<const detail::MoverKind> kind) noexcept : kind_(std::move(kind)) {}

    std::shared_ptr<const detail::MoverKind> kind_;
};

// A mover of type M, walked under `config` by `drive`. M is any type that
//
//   - is constructible from a retropp::Vec2i&, the position it walks, and copyable, with
//     bind(retropp::Vec2i&) to move a copy onto another position;
//   - has walk(const Config&, D, const Solids&), for the D the drive hands it;
//   - has facing() returning a retropp::Dir, collision(const Config&) returning something with
//     bounds() returning a retropp::IntRect in the position's pixels, and frame(const Config&) returning
//     a const retropp::AnimationFrame&.
//
// TopDownMovement::Walker and TopDownMovement::InputWalker are two such types. `drive` is one of:
//
//   - a member of the game's input value, `D Input::*`: the mover is walked with `input.*drive` each
//     tick (&Input::move);
//   - a ScriptKit::Value<retropp::Dir>, the actor's own or the game's: the mover is walked toward that
//     one direction, handed over as a value that converts to the mover's heading type — any type with
//     bool `up`, `down`, `left` and `right` — and Dir::None holds it where it is;
//   - a function of the actor's values, `D(const ScriptKit::Values&)`, for a heading the game works out.
//
// The drive by a member is checked against the type of the input handed to tick, and walking with
// another throws std::logic_error. `config` is copied and kept.
template <class M, class Config, class Drive>
[[nodiscard]] Mover movedBy(Config config, Drive drive) {
    static_assert(detail::isInputMember<Drive> || detail::isDirValue<Drive> || detail::isValuesFunction<Drive>,
                  "ActorKit::movedBy: the drive is a member of the input type, a ScriptKit::Value<retropp::Dir>, "
                  "or a function of const ScriptKit::Values&");
    static_assert(std::constructible_from<M, retropp::Vec2i&>,
                  "ActorKit::movedBy: the mover is constructible from the retropp::Vec2i& it walks");
    return Mover(std::make_shared<const detail::MoverKindOf<M, Config, Drive>>(std::move(config), std::move(drive)));
}

}  // namespace ActorKit
