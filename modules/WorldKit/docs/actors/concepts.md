# Concepts

How the module works: the eight things you write to put actors in a world, what each one is for, and
what a tick does with them.

## Contents

- [Overview](#overview)
- [An actor is a value](#an-actor-is-a-value)
- [A set of actors](#a-set-of-actors)
- [A tick](#a-tick)
- [Values](#values)
- [Triggers](#triggers)
- [Movers](#movers)
- [The camera](#the-camera)
- [Drawing](#drawing)
- [Several sets](#several-sets)
- [Where each part lives in a game](#where-each-part-lives-in-a-game)
- [Lifetimes](#lifetimes)

## Overview

```cpp
#include "ActorKit/actors.h"    // Actor, Actors, movedBy
#include "ActorKit/config.h"    // Config, TriggerMap
#include "ActorKit/sprites.h"   // Sprites
#include "ActorKit/tick.h"      // tick

// 1. A trigger of your own: your name for it, the trigger a script puts in `on`, and the row that binds
//    it to something ActorKit detects.
enum class On : std::uint8_t { Touch };
constexpr ScriptKit::Trigger Touch{On::Touch};
const ActorKit::TriggerMap triggers{{On::Touch, ActorKit::touched()}};

// 2. Values each actor holds its own instance of.
const ScriptKit::Value<bool> Pressed;
const ScriptKit::Value<bool> Pulled;

// 3. Scripts. This one is shared by writing it as a function of the values it acts on.
ScriptKit::Script pressWhile(ScriptKit::Value<bool> pressed, ScriptKit::Value<bool> held) {
    return {.name = "press", .on = Touch, .body = {set(pressed, true), waitWhile(held), set(pressed, false)}};
}

//    This one starts on a trigger of ActorKit's own: Interact, Touch, Near, Enter and Becomes are ready.
const ScriptKit::Script release{.name = "release", .on = ActorKit::Interact, .body = {set(Pulled, false)}};

// 4. The actors: each one a value.
const ActorKit::Actor plate{.slot = "underfoot", .box = {.x = 0, .y = 0, .width = 16, .height = 16},
                            .ownValues = {{Pressed, false}},
                            .look = {{Pressed, plateDown}, {plateUp}},
                            .scripts = {pressWhile(Pressed, ActorKit::Touched)}};
const ActorKit::Actor lever{.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 16},
                            .ownValues = {{Pulled, false}},
                            .look = {{Pulled, leverDown}, {leverUp}},
                            .scripts = {pressWhile(Pulled, ActorKit::Touched), &release}};
const ActorKit::Actor hero{.slot = "actors", .solid = true,
                           .mover = ActorKit::movedBy<TopDownMovement::InputWalker>(heroWalk, &Input::move),
                           .actions = {{Action::Interact, &Input::interact}},
                           .setsOff = {On::Touch}};

// 5. The config: the world, which of your input actions is Interact, and your own triggers.
const ActorKit::Config config{.world = &world, .interactAction = Action::Interact, .triggers = triggers};

// 6. The set of actors, in your state. Spawn any number from one value.
state.actors.spawn(hero, {.layout = 0, .area = 0}, {72, 96});
state.actors.spawn(plate, {.layout = 0, .area = 0}, {32, 64});
state.actors.spawn(plate, {.layout = 0, .area = 0}, {96, 64});
state.actors.spawn(lever, {.layout = 0, .area = 0}, {128, 32});

// 7. Every tick.
ActorKit::tick(state.actors, config, state.camera, input, state);

// 8. Every frame: for each area on screen, the sprites of one slot, on that area's layer.
for (const WorldKit::Shown& shown : state.camera.shown(world)) {
    const WorldKit::Place place{.layout = shown.layout, .area = shown.area};
    frame.layers.push_back(
        AreaSlot(world, shown, "actors", ActorKit::Sprites(state.actors, config, place, "actors", state)));
}
```

Names from your game:

| Name | What it is |
|---|---|
| `plateDown`, `plateUp`, `leverDown`, `leverUp` | `ActorKit::Look`s, made from your sheet with `ActorKit::lookOf` |
| `heroWalk` | a `TopDownMovement::WalkerConfig`: the hero's speed, collision box and walk animations |
| `Input` | your struct of what the player does on one tick. Here it has `move`, a `TopDownMovement::DirectionalInput`, and `interact`, a `bool`. |
| `input` | the tick's `Input` |
| `Action` | your enum of input actions, with an `Interact` enumerator |
| `world` | your `WorldKit::World`. Area 0 of its layout 0 has the actor slots `"actors"` and `"underfoot"`. |
| `state` | your game state, with `actors`, an `ActorKit::Actors`, and `camera`, a `WorldKit::Camera` |
| `frame` | the `retropp::FrameDrawState` you hand the renderer |
| `AreaSlot` | your component that puts one area's sprites at one slot on a layer of their own ([drawing.md](drawing.md#a-layer-for-each-area) has it, whole) |

[getting-started.md](getting-started.md) builds all of these in a program you can run.

The result is a hero that the pad steers and the map's walls stop, two plates, and a lever. Each plate
is pressed while the hero stands on it. The lever is pulled while the hero touches it, and released
early when the hero faces it and presses Interact.

Step 8 draws the `"actors"` slot, which holds the hero and the lever. A second `AreaSlot` line for
`"underfoot"` draws the plates ([Drawing](#drawing)). The sections below cover each step.

## An actor is a value

`ActorKit::Actor` is a struct you fill in with designated initializers. Every field is optional. A
sign, a gate, a hen and the hero are all `Actor`s, and differ in which fields are set.

| Field | What it says | Default |
|---|---|---|
| `slot` | The actor slot of its area its sprite is reported under. | `""` |
| `box` | The rectangle that collides and is touched, relative to the actor's position. | empty. An actor with a mover takes the mover's collision box. |
| `solid` | Whether walking actors stop against it: `true`, `false`, or an expression over values. | `false` |
| `ownValues` | The values each actor spawned from this one holds its own instance of, with their starting worths. | empty |
| `look` | What it shows: one look, or a list of looks with conditions. | The actor shows its mover's frame. |
| `scripts` | Its scripts. Each actor spawned runs its own copy of each. | empty |
| `mover` | What moves it. | The actor stays where it is put. |
| `actions` | The actions it performs on the actor it faces, each with the member of your input that drives it. | empty |
| `setsOff` | The touch and near triggers its body sets off in other actors. | empty |

An actor's position is the top-left of its art, in its area's pixels.

You hand the value to `spawn`, as many times as you want actors:

```cpp
const ActorKit::Actor crate{.slot = "actors", .box = {.x = 0, .y = 0, .width = 16, .height = 16},
                            .solid = true, .look = crateLook};

const ActorKit::Id first = state.actors.spawn(crate, {.layout = 0, .area = 0}, {40, 40});
const ActorKit::Id second = state.actors.spawn(crate, {.layout = 0, .area = 0}, {80, 40});
```

`crateLook` is an `ActorKit::Look`. The two crates are separate actors with ids of their own.

- **`spawn` copies the value.** A change you make to `crate` afterwards applies to the actors you spawn
  from then on.
- **Spawn at any time:** at startup, when the camera enters an area, or from a script
  ([scripts.md](scripts.md#spawn-and-remove)).
- **An actor is a value so that one description serves every instance.** A spawn is one call.

A function that returns an `Actor` describes a family of them:

```cpp
// Any hen, in any colors.
ActorKit::Actor hen(retropp::PaletteId colors) {
    return {.slot = "actors", .solid = true,
            .ownValues = {{Heading, retropp::Dir::None}},
            .mover = ActorKit::movedBy<TopDownMovement::Walker>(henWalk(colors), Heading)};
}
```

```cpp
state.actors.spawn(hen(brown), {.layout = 0, .area = 1}, {80, 56});
state.actors.spawn(hen(white), {.layout = 0, .area = 1}, {136, 112});
```

`Heading` is a `ScriptKit::Value<retropp::Dir>`, `henWalk(colors)` returns a
`TopDownMovement::WalkerConfig`, and `brown` and `white` are `retropp::PaletteId`s.

### Solidity and looks follow values

`solid` and each look's condition are expressions over values. ActorKit evaluates `solid` every tick
and the looks every frame, so an actor changes when a value does:

```cpp
const ScriptKit::Script open{.name = "open", .on = ActorKit::Interact, .body = {set(GateOpen, true)}};

const ActorKit::Actor gate{.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 16},
                           .solid = !GateOpen,
                           .look = {{GateOpen, gateOpenLook}, {gateShutLook}},
                           .scripts = {&open}};
```

`GateOpen` is a `ScriptKit::Value<bool>` and the two looks are `ActorKit::Look`s. While `GateOpen` is
false the gate is solid and shows `gateShutLook`. When the script sets it, the gate lets walkers through
and shows `gateOpenLook`. Your code only declares this.

## A set of actors

`ActorKit::Actors` holds every live actor of one set, from its spawn to its removal. Keep it in your
game state:

```cpp
struct State {
    ActorKit::Actors actors;
    WorldKit::Camera camera{0, 0};
};

State state{.actors = ActorKit::Actors{engineConfig}};
```

`engineConfig` is your `retropp::EngineConfig`. Its `timing` converts the times in your scripts' waits
to ticks.

| Call | What it does |
|---|---|
| `spawn(actor, place, at)` | Adds a copy of `actor` in `place`, with its top-left at `at`. Returns its `Id`. |
| `remove(id)` | Removes the actor. |
| `place(id)`, `position(id)`, `box(id)`, `facing(id)` | Report where the actor is and which way it faces. |
| `get(id, value)`, `set(id, value, worth)` | Read and write one of the actor's own values. |
| `moveTo(id, place, at)` | Puts the actor somewhere at once. |
| `player(id)` | Records which actor is the player. |
| `confinement(id, c)` | Sets what the actor's walk is held inside: its area (the default), its layout, or the map's walls alone. |
| `freeze(id)`, `wake(id)`, `frozen(id)` | Hold the actor exactly as it is, let it carry on, and report which it is. |

- **Ids** count from 1 in spawn order. Each id is handed out once, so an id you kept names the same
  actor for as long as that actor lives. `ActorKit::kNoId` is 0.
- **An `Actors` value is plain data.** Copying your state copies every actor with its position, its
  values and its scripts' progress, and the copy runs on by itself. Saving and loading is copying your
  state.

[actors.md](actors.md#actors) has every member.

## A tick

```cpp
ActorKit::tick(state.actors, config, state.camera, input, state);
```

| Argument | What it is |
|---|---|
| `state.actors` | The set to tick. |
| `config` | Your `ActorKit::Config`. |
| `state.camera` | Your `WorldKit::Camera`. `tick` reads it: the area it is in, and whether a scroll or a fade is running. |
| `input` | Your `Input` value for this tick. Movers and action rows read the members you named. |
| `state` | Your game state. Your bindings read and write your values in it, and your handlers receive it. |

One call does six things, in this order:

| Step | What happens |
|---|---|
| 1. Requests | Everything the scripts asked for on the tick before is written: values are set, actors are spawned and removed, your commands are handled. |
| 2. Solidity | Each actor's `solid` is evaluated. |
| 3. Walking | Each actor with a mover walks by its drive. This step waits while the camera runs a scroll or a fade. |
| 4. Actions | Each action an actor performs this tick starts the matching scripts of the actor it faces. |
| 5. Triggers | Touch, near, enter and becomes are detected, and the scripts they set off start. |
| 6. Scripts | Every running script advances and records what it asks for. |

A script asks, and the next tick writes. With the gate above and a hero walking right into it:

| Tick | What happens |
|---|---|
| N | The hero faces the shut gate and presses Interact. The gate's script starts in step 4 and asks for `GateOpen` to become true in step 6. The gate is still shut. |
| N + 1 | Step 1 writes `GateOpen`. Step 2 makes the gate passable. Step 3 walks the hero one pixel into it. The gate shows its open look. |

- **Call `tick` once per tick for each set.**
- **An actor spawned by a script takes part from the tick after its spawn.** `Sprites` reports it from
  the frame it is spawned on.
- **During a scroll or a fade, actors stand still and their scripts keep running.** A wait that began
  before a 32-tick scroll is 32 ticks further on when the scroll ends.
- **Every awake actor takes part; a frozen one holds as it is.** `freeze(id)` keeps the actor's
  position, values, scripts and waits exactly where they are, while it is still drawn, still solid if
  it was, and still touches the awake actors it meets. `wake(id)` lets it carry on from there
  ([actors.md](actors.md#freeze-wake-and-frozen)).

[actors.md](actors.md#tick) has the exact rule of each step.

## Values

Scripts, `solid`, looks and movers all read values. A value is a `ScriptKit::Value<T>` constant, where
`T` is `bool`, `int` or an enum (ScriptKit's `docs/scripts.md`, "Value"). There are three kinds, by
where the worth is kept:

| Kind | How you declare it | Instances | Written by |
|---|---|---|---|
| An actor's own | List it in the actor's `ownValues`. | one per actor | that actor's scripts, and `Actors::set` |
| ActorKit's six: `Touched`, `X`, `Y`, `PlayerHere`, `PlayerX`, `PlayerY` | They are declared in `values.h`. | one per actor | ActorKit, every tick |
| Your game's | Bind it in your `ScriptKit::Bindings`. | one | every actor's scripts through the binding, and your own code |

```cpp
const ScriptKit::Value<bool> Awake;      // each dog's own
const ScriptKit::Value<bool> GateOpen;   // the game's: there is one gate state

const ScriptKit::Script wake{.name = "wake", .on = ActorKit::Near(24), .body = {set(Awake, true)}};
const ActorKit::Actor dog{.slot = "actors", .ownValues = {{Awake, false}}, .scripts = {&wake}};

const ScriptKit::Bindings<State> bindings{{GateOpen, &State::gateOpen}};
const ActorKit::Config config{.world = &world, .bindings = &bindings};
```

Here `State` has a `bool gateOpen` member. Three dogs spawned from `dog` have three `Awake`s, and a hero
that comes near one dog wakes that dog only.

- **Own or the game's is decided per actor.** A value listed in an actor's `ownValues` is that actor's
  own. Every other value its scripts name, apart from the six, is read and written through your
  bindings.
- **Every value an actor reads must be one of the three.** For any other value, `tick` throws
  `std::out_of_range` naming the actor and the value
  ([troubleshooting.md](troubleshooting.md#exceptions)).
- **Actors affect each other through values.** One actor's script sets a value of your game's, and
  another actor's `solid`, look or `Becomes` trigger reads it
  ([how-to.md](how-to.md#have-one-actor-react-to-another)).

[scripts.md](scripts.md#values) covers values in full.

## Triggers

A trigger starts scripts. A script names its trigger in `on`. ActorKit has five ready to use:

| Trigger | Starts the script when |
|---|---|
| `ActorKit::Interact` | an actor facing this one performs your Interact action |
| `ActorKit::Touch` | an actor that sets off `Touch` starts overlapping this actor's box |
| `ActorKit::Near(distance)` | an actor that sets off `Near` comes within `distance` pixels of this actor's box |
| `ActorKit::Enter` | the camera comes into this actor's area, or this actor is spawned into the camera's area |
| `ActorKit::Becomes(value)` | `value` goes from false to true for this actor |

Three of them depend on something you set:

| Trigger | What it needs |
|---|---|
| `Interact` | `.interactAction` in your config, and an `.actions` row on the actor that presses |
| `Touch`, `Near` | `ActorKit::On::Touch` or `ActorKit::On::Near` in the `.setsOff` of the actor that moves |
| `Enter`, `Becomes` | ready as they are |

`setsOff` lets the hero press a plate that a wandering hen leaves alone: the plate's script starts for
the actors that list its trigger.

When a trigger happens to an actor:

- Every script of that actor whose `on` is the trigger starts, if the script is off. A script that is
  already running carries on where it is.
- A script with a `when` starts only if the condition holds at that moment.
- A script with a `from` starts only for that side. `.from = ActorKit::Below` on an `Interact` script
  means the acting actor stands below this one, facing up.

The overview declares a touch trigger of its own to show the three lines that takes. The ready-made
`ActorKit::Touch` does the same job: put it in the script's `on`, and list `ActorKit::On::Touch` in the
hero's `setsOff`.

[triggers.md](triggers.md) has the exact rule of each detection and covers triggers of your own.

## Movers

An actor moves when its `mover` is set:

```cpp
.mover = ActorKit::movedBy<TopDownMovement::InputWalker>(heroWalk, &Input::move),
```

`movedBy<M>(config, drive)` takes a mover type, that type's config, and what drives it. The mover type
is yours: `TopDownMovement::Walker` and `TopDownMovement::InputWalker` both fit, and so does a type of
your own with the same members ([movers.md](movers.md#a-mover-of-your-own)). ActorKit makes one mover
for each actor spawned and walks it every tick.

| Drive | Written as | Use it for |
|---|---|---|
| a member of your `Input` | `&Input::move` | the player |
| a `ScriptKit::Value<retropp::Dir>` | `Heading` | an actor its own scripts steer |
| a function of the actor's values | `towardHero` | a heading your code works out |

A walking actor stops at three things:

| What stops it | Which actors |
|---|---|
| The solid cells of its area's collision map | all |
| The box of every other solid actor in its area and its set, frozen ones included | all |
| What its confinement holds it inside: by default the edge of its area | all but the player, which walks out through the area's openings |

An actor's confinement is `Area`, `Layout` or `None`. Set to `Layout`, it walks out through the openings
into the other areas of its layout; set to `None`, through every opening that leads somewhere. It then
stands beyond the edge, in its area's pixels, until your game moves it with `moveTo`
([actors.md](actors.md#confinement)).

An actor with a mover takes its box and its facing from the mover, and shows the mover's frame while
every one of its looks' conditions is false.

[movers.md](movers.md) covers movers.

## The camera

ActorKit reads your `WorldKit::Camera` and your game moves it. The camera code is these lines of your
tick, after `ActorKit::tick`:

```cpp
const WorldKit::Step step = state.camera.follow(world, state.actors.box(hero));
const retropp::Vec2i at = state.actors.position(hero);
state.actors.moveTo(hero, step.entered.value_or(state.actors.place(hero)),
                    {.x = at.x + step.move.x, .y = at.y + step.move.y});
if (step.entered) {
    state.actors.fresh(hero);
}
```

`hero` is the `ActorKit::Id` that `spawn` returned for the hero.

- `camera.follow` is WorldKit's. It follows the box and returns a `WorldKit::Step`: `move` is how far
  to carry the box's owner this tick, and `entered` is the place the camera came into, on the tick
  it changes area (WorldKit's `docs/camera.md`).
- `moveTo` applies both to the hero, so the hero is carried through a door while the screen scrolls
  and lands in the next area's pixels.
- `fresh` on the tick the hero lands shows it in place: its position has jumped to the next area's
  pixels and its sprite to that area's layer ([drawing.md](drawing.md#fresh-keys)).

Tell ActorKit that the hero is the player, once, when you spawn it:

```cpp
state.actors.player(hero);
```

`player` records the id. The player walks out through its area's openings, where every other actor's
walk is held inside its confinement, and `PlayerHere`, `PlayerX` and `PlayerY` are filled in from it.

The camera stays in your hands so that a game already running WorldKit adds ActorKit with its camera
code as it is. A game that hands its camera to a `WorldKit::Scene` leaves these lines to the scene, which
follows the actor it is told to and carries it ([../scene.md](../scene.md)).

[how-to.md](how-to.md#follow-the-player-with-the-camera) has the whole recipe.

## Drawing

ActorKit reports sprites and your game draws them:

```cpp
frame.layers = WorldKit::WorldLayers(state.camera, world);
for (const WorldKit::Shown& shown : state.camera.shown(world)) {
    const WorldKit::Place place{.layout = shown.layout, .area = shown.area};
    frame.layers.push_back(
        AreaSlot(world, shown, "underfoot", ActorKit::Sprites(state.actors, config, place, "underfoot", state)));
    frame.layers.push_back(
        AreaSlot(world, shown, "actors", ActorKit::Sprites(state.actors, config, place, "actors", state)));
}
```

`Sprites(actors, config, place, slot, state)` returns one `retropp::Sprite` for each live actor in
`place` whose `slot` is `slot` and that has something to show. `WorldKit::WorldLayers` and
`WorldKit::Camera::shown` are WorldKit's: the map's layers, and the areas on screen with the scroll each
takes. `AreaSlot` is your component that puts one area's sprites at one slot on a layer of their own, at
that slot's depth, with that area's scroll ([drawing.md](drawing.md#a-layer-for-each-area)).

| The actor has | Its sprite shows |
|---|---|
| a look whose condition holds | the first such look in its list |
| a mover, and every look's condition false or an empty `look` | the mover's frame |
| an empty `mover`, and every look's condition false or an empty `look` | The actor is live and `Sprites` skips it. |

- **Position.** The sprite's top-left is the actor's position, in its area's pixels. Each area's layer
  takes that area's scroll.
- **A layer for each area.** An area scrolling in is drawn with its actors, and when the camera's area
  changes every layer keeps its own scroll, so every sprite stays where it is on screen.
- **Depth.** The slot's depth is set in your world. Within a slot, the sprite's `z` is the bottom of
  the actor's box, so an actor lower on the screen draws in front.
- **Key.** Each sprite is keyed `actor:<set>:<id>`, which the engine uses to match the sprite between
  ticks ([drawing.md](drawing.md#keys)).
- **Sprites of your own** go in the list you hand `AreaSlot`.

[drawing.md](drawing.md) covers looks, slots and keys.

## Several sets

A game can keep more than one `Actors` value. Each one is a set: it is ticked by its own `tick` call
and reports its own sprites.

```cpp
struct State {
    ActorKit::Actors actors;   // the hero and everything it touches, blocks and acts on
    ActorKit::Actors lights;   // torches and lamps: they share only values with the rest
    WorldKit::Camera camera{0, 0};
};
```

```cpp
// Every tick: one call per set.
ActorKit::tick(state.actors, config, state.camera, input, state);
ActorKit::tick(state.lights, config, state.camera, input, state);

// Every frame: each set's sprites go to the layer you choose. Here the two share each area's slot.
for (const WorldKit::Shown& shown : state.camera.shown(world)) {
    const WorldKit::Place place{.layout = shown.layout, .area = shown.area};
    LayerKit::Sprites sprites = ActorKit::Sprites(state.lights, config, place, "actors", state);
    const LayerKit::Sprites actors = ActorKit::Sprites(state.actors, config, place, "actors", state);
    sprites.insert(sprites.end(), actors.begin(), actors.end());
    frame.layers.push_back(AreaSlot(world, shown, "actors", std::move(sprites)));
}
```

| | Within one set | Between sets |
|---|---|---|
| Touch, near, Interact | Actors detect each other. | Scripts exchange through your game's values, when the sets are ticked with the same bindings. |
| Walking | Actors stop at each other's solid boxes. | Actors pass through each other. |
| Ids | Count from 1. | Each set counts from 1. Sprite keys carry the set's own number, so every key in a frame is distinct. |

Put actors that touch, block or act on each other in one set. Use separate sets for groups that share
only values, or that you draw on different layers.

To carry an event from one set to another, a script in the first sets one of your game's values and a
script in the second starts on `ActorKit::Becomes` of that value.

`tick` and `Sprites` take the config as an argument, so sets can share one config or each have their
own.

[how-to.md](how-to.md#keep-several-sets-and-choose-their-layers) has a worked example.

## Where each part lives in a game

| Part | When you create or call it | Where it goes |
|---|---|---|
| Values, triggers of your own, commands | once, as constants at namespace scope | a header your script files include |
| Scripts | once, as constants at namespace scope, or functions that return one | your script files |
| `Actor` values | once, after loading the art | with your loaded data |
| `Bindings`, `Handlers`, `TriggerMap`, `Config` | once, at startup | with your loaded data |
| `Actors` | at startup, empty | game state |
| `spawn` | at startup, on entering an area, from a script | wherever the actor comes into being |
| `tick`, then your camera lines | every tick | the tick |
| `Sprites` | every frame | drawing code |

`sprites.h` is the only header that creates a draw type. Simulation code includes the others and runs
with the engine library alone.

## Lifetimes

What the module keeps a pointer to, and what it copies:

| Object | Rule |
|---|---|
| An `Actor` value | Copied by `spawn`. A temporary works. |
| A script listed by address (`&release`) | Must outlive every `Actors` value with an actor that lists it. |
| A script listed by value (`pressWhile(…)`) | Kept by the actor value and by every actor spawned from it. |
| The actor named in a script's `ActorKit::spawn(egg)` | Copied when the script is written, and kept for the life of the program. |
| `Config::world` | A pointer. The world, and the maps it points to, must outlive the config. |
| `Config::bindings`, `Config::handlers` | Pointers. Both must outlive the config. |
| `Config::triggers` | Copied into the config. |
| The mover's config in `movedBy` | Copied, and shared by every actor spawned from the value. |
| The `retropp::EngineConfig` given to `Actors` | Copied. |
| An `Actors` value | Plain data. A copy is independent of the original. |
| The result of `Sprites` | A vector you own. |

The common mistake is a script declared inside a function and listed by address:

```cpp
void spawnBell(ActorKit::Actors& actors) {
    const ScriptKit::Script ring{.name = "ring", .on = ActorKit::Interact, .body = {set(BellRung, true)}};
    actors.spawn(ActorKit::Actor{.slot = "actors", .box = {.x = 0, .y = 8, .width = 16, .height = 8},
                                 .scripts = {&ring}},
                 {.layout = 0, .area = 0}, {72, 16});
}   // `ring` is destroyed here, and the bell still points at it
```

Two ways to avoid it:

- Declare the script at namespace scope, as a constant. It then lives as long as the program.
- List it by value. The actor keeps its own copy:

```cpp
.scripts = {ring},
```

A script that other scripts name in `call`, `stop`, `restart` or `interrupts` is named by address, so
declare that one at namespace scope ([scripts.md](scripts.md#scripts-on-an-actor)).
