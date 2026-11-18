// The values an actor's scripts read: the actor's own, the six ActorKit fills in, and the game's.
//
//     const ScriptKit::Value<bool> Awake;
//
//     ActorKit::Actor dog{.ownValues = {{Awake, false}}, .scripts = {&wake}};   // each dog its own Awake
//
//     // In a script: ActorKit::X, ActorKit::Touched, ActorKit::PlayerX … read like any value.
//
// A value listed in an actor's `ownValues` is that actor's own: every actor spawned from the value
// holds an instance of it, starting at the worth listed, and its scripts read and set that instance. Any
// other value a script names is the game's: one instance, read and written through the game's
// ScriptKit::Bindings. The six values below are ActorKit's: worked out for the actor whose script reads
// them, and read only.
#pragma once

#include <cstdint>
#include <vector>

#include "ScriptKit/command.h"
#include "ScriptKit/value.h"

namespace ActorKit {

// One value an actor owns and the worth each actor spawned from the value starts it at:
// {Awake, false}, {Heading, retropp::Dir::None}. The worth has exactly the value's type.
class OwnValue {
public:
    template <ScriptKit::ValueType T>
    OwnValue(ScriptKit::Value<T> value, T worth) noexcept
        : id_(value.id()), raw_(static_cast<std::int64_t>(worth)), apply_(&applyAs<T>) {}

    // The value's identity.
    [[nodiscard]] ScriptKit::ValueId id() const noexcept { return id_; }

    // The starting worth, as the integer a ScriptKit::Values stores it as.
    [[nodiscard]] std::int64_t start() const noexcept { return raw_; }

    // Writes `request` into `own` and returns true when it is a `set` of this value; returns false for
    // any other request, leaving `own` as it is.
    bool apply(const ScriptKit::Request& request, ScriptKit::Values& own) const { return apply_(request, id_, own); }

private:
    template <ScriptKit::ValueType T>
    static bool applyAs(const ScriptKit::Request& request, ScriptKit::ValueId id, ScriptKit::Values& own) {
        const ScriptKit::Change<T>* change = request.change<T>();
        if (change == nullptr || change->value.id() != id) {
            return false;
        }
        own.set(change->value, change->to);
        return true;
    }

    ScriptKit::ValueId id_;
    std::int64_t raw_;
    bool (*apply_)(const ScriptKit::Request&, ScriptKit::ValueId, ScriptKit::Values&);
};

// An actor's own values, in the order listed.
using OwnValues = std::vector<OwnValue>;

// True on a tick an actor whose `setsOff` lists a trigger bound to ActorKit::touched() overlaps this
// actor's box, in the same area.
inline const ScriptKit::Value<bool> Touched;

// The actor's position: the top-left of its art, in its area's pixels.
inline const ScriptKit::Value<int> X;
inline const ScriptKit::Value<int> Y;

// True when the player (Actors::player) is in this actor's area.
inline const ScriptKit::Value<bool> PlayerHere;

// The player's position while it is in this actor's area; this actor's own position otherwise.
inline const ScriptKit::Value<int> PlayerX;
inline const ScriptKit::Value<int> PlayerY;

namespace detail {

// Whether `id` names one of the six values ActorKit fills in.
[[nodiscard]] inline bool provided(ScriptKit::ValueId id) noexcept {
    return id == Touched.id() || id == X.id() || id == Y.id() || id == PlayerHere.id() ||
           id == PlayerX.id() || id == PlayerY.id();
}

// Whether `request` is a `set` of one of the six: such a set is dropped.
[[nodiscard]] inline bool setsProvided(const ScriptKit::Request& request) noexcept {
    if (const auto* change = request.change<bool>()) {
        return provided(change->value.id());
    }
    if (const auto* change = request.change<int>()) {
        return provided(change->value.id());
    }
    return false;
}

}  // namespace detail

}  // namespace ActorKit
