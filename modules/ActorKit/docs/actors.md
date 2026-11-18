# Actors reference

Every type and function in `actor.h`, `actors.h`, `config.h` and `tick.h`: the actor value, a set of
live actors, the config and the tick. For triggers see [triggers.md](triggers.md), for values and
commands [scripts.md](scripts.md), for movers [movers.md](movers.md), and for looks and sprites
[drawing.md](drawing.md).

```cpp
#include "ActorKit/actor.h"    // Actor, Id, kNoId, Side, ScriptRef, Scripts, ActionRow, Actions
#include "ActorKit/actors.h"   // Actors
#include "ActorKit/config.h"   // Config, AnyBindings, AnyHandlers
#include "ActorKit/tick.h"     // tick
```

`tick.h` includes the other three.

## Contents

- [`Actor`](#actor)
- [`Id`](#id)
- [`Side`](#side)
- [`ScriptRef` and `Scripts`](#scriptref-and-scripts)
- [`ActionRow` and `Actions`](#actionrow-and-actions)
- [`Actors`](#actors)
- [`Config`](#config)
- [`tick`](#tick)

## `Actor`

```cpp
struct Actor {
    std::string slot;
    retropp::IntRect box{};
    ScriptKit::Expr<bool> solid = false;
    OwnValues ownValues;
    Looks look;
    Scripts scripts;
    Mover mover;
    Actions actions;
    TriggerKinds setsOff;
};
```

Describes a kind of actor. Build one per kind, after the art is loaded, and hand it to
[`Actors::spawn`](#spawn) once for each actor you want.

| Field | Meaning | Default |
|---|---|---|
| `slot` | The name its sprite is reported under: [`Sprites`](drawing.md#sprites) returns the actors of one slot. In a WorldKit world it is the name of one of the area's `actorSlots`. | `""` |
| `box` | The rectangle that collides, is touched and is acted on, relative to the actor's position. | empty |
| `solid` | Whether walking actors stop against the box: `true`, `false`, or an expression over the actor's values. Evaluated every tick. | `false` |
| `ownValues` | The values each actor spawned from this one holds its own instance of, each with its starting worth ([scripts.md](scripts.md#ownvalue)). | empty |
| `look` | What it shows: one `Look`, or a list of looks with conditions ([drawing.md](drawing.md#lookrule-and-looks)). | empty: the actor shows its mover's frame |
| `scripts` | Its scripts ([`Scripts`](#scriptref-and-scripts)). | empty |
| `mover` | What moves it, made by [`movedBy`](movers.md#movedby). | empty: the actor stays where it is put |
| `actions` | The actions it performs on the actor it faces ([`Actions`](#actionrow-and-actions)). | empty |
| `setsOff` | The touch and near triggers its body sets off in other actors ([triggers.md](triggers.md#the-detections)). | empty |

```cpp
const ActorKit::Actor chest{
    .slot      = "actors",
    .box       = {.x = 0, .y = 8, .width = 16, .height = 8},
    .solid     = true,
    .ownValues = {{Opened, false}},
    .look      = {{Opened, chestOpen}, {chestShut}},
    .scripts   = {&openChest},
};
```

`Opened` is a `ScriptKit::Value<bool>`, `chestOpen` and `chestShut` are `ActorKit::Look`s, and
`openChest` is a `ScriptKit::Script`. With art 16 pixels square, this box is the bottom half of the
chest: a walker stops at its base and overlaps its lid.

Watch for:

- **Initializer order.** C++ requires designated initializers in declaration order: `slot`, `box`,
  `solid`, `ownValues`, `look`, `scripts`, `mover`, `actions`, `setsOff`.
- **A box beside a mover.** An actor with a mover takes its box from the mover: the bounds of the
  mover's collision shape. Leave `box` empty for it. `spawn` throws `std::invalid_argument` for an actor
  with both.
- **An empty box.** Touch, Interact and solidity all work through the box. Give a box to every actor
  that is touched, acted on or solid.
- **`slot`.** Each slot name you use needs a `Sprites` call for it in your frame.
- **Copying.** Copies are cheap and independent. A copy shares the scripts given by value and what
  `movedBy` captured.

## `Id`

```cpp
using Id = std::uint32_t;
inline constexpr Id kNoId = 0;
```

The number that names one live actor. `spawn` returns it, and every `Actors` member that asks about an
actor takes it.

- An `Actors` value hands out ids from 1, in spawn order.
- Each id is handed out once. After `remove`, the id stays retired, so an id you kept only ever names
  the one actor.
- `kNoId` stands for the absence of an id. It is what `player()` returns until you record a player.
- Each `Actors` value counts from 1 on its own. Two sets both have an actor 1.

## `Side`

```cpp
enum Side : int { Any, Above, Below, Left, Right };
```

The side of an actor that an action comes from. Write it in a script's `from` to start the script from
that side only:

```cpp
const ScriptKit::Script ring{.name = "ring", .on = ActorKit::Interact, .from = ActorKit::Below,
                             .body = {set(BellRung, true)}};
```

| `from` | The acting actor stands | And faces |
|---|---|---|
| `ActorKit::Any` (the default) | on any side | the actor |
| `ActorKit::Above` | above it | down |
| `ActorKit::Below` | below it | up |
| `ActorKit::Left` | to its left | right |
| `ActorKit::Right` | to its right | left |

- `Side` is a plain enum, so its enumerators convert to the `int` that `ScriptKit::Script::from` is.
- A side applies to `ActorKit::Interact` and to triggers of your own bound to `actedOn`. On a script
  whose trigger is touch, near, enter or becomes, leave `from` at `Any`: with a side set, that script
  stays off.

## `ScriptRef` and `Scripts`

```cpp
class ScriptRef {
public:
    ScriptRef(const ScriptKit::Script* script) noexcept;   // a constant, by address
    ScriptRef(ScriptKit::Script script);                   // a script made by a function, by value

    const ScriptKit::Script* get() const noexcept;
    const std::shared_ptr<const ScriptKit::Script>& shared() const noexcept;
};

using Scripts = std::vector<ScriptRef>;
```

`Scripts` is the type of `Actor::scripts`. You write a braced list, and each element converts to a
`ScriptRef`:

```cpp
.scripts = {&wake, toggle(GateOpen)},
```

| Written as | What it is | Lifetime |
|---|---|---|
| `&wake` | A script you declared as a constant, by address. | The constant must outlive every `Actors` value with an actor that lists it. |
| `toggle(GateOpen)` | A script returned by a function, by value. | The actor value keeps it, and every actor spawned from the value names that one script. |

- **Each actor runs its own copy of each script.** Two gates spawned from one value are at their own
  place in `toggle`'s body and in their own waits.
- **Names.** The scripts of one actor have different names. `spawn` passes on ScriptKit's
  `std::logic_error` for two of one name.
- **Order.** Scripts of the same `priority` are stepped in the order listed.
- **A script given by value has an address of its own.** `call`, `stop`, `restart` and `interrupts`
  name a script by address, so the scripts they name are constants listed by address
  ([scripts.md](scripts.md#scripts-on-an-actor)).
- `get()` returns the script. For a script given by value, `actor.scripts[n].get()` is the address to
  hand `Actors::processor(id).running(…)`.
- `shared()` is what keeps a script given by value alive. ActorKit uses it; your code has `get()`.

## `ActionRow` and `Actions`

```cpp
class ActionRow {
public:
    template <retropp::ActionLike A, class Input>
    ActionRow(A action, bool Input::* drive);

    retropp::ActionId action() const noexcept;
    bool performed(const void* input, const std::type_info& type) const;
};

using Actions = std::vector<ActionRow>;
```

`Actions` is the type of `Actor::actions`. Each row names one of your input actions and the `bool`
member of your input value that drives it:

```cpp
.actions = {{Action::Interact, &Input::interact}},
```

On a tick where `input.interact` is true, the actor performs `Action::Interact` on the actor it faces.
The acted-on actor's scripts on [`ActorKit::Interact`](triggers.md#actorkits-own-triggers) start when
the action is the one named in `Config::interactAction`, and so do its scripts on any trigger of yours
bound to `actedOn` of that action.

- **The member is what your tick read.** Fill it from `InputState::justPressed` for an action performed
  once per press. A member that stays true performs the action on every tick.
- **The action is a name.** `Action::Interact` is your own enumerator, as you give it to the engine's
  `ActionMap` (the engine's `docs/guide/input.md`). ActorKit matches it against your config and your
  trigger map.
- **Which actor is faced** is in [triggers.md](triggers.md#the-detections).
- `performed` is the call `tick` makes. It throws `std::logic_error` when the input handed to `tick` is
  of another type than the row's member belongs to.
- To have an actor perform an action from your own code, call [`Actors::act`](#act).

## `Actors`

```cpp
class Actors {
public:
    explicit Actors(const retropp::EngineConfig& config);

    Id spawn(const Actor& actor, WorldKit::Place place, retropp::Vec2i at);
    bool remove(Id id);

    void player(Id id);
    Id player() const noexcept;

    bool has(Id id) const noexcept;
    std::size_t size() const noexcept;
    std::span<const Id> ids() const noexcept;

    WorldKit::Place place(Id id) const;
    retropp::Vec2i position(Id id) const;
    retropp::IntRect box(Id id) const;
    retropp::Dir facing(Id id) const;
    bool touching(Id a, Id b) const;

    void moveTo(Id id, WorldKit::Place place, retropp::Vec2i at);

    std::string key(Id id) const;
    void fresh(Id id);

    template <ScriptKit::ValueType T> T get(Id id, ScriptKit::Value<T> value) const;
    template <ScriptKit::ValueType T> void set(Id id, ScriptKit::Value<T> value, T worth);

    template <retropp::ActionLike A> void act(Id id, A action);

    const ScriptKit::ScriptProcessor& processor(Id id) const;
};
```

A set of live actors: every actor from its spawn to its removal, with its place, position, own values,
mover and scripts' progress. Keep it in your game state and hand it to [`tick`](#tick) every tick.

Every member that takes an id throws `std::out_of_range`, naming the id, unless a live actor of the set
has that id. `has` and `remove` are the two exceptions: they return `false`.

### Constructor

```cpp
explicit Actors(const retropp::EngineConfig& config);
```

An empty set. `config.timing` converts the times in the actors' scripts to ticks, as
`ScriptKit::ScriptProcessor` does. The config is copied.

```cpp
struct State {
    ActorKit::Actors actors;
    WorldKit::Camera camera{0, 0};
};

State state{.actors = ActorKit::Actors{engineConfig}};
```

Each `Actors` value takes a number of its own when it is constructed. The number is part of every
sprite key the set reports ([drawing.md](drawing.md#keys)).

### `spawn`

```cpp
Id spawn(const Actor& actor, WorldKit::Place place, retropp::Vec2i at);
```

Adds a copy of `actor` in `place`, with its top-left at `at` in that area's pixels, and returns its id.
The new actor takes part from the next call to `tick`, and `Sprites` reports it at once.

```cpp
const ActorKit::Id chestId = state.actors.spawn(chest, {.layout = 0, .area = 2}, {64, 40});
```

- `spawn` puts the actor exactly at `at`, walls included. Choose a position in open ground.
- `spawn` accepts any `place`. `tick` checks it against the world.
- The actor's own values start at the worths listed in its `ownValues`.

| Throws | When |
|---|---|
| `std::invalid_argument` | The actor has a mover and a `box` with a size. |
| `std::invalid_argument` | A value is listed twice in `ownValues`. |
| `std::invalid_argument` | A script is a null pointer. |
| `std::logic_error` | ScriptKit refuses the actor's scripts: two of one name, a `call`, `stop`, `restart` or `interrupts` naming a script outside the actor's list, a script that calls itself, or scripts that interrupt each other in a ring. |

### `remove`

```cpp
bool remove(Id id);
```

Removes the actor. Returns `true` when the actor was live, `false` otherwise.

- Called from one of your handlers, during a tick, the removal takes effect once that tick's requests
  are all written. Until then `has(id)` is still `true`, and a second `remove(id)` returns `false`.
- Removing the player sets `player()` to `kNoId`.
- A script removes its own actor with `ActorKit::remove()` ([scripts.md](scripts.md#spawn-and-remove)).

### `player`

```cpp
void player(Id id);
Id player() const noexcept;
```

`player(id)` records which actor is the player. `player()` returns it, or `kNoId`. Pass `kNoId` to
clear it.

```cpp
state.actors.player(hero);
```

Recording it has two effects:

| Effect | Detail |
|---|---|
| The player walks out through its area's openings. | Every other actor stops at its area's edge ([movers.md](movers.md#what-a-mover-collides-with)). |
| `PlayerHere`, `PlayerX` and `PlayerY` are filled in from it. | For every actor's scripts ([scripts.md](scripts.md#the-six-values)). |

The camera stays your game's to move. To follow the player with it, call `WorldKit::Camera::follow` with
`box(player())` ([how-to.md](how-to.md#follow-the-player-with-the-camera)).

### `has`, `size` and `ids`

```cpp
bool has(Id id) const noexcept;
std::size_t size() const noexcept;
std::span<const Id> ids() const noexcept;
```

Whether an actor with that id is live, how many actors are live, and the live actors' ids in spawn
order. The span is valid until the next `spawn` or `remove`.

```cpp
for (const ActorKit::Id id : state.actors.ids()) {
    if (state.actors.place(id) == here) {
        // an actor in the camera's area
    }
}
```

`here` is the camera's `WorldKit::Place`.

### `place`, `position`, `box` and `facing`

```cpp
WorldKit::Place place(Id id) const;
retropp::Vec2i position(Id id) const;
retropp::IntRect box(Id id) const;
retropp::Dir facing(Id id) const;
```

| Call | Returns |
|---|---|
| `place(id)` | The layout and area the actor is in. |
| `position(id)` | The top-left of its art, in its area's pixels. |
| `box(id)` | Its box in its area's pixels: the mover's collision bounds for an actor with a mover, otherwise the actor's own `box` moved to its position. |
| `facing(id)` | The way its mover faces. `retropp::Dir::Down` for an actor with an empty `mover`. |

`box` is the rectangle `WorldKit::Camera::follow` takes.

### `touching`

```cpp
bool touching(Id a, Id b) const;
```

Whether the two actors are in the same area with their boxes overlapping. It compares any two actors
of the set, whatever their `setsOff` lists.

### `moveTo`

```cpp
void moveTo(Id id, WorldKit::Place place, retropp::Vec2i at);
```

Puts the actor at `at` in `place`, at once.

| Use it to | See |
|---|---|
| Carry the player where the camera says, through a scroll, a fade or a seam | [how-to.md](how-to.md#follow-the-player-with-the-camera) |
| Warp an actor | [how-to.md](how-to.md#warp-an-actor) |
| Send any actor into another area | [how-to.md](how-to.md#send-an-actor-to-another-area) |

- `moveTo` puts the actor exactly there, walls included.
- The actor keeps its facing, its values and its scripts.
- The engine eases a sprite from where it was last drawn. To show the actor at its new position at
  once, call [`fresh`](drawing.md#fresh-keys) on the same tick.
- `ActorKit::Enter` follows the camera: it happens when the camera's area changes. Moving an actor into
  the camera's area leaves its `Enter` scripts off ([triggers.md](triggers.md#the-detections)).

### `key` and `fresh`

```cpp
std::string key(Id id) const;
void fresh(Id id);
```

`key` is the key of the actor's sprite, `actor:<set>:<id>`. `fresh` reports that the actor's sprite is
shown where it is at once on this tick. Both are covered in [drawing.md](drawing.md#keys).

### `get` and `set`

```cpp
template <ScriptKit::ValueType T> T get(Id id, ScriptKit::Value<T> value) const;
template <ScriptKit::ValueType T> void set(Id id, ScriptKit::Value<T> value, T worth);
```

Read and write one of the actor's own values from your code.

```cpp
if (state.actors.get(dogId, Awake)) {
    state.actors.set(dogId, Awake, false);
}
```

- `set` writes at once. A script's `set` of the same value is written at the start of the next tick.
- `worth` has exactly the value's type.
- Both throw `std::out_of_range`, naming the actor and the value's id, for a value outside the actor's
  `ownValues`. Your game's values are in your state: read them there.

### `act`

```cpp
template <retropp::ActionLike A> void act(Id id, A action);
```

Has the actor perform `action` on the actor it faces, on the next tick, exactly as one of its
[action rows](#actionrow-and-actions) does. Use it for an actor your code controls: an enemy that
presses a switch, or a scripted scene.

```cpp
state.actors.act(guardId, Action::Interact);
```

### `processor`

```cpp
const ScriptKit::ScriptProcessor& processor(Id id) const;
```

The actor's script processor, for a readout or a test: `running(script)`, `waiting(script)`,
`position(script)` (ScriptKit's `docs/script-processor.md`, "Where a script is").

```cpp
const bool ringing = state.actors.processor(bellId).running(ring);
```

### Copying

An `Actors` value is plain data. A copy holds every actor with its position, own values and scripts'
progress, and its movers walk the copy's own positions. The copy runs on by itself from where the
original was.

```cpp
const State saved = state;   // save
state = saved;               // load
```

A copy keeps the original's set number, so its actors keep their sprite keys.

## `Config`

```cpp
struct Config {
    const WorldKit::World* world = nullptr;
    std::optional<ActionName> interactAction;
    TriggerMap triggers;
    AnyBindings bindings;
    AnyHandlers handlers;
};
```

What your game hands ActorKit. Build it once, at startup, and pass it to every `tick` and `Sprites`
call.

| Field | Meaning | Default |
|---|---|---|
| `world` | Your `WorldKit::World`. Required. | `nullptr` |
| `interactAction` | Which of your input actions `ActorKit::Interact` is. Set it and `Interact` starts scripts. | unset |
| `triggers` | Your own triggers, each bound to a detection ([triggers.md](triggers.md#triggermap)). ActorKit's own triggers are in every map. | ActorKit's own only |
| `bindings` | How your game's values are read and written: the address of your `ScriptKit::Bindings<State>`. | empty |
| `handlers` | What your game does for each of its commands: the address of your `ActorKit::Handlers<State>` ([scripts.md](scripts.md#handlers)). | empty |

The smallest config, for a game whose scripts use only ActorKit's own triggers and each actor's own
values:

```cpp
const ActorKit::Config config{.world = &world, .interactAction = Action::Interact};
```

A full one:

```cpp
const ActorKit::Config config{
    .world          = &world,
    .interactAction = Action::Interact,
    .triggers       = triggers,
    .bindings       = &bindings,
    .handlers       = &handlers,
};
```

Here `triggers` is an `ActorKit::TriggerMap`, `bindings` a `ScriptKit::Bindings<State>` and `handlers`
an `ActorKit::Handlers<State>`, where `State` is your game state's type.

Watch for:

- **Lifetimes.** `world`, `bindings` and `handlers` are pointers. All three must outlive the config.
  `triggers` is copied in.
- **The state type.** `bindings` and `handlers` remember the `State` they were written for. `tick` and
  `Sprites` throw `std::logic_error` when they are handed a state of another type.
- **Initializer order:** `world`, `interactAction`, `triggers`, `bindings`, `handlers`.
- **One config or several.** A config is an argument. Any number of `Actors` values can be ticked with
  one config, and one `Actors` value can be handed a different config on each call.

`AnyBindings` and `AnyHandlers` are the types of the last two fields. Each converts from the address of
your bindings or handlers, whatever your state type is, so `Config` itself is a plain struct. You write
`&bindings` and `&handlers`.

## `tick`

```cpp
template <class State, class Input>
void tick(Actors& actors, const Config& config, const WorldKit::Camera& camera, const Input& input, State& state);
```

One tick of every live actor in `actors`. Call it once per tick for each set.

```cpp
loop.simTick([&](const retropp::InputState& in) {
    ActorKit::tick(state.actors, config, state.camera, readInput(in), state);
});
```

`readInput` is your function that reads the engine's input into your `Input` value. `Input` is any
type of yours: movers and action rows read the members you named in `movedBy` and in `actions`.

The tick runs six steps in order.

**1. Requests.** For each actor in spawn order, everything its scripts asked for on the tick before is
done, in the order asked:

| The script asked for | What happens |
|---|---|
| a `set` of one of the actor's own values | The actor's instance is written. |
| a `set` of one of ActorKit's six values | The request is dropped. |
| a `set` of any other value | It goes to `config.bindings`. A value bound read-only stays as it is. |
| `ActorKit::spawn(actor, offset)` | The actor is spawned in the asker's area, at the asker's position plus `offset`. |
| `ActorKit::remove()` | The asker is removed once every request of this step is written. |
| any other command | It goes to `config.handlers`, with the asker's id. |

**2. Solidity.** Each actor's `solid` is evaluated against its values.

**3. Walking.** While `camera.transitioning()` is false, each actor with a mover walks by its drive, in
spawn order. An actor that walks later meets the earlier ones where they now stand.
[movers.md](movers.md#what-a-mover-collides-with) lists what stops it.

**4. Actions.** For each actor in spawn order: each action row whose member is true, then each action
queued with `act`. The action is performed on the actor faced
([triggers.md](triggers.md#the-detections)).

**5. Triggers.** Touch, near, enter and becomes are detected for every actor, and the scripts they set
off start. The camera's area is the one `camera` is in when `tick` is called.

**6. Scripts.** Each actor with a running script is handed its values, and its script processor
advances once. A script started in step 4 or 5 runs its first entries here, on the same tick.

- An actor spawned during a tick, or by your code since the last one, takes part from the next tick.
- `camera` is read. Moving it, and moving the player with it, is your code after the call.
- Steps 3 to 5 compare each actor with the other actors of the set.

| Throws | When |
|---|---|
| `std::invalid_argument` | `config.world` is null. |
| `std::out_of_range` | An actor is in a layout or area outside the world. The message names the actor and the place. |
| `std::out_of_range` | An actor reads a value that is outside its own, ActorKit's six and your bindings. The message names the actor and the value's id. |
| `std::out_of_range` | A script sets one of your game's values and `config.bindings` is empty or lacks it. |
| `std::out_of_range` | A script asks for a command and `config.handlers` is empty or lacks it. The message names the command. |
| `std::logic_error` | A mover or an action row is driven by a member of another type than `Input`. |
| `std::logic_error` | `config.bindings` or `config.handlers` is written for another type than `State`. |

An exception thrown by one of your bindings, handlers or movers passes through `tick`.
