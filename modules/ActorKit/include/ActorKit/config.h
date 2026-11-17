// Config — what the game hands ActorKit: the world, its interact action, and its own triggers, values
// and commands.
//
//     const ActorKit::Config config{.world = &world, .interactAction = Action::Interact};
//
//     const ActorKit::Config config{
//         .world          = &world,
//         .interactAction = Action::Interact,
//         .triggers       = triggers,      // the game's own triggers
//         .bindings       = &bindings,     // a ScriptKit::Bindings<State>: the game's values
//         .handlers       = &handlers,     // an ActorKit::Handlers<State>: the game's commands
//     };
//
// A config is the argument tick and Sprites take. One config serves any number of Actors values, and an
// Actors value takes whichever config a call hands it. Every field but `world` is optional.
#pragma once

#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <typeinfo>

#include "ScriptKit/bindings.h"
#include "ScriptKit/command.h"
#include "ScriptKit/value.h"

#include "WorldKit/area.h"

#include "ActorKit/actors.h"
#include "ActorKit/commands.h"
#include "ActorKit/triggers.h"
#include "ActorKit/values.h"

namespace ActorKit {

// The game's bindings, held by a Config: made from the address of a ScriptKit::Bindings<State>, whatever
// the game's State type is. tick and Sprites use it with the state they are handed, and throw
// std::logic_error when that state is of another type than the bindings were written for.
class AnyBindings {
public:
    // Empty, for a game in which every value the scripts name is an actor's own or one of ActorKit's.
    AnyBindings() = default;

    template <class State>
    AnyBindings(const ScriptKit::Bindings<State>* bindings)
        : bindings_(bindings),
          type_(&typeid(State)),
          gather_([](const void* held, ScriptKit::Values& values, const void* state,
                     std::span<const ScriptKit::ValueId> ids) {
              static_cast<const ScriptKit::Bindings<State>*>(held)->gather(values, *static_cast<const State*>(state), ids);
          }),
          apply_([](const void* held, void* state, const ScriptKit::Request& request) {
              static_cast<const ScriptKit::Bindings<State>*>(held)->apply(*static_cast<State*>(state), request);
          }) {}

    [[nodiscard]] explicit operator bool() const noexcept { return bindings_ != nullptr; }

    // ScriptKit::Bindings::gather, with `state`.
    template <class State>
    void gather(ScriptKit::Values& values, const State& state, std::span<const ScriptKit::ValueId> ids) const {
        check(typeid(State));
        gather_(bindings_, values, &state, ids);
    }

    // ScriptKit::Bindings::apply, with `state`.
    template <class State>
    void apply(State& state, const ScriptKit::Request& request) const {
        check(typeid(State));
        apply_(bindings_, &state, request);
    }

private:
    void check(const std::type_info& state) const {
        if (*type_ != state) {
            throw std::logic_error("ActorKit: the config's bindings are written for another state type than the "
                                   "one handed to this call");
        }
    }

    const void* bindings_ = nullptr;
    const std::type_info* type_ = nullptr;
    void (*gather_)(const void*, ScriptKit::Values&, const void*, std::span<const ScriptKit::ValueId>) = nullptr;
    void (*apply_)(const void*, void*, const ScriptKit::Request&) = nullptr;
};

// The game's handlers, held by a Config: made from the address of an ActorKit::Handlers<State>, as
// AnyBindings is made from bindings, and checked against the state the same way.
class AnyHandlers {
public:
    // Empty, for a game whose scripts ask only for ActorKit's own commands, spawn and remove.
    AnyHandlers() = default;

    template <class State>
    AnyHandlers(const Handlers<State>* handlers)
        : handlers_(handlers),
          type_(&typeid(State)),
          apply_([](const void* held, void* state, Id asker, const ScriptKit::Request& request) {
              static_cast<const Handlers<State>*>(held)->apply(*static_cast<State*>(state), asker, request);
          }) {}

    [[nodiscard]] explicit operator bool() const noexcept { return handlers_ != nullptr; }

    // Handlers::apply, with `state`.
    template <class State>
    void apply(State& state, Id asker, const ScriptKit::Request& request) const {
        if (*type_ != typeid(State)) {
            throw std::logic_error("ActorKit: the config's handlers are written for another state type than the "
                                   "one handed to this call");
        }
        apply_(handlers_, &state, asker, request);
    }

private:
    const void* handlers_ = nullptr;
    const std::type_info* type_ = nullptr;
    void (*apply_)(const void*, void*, Id, const ScriptKit::Request&) = nullptr;
};

struct Config {
    // The world the actors are in. It outlives the config.
    const WorldKit::World* world = nullptr;

    // Which of the game's input actions ActorKit::Interact is: a script whose `on` is Interact starts
    // when an actor facing its actor performs this action. Interact starts scripts once this is set.
    std::optional<ActionName> interactAction;

    // The game's own triggers, each bound to what ActorKit detects. ActorKit's own are in every map.
    TriggerMap triggers;

    // How the game's values are read and written: `&bindings`, the address of a ScriptKit::Bindings of
    // the game's state type. The bindings outlive the config.
    AnyBindings bindings;

    // What the game does for each of its commands: `&handlers`, the address of an ActorKit::Handlers of
    // the game's state type. The handlers outlive the config.
    AnyHandlers handlers;
};

namespace detail {

// Fills `values` with everything the actor `live` reads: the game's values through `bindings`, its own,
// and ActorKit's six, worked out against `followed`, the followed actor or null. Throws
// std::out_of_range naming the actor and the value for an unbound value: one outside the actor's own,
// ActorKit's six and the game's bindings.
template <class State>
void gatherValues(ScriptKit::Values& values, const Live& live, const Live* followed, const AnyBindings& bindings,
                  const State& state) {
    const auto unbound = [&](ScriptKit::ValueId id) {
        return std::out_of_range("ActorKit: actor " + std::to_string(live.id) + " reads the value with id " +
                                 std::to_string(id) + ", which is neither its own, ActorKit's, nor bound");
    };
    values.clear();
    if (!live.gameReads.empty()) {
        if (!bindings) {
            throw unbound(live.gameReads.front());
        }
        try {
            bindings.gather(values, state, live.gameReads);
        } catch (const std::out_of_range&) {
            ScriptKit::Values one;
            for (const ScriptKit::ValueId id : live.gameReads) {
                try {
                    bindings.gather(one, state, std::span<const ScriptKit::ValueId>(&id, 1));
                } catch (const std::out_of_range&) {
                    throw unbound(id);
                }
            }
            throw;
        }
    }
    for (const OwnValue& value : live.actor->ownValues) {
        values.setRaw(value.id(), live.own.raw(value.id()));
    }
    const bool here = followed != nullptr && followed->place == live.place;
    values.set(Touched, live.touched);
    values.set(X, live.position.x);
    values.set(Y, live.position.y);
    values.set(FollowedHere, here);
    values.set(FollowedX, here ? followed->position.x : live.position.x);
    values.set(FollowedY, here ? followed->position.y : live.position.y);
}

}  // namespace detail

}  // namespace ActorKit
