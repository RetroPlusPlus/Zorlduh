// Actor — what an actor is: a plain value the game declares anywhere, or makes with a function, and
// hands to Actors::spawn as often as it likes.
//
//     const ActorKit::Actor bell{.slot = "actors", .box = {0, 8, 16, 8}, .solid = true,
//                                .look = {{BellRung, swinging}, {still}}, .scripts = {&ring}};
//
//     ActorKit::Actor hen(retropp::PaletteId colors, retropp::Dir flees) {     // any hen, in any colors
//         return {.slot = "actors", .solid = true,
//                 .ownValues = {{Heading, retropp::Dir::None}},
//                 .scripts   = {scatter(flees)},
//                 .mover     = ActorKit::movedBy<TopDownMovement::Walker>(henWalk(colors), Heading)};
//     }
//
// A sign, a gate, a hen and the hero are the same type. An object is an actor whose `mover` is left
// empty.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#include "retropp/geometry.h"
#include "retropp/input.h"

#include "ScriptKit/expression.h"
#include "ScriptKit/script.h"

#include "ActorKit/look.h"
#include "ActorKit/mover.h"
#include "ActorKit/triggers.h"
#include "ActorKit/values.h"

namespace ActorKit {

// The number that names one live actor. An Actors value hands them out from 1, in spawn order, and
// hands each out once; 0 is kNoId, which stands where an id is absent.
using Id = std::uint32_t;
inline constexpr Id kNoId = 0;

// The side of an actor something happens from, as a script's `from` names it. A plain enum, so
// `.from = ActorKit::Below` builds; Any, 0, is ScriptKit's default and matches every side.
enum Side : int { Any, Above, Below, Left, Right };

// One of an actor's scripts: a constant, by address (&ring), or a script made by a function, by value
// (pressWhile(Pressed, OnPlate)). A script given by value is kept, and every copy of the actor and every
// actor spawned from it name that one script.
class ScriptRef {
public:
    ScriptRef(const ScriptKit::Script* script) noexcept
        : script_(std::shared_ptr<const ScriptKit::Script>(script, [](const ScriptKit::Script*) {})) {}
    ScriptRef(ScriptKit::Script script) : script_(std::make_shared<const ScriptKit::Script>(std::move(script))) {}

    // The script; null for a ScriptRef made from a null pointer.
    [[nodiscard]] const ScriptKit::Script* get() const noexcept { return script_.get(); }

    // What keeps a script given by value alive.
    [[nodiscard]] const std::shared_ptr<const ScriptKit::Script>& shared() const noexcept { return script_; }

private:
    std::shared_ptr<const ScriptKit::Script> script_;
};

// An actor's scripts, in order. Each runs in a copy of its own for every actor spawned.
//
// To share one script among many actors — instances of one value, or different kinds — write a
// function that takes the values the script acts on and returns it:
//
//     ScriptKit::Script pressWhile(ScriptKit::Value<bool> pressed, ScriptKit::Value<bool> held) {
//         return {.name = "press", .on = Touch, .body = {set(pressed, true), waitWhile(held), set(pressed, false)}};
//     }
//     const ActorKit::Actor plate{.ownValues = {{Pressed, false}}, .scripts = {pressWhile(Pressed, ActorKit::Touched)}};
//     const ActorKit::Actor lever{.ownValues = {{Pulled, false}},  .scripts = {pressWhile(Pulled, HandOnLever)}};
//
// A script made this way has an address of its own, so a trigger starts it and the scripts that name
// others by address — `call`, `stop`, `restart`, `interrupts` — name constants.
using Scripts = std::vector<ScriptRef>;

// One action an actor performs, with what drives it: {Action::Interact, &Input::interact}. The actor
// performs the action on a tick the member of the game's input value is true.
class ActionRow {
public:
    template <retropp::ActionLike A, class Input>
    ActionRow(A action, bool Input::* drive)
        : action_(retropp::actionId(action)),
          type_(&typeid(Input)),
          read_([drive](const void* input) { return static_cast<const Input*>(input)->*drive; }) {}

    [[nodiscard]] retropp::ActionId action() const noexcept { return action_; }

    // Whether the action is performed this tick, read from `input`, whose type is `type`. Throws
    // std::logic_error when `type` is another type than the one the row was written for.
    [[nodiscard]] bool performed(const void* input, const std::type_info& type) const {
        if (type != *type_) {
            throw std::logic_error("ActorKit: an action is driven by a member of an input type other than the "
                                   "one handed to tick");
        }
        return read_(input);
    }

private:
    retropp::ActionId action_;
    const std::type_info* type_;
    std::function<bool(const void*)> read_;
};

// The actions an actor performs.
using Actions = std::vector<ActionRow>;

// What an actor is. An aggregate, written with designated initializers in the order below; every field
// is optional. The game's own types stay behind `mover` and `actions`, which hold what movedBy and an
// action row made from them. Copies are cheap and independent.
struct Actor {
    // The slot its sprite is reported under: Sprites (sprites.h) returns the actors of one slot. In a
    // WorldKit world it is the name of one of the area's `actorSlots`, the one the game draws them at.
    std::string slot;

    // What of it collides and is touched, relative to its position. An actor with a mover takes its
    // box from the mover — the bounds of the mover's collision shape — and leaves this empty.
    retropp::IntRect box{};

    // Whether walkers stop against it: an expression over its values, its own or the game's.
    ScriptKit::Expr<bool> solid = false;

    // The values each actor spawned from this one holds its own instance of, with their starting worths.
    OwnValues ownValues;

    // What it shows. Left empty, the actor shows its mover's frame.
    Looks look;

    // Its scripts.
    Scripts scripts;

    // What moves it; empty for an actor that stays where it is put.
    Mover mover;

    // The actions it performs on the actor it faces.
    Actions actions;

    // The triggers its body sets off in other actors: an actor that touches or comes near another sets
    // off the touch and near triggers its `setsOff` lists — ActorKit's own, {ActorKit::On::Touch,
    // ActorKit::On::Near}, or the game's.
    TriggerKinds setsOff;
};

}  // namespace ActorKit
