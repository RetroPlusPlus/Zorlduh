# Scripts

How an actor's scripts are written and run: the scripts on an actor, the three kinds of value they read
and set, when what they ask for is written, sharing one script among many actors, and commands.

A script is a `ScriptKit::Script`. ScriptKit's `docs/scripts.md` covers what a script is written in.
This page covers what ActorKit adds.

```cpp
#include "ActorKit/values.h"     // OwnValue, OwnValues, Touched, X, Y, PlayerHere, PlayerX, PlayerY
#include "ActorKit/commands.h"   // spawn, remove, Handler, Handlers
```

`actor.h` includes `values.h`, and `config.h` includes `commands.h`.

## Contents

- [Overview](#overview)
- [Scripts on an actor](#scripts-on-an-actor)
- [Values](#values)
- [When a request is written](#when-a-request-is-written)
- [Share a script with a function](#share-a-script-with-a-function)
- [Commands](#commands)
- [Count with a command](#count-with-a-command)
- [`OwnValue`](#ownvalue)
- [The six values](#the-six-values)
- [`spawn` and `remove`](#spawn-and-remove)
- [`Handlers`](#handlers)

## Overview

```cpp
// A value each slime owns, and a command of the game's.
const ScriptKit::Value<int> Health;
const ScriptKit::Command Collect{"collect"};

// A coin: touched, it is counted and removed.
const ScriptKit::Script pickUp{.name = "pick up", .on = ActorKit::Touch, .body = {Collect(), ActorKit::remove()}};
const ActorKit::Actor coin{.slot = "actors", .box = {.x = 4, .y = 4, .width = 8, .height = 8},
                           .look = coinLook, .scripts = {&pickUp}};

// A slime: each hit takes one of its three health. At zero it leaves a coin and is removed.
const ScriptKit::Script hurt{
    .name = "hurt",
    .on = Hit,
    .body = {set(Health, Health - 1), when(Health <= 0, {ActorKit::spawn(coin), ActorKit::remove()})},
};
const ActorKit::Actor slime{.slot = "actors", .box = {.x = 0, .y = 8, .width = 16, .height = 8}, .solid = true,
                            .ownValues = {{Health, 3}}, .look = slimeLook, .scripts = {&hurt}};

// What the game does for its command.
const ActorKit::Handlers<State> handlers{
    {Collect, [](State& state, ActorKit::Id, const ScriptKit::Arguments&) { ++state.coins; }},
};
const ActorKit::Config config{.world = &world, .triggers = triggers, .handlers = &handlers};
```

Names from your game:

| Name | What it is |
|---|---|
| `Hit` | a trigger of your own, bound in `triggers` to `ActorKit::actedOn(Action::Attack)` ([triggers.md](triggers.md#triggers-of-your-own)) |
| `triggers` | your `ActorKit::TriggerMap` |
| `coinLook`, `slimeLook` | `ActorKit::Look`s |
| `State` | your game state's type, with an `int coins` member |
| `world` | your `WorldKit::World` |

Every slime spawned from `slime` has its own `Health`. The third hit on one slime brings that slime's
`Health` to 0, and two ticks later a coin stands where the slime was. A hero that walks onto the coin
adds one to `state.coins`, and the coin is removed.

## Scripts on an actor

An actor lists its scripts in `scripts`. Each actor spawned runs its own copy of each one: its own
place in the body and its own waits.

| A script's field | What ActorKit does with it |
|---|---|
| `on` | Starts the script when that trigger happens to the actor ([triggers.md](triggers.md)). |
| `from` | For an acted-on trigger, starts the script from that side only ([actors.md](actors.md#side)). |
| `when` | Tests it at the moment the trigger happens. The script starts if it holds. |
| `body` | Runs it, one step per tick, in step 6 of the [tick](actors.md#tick). |
| `priority`, `interrupts` | ScriptKit's, among the scripts of the one actor (ScriptKit's `docs/scripts.md`, "Script"). |

- **An actor is live wherever it is.** Its scripts start and run in every area, whichever area the
  camera is in.
- **A script reaches its own actor's scripts by name.** `call`, `stop`, `restart` and `interrupts`
  name scripts of the same actor's list, by address. List those scripts by address:
  `.scripts = {&patrol, &flee}`.
- **A script reaches other actors through values and commands.** It sets a value another actor reads,
  or asks for a command your handler carries out.
- **The entry functions** `wait` and `loop` take a number or a braced list, so name them where you
  write scripts (ScriptKit's `docs/scripts.md`, "Finding the entry functions"):

```cpp
using ScriptKit::loop;
using ScriptKit::wait;
using namespace std::chrono_literals;
```

## Values

A script reads and sets `ScriptKit::Value`s. For each value a script names, ActorKit decides where its
worth is kept:

| The value is | Kind | Worth kept |
|---|---|---|
| listed in the actor's `ownValues` | the actor's own | in the actor: one instance for each actor |
| one of `Touched`, `X`, `Y`, `PlayerHere`, `PlayerX`, `PlayerY` | ActorKit's | worked out for the actor, every tick |
| bound in your `ScriptKit::Bindings` | your game's | in your state: one instance |

Any other value makes `tick` throw `std::out_of_range`, naming the actor and the value's id.

### An actor's own values

```cpp
const ScriptKit::Value<bool> Awake;
const ScriptKit::Value<retropp::Dir> Heading;

const ActorKit::Actor dog{.slot = "actors",
                          .ownValues = {{Awake, false}, {Heading, retropp::Dir::None}},
                          .scripts = {&wake}};
```

Each element of `ownValues` is a value and the worth every new actor starts it at. The actor's scripts,
its `solid`, its looks and its mover's drive all read the actor's own instance.

- One `Value` constant serves any number of actors and kinds of actor. A dog and a hen can both list
  `Heading`, and each actor has its own.
- From your code, read and write an actor's own value with
  [`Actors::get` and `set`](actors.md#get-and-set).
- Use an own value for anything that differs from one actor to the next: health, a timer, a heading,
  whether this door is open.

### Your game's values

```cpp
const ScriptKit::Value<bool> GateOpen;
const ScriptKit::Value<int> Keys;
const ScriptKit::Value<bool> Night;

const ScriptKit::Bindings<State> bindings{
    {GateOpen, &State::gateOpen},                                   // a member: read and written
    {Keys, &State::keys},
    {Night, [](const State& state) { return state.hour >= 20; }},   // a getter: read only
};

const ActorKit::Config config{.world = &world, .bindings = &bindings};
```

`State` has the members `bool gateOpen`, `int keys` and `int hour`. `ScriptKit::Bindings` is ScriptKit's
(its `docs/commands.md`, "Bindings"): each row says how one value is read from your state and written
to it.

- There is one instance of each of these values. Every actor's scripts read the same worth.
- A value bound with a getter alone is read-only. A script's `set` of it leaves it as it is.
- Use a value of your game's for what actors share: a switch that opens a gate elsewhere, the time of
  day, a quest flag.

## When a request is written

A `set`, a command, `ActorKit::spawn` and `ActorKit::remove` are requests. A script records a request
when it reaches the entry, in step 6 of a tick. The request is carried out in step 1 of the next tick.

The slime's `hurt` script, hit on tick N with its `Health` at 1:

| Tick | Step | What happens |
|---|---|---|
| N | 4 | The hero's attack starts `hurt`. |
| N | 6 | `hurt` asks for `Health` to become 0. Its next entry reads `Health`, so the script stops there for this tick. |
| N + 1 | 1 | `Health` is written: 0. |
| N + 1 | 6 | The `when` reads 0, and `hurt` asks for the coin and for its own removal. |
| N + 2 | 1 | The coin is spawned and the slime is removed. |

- **Requests are written in order:** actor by actor in spawn order, and each actor's in the order its
  scripts asked.
- **A script that has asked for something reads on the next tick.** After a `set` or a command, the
  script stops at the next entry that reads a value, and runs that entry a tick later, when the request
  has been written (ScriptKit's `docs/script-processor.md`, "Reading after asking").
- **A hit that lands while `hurt` is still running is missed.** Here that is a hit on tick N + 1
  ([triggers.md](triggers.md#how-a-trigger-starts-a-script)).
- **Two scripts that set the same value on one tick both read the old worth.** The later write
  replaces the earlier. To add things up, [count with a command](#count-with-a-command).

## Share a script with a function

To use one script on many actors, write a function that takes the values the script acts on and
returns the script:

```cpp
// Touched, it makes `pressed` true, holds while `held` does, and makes it false again.
ScriptKit::Script pressWhile(ScriptKit::Value<bool> pressed, ScriptKit::Value<bool> held) {
    return {.name = "press", .on = ActorKit::Touch,
            .body = {set(pressed, true), waitWhile(held), set(pressed, false)}};
}

const ActorKit::Actor plate{.slot = "underfoot", .box = {.x = 0, .y = 0, .width = 16, .height = 16},
                            .ownValues = {{Pressed, false}},
                            .look = {{Pressed, plateDown}, {plateUp}},
                            .scripts = {pressWhile(Pressed, ActorKit::Touched)}};
const ActorKit::Actor lever{.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 16},
                            .ownValues = {{Pulled, false}},
                            .look = {{Pulled, leverDown}, {leverUp}},
                            .scripts = {pressWhile(Pulled, ActorKit::Touched)}};
```

`Pressed` and `Pulled` are `ScriptKit::Value<bool>` constants and the four looks are
`ActorKit::Look`s. The plate and the lever are two kinds of actor with one script between them. Each
hands the function the value it wants set: the plate its own `Pressed`, the lever its own `Pulled`.

This is the recommended way to share a script. A script constant names fixed values. A function lets
each actor say which values the script acts on, and anything else that varies:

```cpp
// A hen's first move: it runs the way it is handed for half a second.
ScriptKit::Script scatter(retropp::Dir flees) {
    return {.name = "scatter", .on = ActorKit::Enter,
            .body = {set(Heading, flees), wait(500ms), set(Heading, retropp::Dir::None)}};
}
```

| A parameter can be | Used in the body as |
|---|---|
| a `ScriptKit::Value` | the value a `set`, a `waitWhile` or a condition names |
| a number, a direction, any enumerator | what a `set` writes, or a command's argument |
| a duration or a count of ticks | a `wait` |
| an `ActorKit::Actor` | what `ActorKit::spawn` spawns |

Watch for:

- **A script made by a function is started by its trigger alone.** The script is listed by value and
  has an address of its own, and `call`, `stop`, `restart` and `interrupts` name a script by address.
  Scripts that name each other are constants, listed by address.
- **Two scripts from one function have one name.** An actor that lists
  `pressWhile(Pressed, ActorKit::Touched)` and `pressWhile(Pulled, ActorKit::Touched)` has two scripts
  named `"press"`, and `spawn` throws `std::logic_error`. Give the function a name parameter for an
  actor that lists it twice.
- **Build the actor value once.** Every actor spawned from one value shares the one script. Calling
  the function again makes another script, which the `Actors` value compiles and keeps.

## Commands

A command is something a script asks your game to do. Declare it as a `ScriptKit::Command` constant and
call it in a body (ScriptKit's `docs/commands.md`, "Command"):

```cpp
const ScriptKit::Command Say{"say"};

const ScriptKit::Script greet{.name = "greet", .on = ActorKit::Interact, .body = {Say("Hello!")}};

const ActorKit::Handlers<State> handlers{
    {Say, [](State& state, ActorKit::Id asker, const ScriptKit::Arguments& arguments) {
         state.message = {.text = arguments.text(0), .at = state.actors.position(asker)};
     }},
};
```

`State` has `actors`, your `ActorKit::Actors`, and `message`, a struct of yours with a
`std::string_view text` and a `retropp::Vec2i at`.

| The script asks for | Who carries it out |
|---|---|
| `ActorKit::spawn(actor, offset)` | ActorKit ([`spawn` and `remove`](#spawn-and-remove)) |
| `ActorKit::remove()` | ActorKit |
| any command of yours | your handler for it, which is handed the asking actor's id ([`Handlers`](#handlers)) |

- A handler runs in step 1 of the tick after the script asked.
- The id says which actor asked. Ask the set about it: `state.actors.position(asker)`.
- A command is the way a script does anything beyond setting a value: show a message, play a sound,
  give an item, start a battle.

## Count with a command

To count things that can happen on the same tick, use a command.

A `set` of a shared value loses counts. Three eggs touched on one tick:

```cpp
// Each of the three scripts reads the same Eggs, and each asks for that plus one.
const ScriptKit::Script pickUp{.name = "pick up", .on = ActorKit::Touch,
                               .body = {set(Eggs, Eggs + 1), ActorKit::remove()}};
```

All three eggs are removed and `Eggs` goes up by one.

A command is carried out once for every script that asks:

```cpp
const ScriptKit::Command Collect{"collect"};

const ScriptKit::Script pickUp{.name = "pick up", .on = ActorKit::Touch,
                               .body = {Collect(), ActorKit::remove()}};

const ActorKit::Handlers<State> handlers{
    {Collect, [](State& state, ActorKit::Id, const ScriptKit::Arguments&) { ++state.eggs; }},
};
```

All three eggs are removed and `state.eggs` goes up by three.

| Use | For |
|---|---|
| a command | pickups, hits, kills, score: anything several actors add to |
| a `set` | a value one script writes at a time: an actor's own value, a switch, a flag |

## `OwnValue`

```cpp
class OwnValue {
public:
    template <ScriptKit::ValueType T>
    OwnValue(ScriptKit::Value<T> value, T worth) noexcept;

    ScriptKit::ValueId id() const noexcept;
    std::int64_t start() const noexcept;
    bool apply(const ScriptKit::Request& request, ScriptKit::Values& own) const;
};

using OwnValues = std::vector<OwnValue>;
```

One value an actor owns, with its starting worth. `OwnValues` is the type of `Actor::ownValues`. You
write braces:

```cpp
.ownValues = {{Health, 3}, {Awake, false}, {Heading, retropp::Dir::None}},
```

- **The worth has exactly the value's type:** an `int` for a `Value<int>`, a `bool` for a
  `Value<bool>`, the enum's own enumerator for an enum. `{Health, true}` fails to compile.
- **A value listed twice** makes `spawn` throw `std::invalid_argument`.
- `id`, `start` and `apply` are what `spawn` and `tick` call.

## The six values

```cpp
inline const ScriptKit::Value<bool> Touched;
inline const ScriptKit::Value<int> X;
inline const ScriptKit::Value<int> Y;
inline const ScriptKit::Value<bool> PlayerHere;
inline const ScriptKit::Value<int> PlayerX;
inline const ScriptKit::Value<int> PlayerY;
```

Values ActorKit fills in for each actor. A script reads them like any value, and each actor's scripts
read that actor's own worths.

| Value | Worth, for the actor whose script reads it |
|---|---|
| `ActorKit::Touched` | True on a tick an actor that sets off a touch trigger overlaps this actor's box. |
| `ActorKit::X`, `ActorKit::Y` | This actor's position: the top-left of its art, in its area's pixels. |
| `ActorKit::PlayerHere` | True when the [player](actors.md#player) is in this actor's area. |
| `ActorKit::PlayerX`, `ActorKit::PlayerY` | The player's position, while it is in this actor's area. Otherwise this actor's own position. |

```cpp
// The dog trots right until it is beside the bell, then stops.
const ScriptKit::Script answer{
    .name = "answer",
    .on = ActorKit::Becomes(BellRung),
    .body = {
        loopWhile(ActorKit::X < kBellX - 16, {set(Heading, retropp::Dir::Right), wait(1)}),
        set(Heading, retropp::Dir::None),
    },
};
```

`BellRung` is a `ScriptKit::Value<bool>` of your game's, `Heading` a `ScriptKit::Value<retropp::Dir>`
the dog owns and its mover is driven by, and `kBellX` an `int`.

- **They are read-only.** A script's `set` of one is dropped.
- **They work wherever a value does:** in a body, a `when`, an actor's `solid`, a look's condition, a
  mover's drive, and `ActorKit::Becomes(ActorKit::Touched)`.
- **With the player in another area, or before a player is recorded,** `PlayerHere` is false and
  `PlayerX` and `PlayerY` equal `X` and `Y`. An actor that walks toward the player's position then
  stands where it is.
- **`X` and `Y` in a script are the position after this tick's walking.** Scripts run in step 6, after
  step 3 has walked the actor.

## `spawn` and `remove`

```cpp
ScriptKit::Entry spawn(const Actor& actor, retropp::Vec2i offset = {});
ScriptKit::Entry remove();
```

The two commands ActorKit carries out itself. Each returns an entry for a script's body.

```cpp
// Every four seconds the hen lays an egg at its feet, three in all.
ScriptKit::Script lay(const ActorKit::Actor& egg) {
    return {.name = "lay", .on = ActorKit::Enter,
            .body = {loopWhile(Laid < 3, {wait(4s), ActorKit::spawn(egg, {0, 6}), set(Laid, Laid + 1)})}};
}
```

`Laid` is a `ScriptKit::Value<int>` the hen owns.

| Entry | What it asks for |
|---|---|
| `ActorKit::spawn(actor, offset)` | A copy of `actor` in the asking actor's area, at the asking actor's position plus `offset`. |
| `ActorKit::remove()` | The removal of the asking actor. |

A spawn asked for on tick N:

| Tick | What happens |
|---|---|
| N | The script reaches the entry and asks. |
| N + 1 | Step 1 spawns the actor. `Sprites` reports it from this tick's frame. |
| N + 2 | The new actor takes part in the tick. Its `Enter` scripts start here, if it is in the camera's area. |

- **`spawn` copies `actor` when the entry is written,** and keeps the copy for the life of the program.
  A value made on the spot works: `ActorKit::spawn(Egg(art))`. Write the scripts that spawn once, at
  startup.
- **`remove` takes effect once the tick's requests are all written.** What the same script asked for
  after its `remove`, on the same tick, is still carried out.
- **Write both qualified:** `ActorKit::spawn`, `ActorKit::remove`.
- From your own code, call [`Actors::spawn`](actors.md#spawn) and [`Actors::remove`](actors.md#remove).

## `Handlers`

```cpp
template <class State>
class Handler {
public:
    template <class Run> requires std::invocable<Run, State&, Id, const ScriptKit::Arguments&>
    Handler(const ScriptKit::Command& command, Run run);
};

template <class State>
class Handlers {
public:
    Handlers(std::initializer_list<Handler<State>> handlers);

    void apply(State& state, Id asker, const ScriptKit::Request& request) const;
};
```

What your game does for each of its commands: one function per command. Build it once, at startup, and
put its address in `Config::handlers`.

```cpp
const ActorKit::Handlers<State> handlers{
    {Collect, [](State& state, ActorKit::Id, const ScriptKit::Arguments&) { ++state.coins; }},
    {Say, [](State& state, ActorKit::Id asker, const ScriptKit::Arguments& arguments) {
         state.message = {.text = arguments.text(0), .at = state.actors.position(asker)};
     }},
};

const ActorKit::Config config{.world = &world, .handlers = &handlers};
```

A handler's function takes three things:

| Parameter | What it is |
|---|---|
| `State& state` | Your game state, the one handed to `tick`. |
| `ActorKit::Id asker` | The actor whose script asked. |
| `const ScriptKit::Arguments& arguments` | The command's arguments, each read by kind: `text(n)`, `number(n)`, `truth(n)`, `word<E>(n)` (ScriptKit's `docs/commands.md`, "Arguments"). |

- **`ActorKit::Handlers` is the type `Config` takes.** It is `ScriptKit::Handlers` with the asking
  actor's id added.
- **A handler can change the set.** `state.actors.spawn`, `moveTo` and `set` act at once. A `remove`
  takes effect once the tick's requests are all written.
- **A command listed twice** keeps its first function.
- **The commands outlive the handlers.** Declare each `ScriptKit::Command` as a constant.
- `apply` is the call `tick` makes. It throws `std::out_of_range`, naming the command, for a command
  outside the list. `tick` throws the same when `Config::handlers` is empty and a script asks for one
  of your commands.
