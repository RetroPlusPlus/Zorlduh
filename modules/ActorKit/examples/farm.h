// What the farmyard's scripts are written in: its places, ActorKit's triggers, its values and its one
// command. Each script is a file of its own in scripts/, and every one of them includes this.
#pragma once

#include <chrono>
#include <cstdint>

#include "retropp/input_actions.h"

#include "ScriptKit/entry.h"
#include "ScriptKit/expression.h"
#include "ScriptKit/script.h"
#include "ScriptKit/trigger.h"
#include "ScriptKit/value.h"

#include "ActorKit/actor.h"
#include "ActorKit/commands.h"
#include "ActorKit/triggers.h"
#include "ActorKit/values.h"

namespace farm {

// A bare count and a time are plain C++ types, so name lookup finds wait once it is named here; the
// literals are what let a script say wait(500ms).
using ScriptKit::wait;
using namespace std::chrono_literals;

// The farm's scripts start on ActorKit's own triggers. Interact is the hero pressing the program's
// Interact action, which getting_started.cpp names to the trigger map.
using ActorKit::Becomes;    // a value goes from false to true
using ActorKit::Enter;      // the camera comes into the actor's area
using ActorKit::Interact;   // the hero, facing the actor, presses Interact
using ActorKit::Near;       // the hero comes within a distance
using ActorKit::Touch;      // the hero walks onto the actor

// The farm's one layout, by its index in the world, and its three areas, by their index in the layout.
enum Layout : std::uint8_t { Farm };
enum Area : std::uint8_t { Yard, Field, Pond };

// Where the bell and the dog's kennel stand in the yard, in its pixels.
inline constexpr int kBellX = 72;
inline constexpr int kKennelX = 32;

// The farm's values: one instance each, kept in its state and bound there.
inline const ScriptKit::Value<bool> GateOpen;    // the gate to the field stands open
inline const ScriptKit::Value<bool> BellRung;    // the bell is ringing

// The farm's one command: count an egg as picked up. A command is done once for every script that asks,
// so three eggs picked up on one tick count as three; getting_started.cpp has the handler.
inline const ScriptKit::Command Collect{"collect"};
inline ScriptKit::Entry collect() { return Collect(); }

// The farm's second command: ripple the asking actor's sprite from a point in the sprite's own art
// pixels, `x` across and `y` down from its top-left. getting_started.cpp has the handler.
inline const ScriptKit::Command Ripple{"ripple"};
inline ScriptKit::Entry ripple(ScriptKit::Expr<int> x, ScriptKit::Expr<int> y) { return Ripple(x, y); }

// The middle of the hero's feet, from its top-left: the middle of its collision box.
inline constexpr int kFeetX = 8;
inline constexpr int kFeetY = 13;

// How often the pond looks for a step: the length of one frame of the hero's walk.
inline constexpr int kStepTicks = 8;

// Values an actor owns: each actor that lists one in its ownValues holds its own instance.
inline const ScriptKit::Value<bool> Awake;            // the dog's
inline const ScriptKit::Value<bool> Splashing;        // the puddle's
inline const ScriptKit::Value<int> Laid;              // each hen's: the eggs it has laid
inline const ScriptKit::Value<retropp::Dir> Heading;  // the dog's and each hen's: where its walker heads
inline const ScriptKit::Value<int> SteppedX;          // the pond's: where the hero stood at its last ripple
inline const ScriptKit::Value<int> SteppedY;

}  // namespace farm
