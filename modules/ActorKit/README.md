# ActorKit

Actors for a game on the [Polyrhythm](https://github.com/RetroPlusPlus/Polyrhythm) engine: the people,
creatures and objects in a [WorldKit](../WorldKit/README.md) world.

An actor is anything you spawn into an area: a hero, a hen, a gate, a sign. You describe one as a plain
value — where it is drawn, what it collides with, what it shows, what moves it, and the scripts that run
when something happens to it — and spawn as many from that value as you like, at any time. Each tick,
one call walks every actor, works out what happened between them, and steps their scripts. Asked, an
actor reports where it is, and a set of actors reports its sprites for you to draw.

```cpp
#include "ActorKit/actors.h"    // Actor, Actors, movedBy
#include "ActorKit/config.h"    // Config, TriggerMap
#include "ActorKit/sprites.h"   // Sprites
#include "ActorKit/tick.h"      // tick

// A trigger of your own takes three lines: your name for it, the trigger a script puts in `on`, and the
// row that binds it to something ActorKit detects.
enum class On : std::uint8_t { Touch };
constexpr ScriptKit::Trigger Touch{On::Touch};
const ActorKit::TriggerMap triggers{{On::Touch, ActorKit::touched()}};

// Values each actor holds its own instance of.
const ScriptKit::Value<bool> Pressed;
const ScriptKit::Value<bool> Pulled;

// One script, shared by writing it as a function of the values it acts on.
ScriptKit::Script pressWhile(ScriptKit::Value<bool> pressed, ScriptKit::Value<bool> held) {
    return {.name = "press", .on = Touch, .body = {set(pressed, true), waitWhile(held), set(pressed, false)}};
}

// ActorKit's own triggers are ready as they are: Interact, Touch, Near, Enter and Becomes.
const ScriptKit::Script release{.name = "release", .on = ActorKit::Interact, .body = {set(Pulled, false)}};

// What each actor is: a value.
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

// Handed over once: the world, which of your input actions is Interact, and your own triggers.
const ActorKit::Config config{.world = &world, .interactAction = Action::Interact, .triggers = triggers};

// The actors live in your state. Spawn any number from one value.
state.actors.spawn(hero, {.layout = 0, .area = 0}, {72, 96});
state.actors.spawn(plate, {.layout = 0, .area = 0}, {32, 64});
state.actors.spawn(plate, {.layout = 0, .area = 0}, {96, 64});
state.actors.spawn(lever, {.layout = 0, .area = 0}, {128, 32});

// Every tick.
ActorKit::tick(state.actors, config, state.camera, input, state);

// Every frame: the sprites of one slot, for the layer you draw them on.
const WorldKit::Place here{.layout = state.camera.layout(), .area = state.camera.area()};
frame.layers.push_back(WorldKit::Actors(state.camera, world, "actors",
                                        {ActorKit::Sprites(state.actors, config, here, "actors", state)}));
```

The example borrows these names from your program:

| Name | Type | What it is |
|---|---|---|
| `plateUp`, `plateDown`, `leverUp`, `leverDown` | `ActorKit::Look` | Art from your sheets, made with `ActorKit::lookOf(sheet, slot, palette)`. |
| `heroWalk` | `TopDownMovement::WalkerConfig` | The hero's walker: speed, collision shape, walk animations. |
| `Action` | your enum | Your input actions. `Action::Interact` is the one the player presses to interact. |
| `Input` | your struct | What the player does on one tick. `Input::move` is a `TopDownMovement::DirectionalInput`; `Input::interact` is a `bool`, true on the tick Interact is pressed. |
| `State` | your struct | Your game's state. `State::actors` is an `ActorKit::Actors`, `State::camera` a `WorldKit::Camera`. |
| `world` | `WorldKit::World` | Your world. |
| `input`, `state`, `frame` | `Input`, `State`, `retropp::FrameDrawState` | This tick's input, your state, and the frame you are building. |

## What ActorKit does and what your game does

| ActorKit | Your game |
|---|---|
| Keeps every live actor: its area, position, own values, mover and running scripts. | Declares the actor values and spawns them. |
| Walks each actor with a mover, against its area's collision map and the other solid actors. | Reads the player's input into its own `Input` value once a tick. |
| Detects what happens between actors — acted on, touched, near, entered, a value becoming true — and starts the scripts those set off. | Writes the scripts, names its Interact action, and declares any triggers of its own. |
| Steps every actor's scripts and writes what they ask for. | Binds its own values and handles its own commands. |
| Reports an actor's place, position, box and facing, and a slot's sprites. | Moves the camera, with `WorldKit::Camera`, and carries the followed actor where the camera says. |
| | Draws: hands each slot's sprites to `WorldKit::Actors`, beside any sprites of its own. |

ActorKit reads the WorldKit camera you hand it and leaves it as it is.

## Contents

| Type or function | Purpose | Header |
|---|---|---|
| `Actor` | What an actor is: slot, box, solidity, own values, looks, scripts, mover, actions, the triggers it sets off. | `actor.h` |
| `Actors`, `Id` | The live actors. `spawn`, `remove`, `follow`, and what an actor reports: `place`, `position`, `box`, `facing`, `key`. | `actors.h` |
| `movedBy`, `Mover`, `Solids` | What moves an actor: a mover of your type, driven by your input, a value, or a function. | `mover.h` |
| `Look`, `lookOf`, `LookRule`, `Looks` | What an actor shows, chosen by its values. | `look.h` |
| `OwnValue`, `Touched`, `X`, `Y`, `FollowedHere`, `FollowedX`, `FollowedY` | An actor's own values, and the six ActorKit fills in for each actor. | `values.h` |
| `Interact`, `Touch`, `Near`, `Enter`, `Becomes`, `On` | ActorKit's own triggers, ready to use in a script's `on`. | `triggers.h` |
| `TriggerMap`, `actedOn`, `touched`, `near`, `entered`, `becomes` | Triggers of your own, each bound to a detection. | `triggers.h` |
| `spawn`, `remove`, `Handlers` | What a script asks of ActorKit, and your handlers for your own commands. | `commands.h` |
| `Config` | The world, your Interact action, your triggers, your bindings and your handlers. One config serves any number of `Actors` values, or each has its own. | `config.h` |
| `tick` | One tick of every actor. | `tick.h` |
| `Sprites` | The sprites of the actors of one area at one slot. | `sprites.h` |

## Several sets of actors

An `Actors` value is one set. You can keep several — one for switches, one for enemies — tick each, and
send each one's sprites to the layer you choose, combined on one layer or each on its own. ActorKit
numbers the sets itself, so every sprite key is unique across them.

Actors detect the actors of their own set: touching, coming near and acting on happen within one
`Actors` value. Sets exchange through your game's values: a script in one set sets a value, and a script
in another reads it, with `Becomes(value)`, a `when` or a `waitWhile`.

## Requirements

- C++20
- The Polyrhythm engine, as the CMake target `retropp::engine`
- `LayerKit::LayerKit`, `WorldKit::WorldKit` and `ScriptKit::ScriptKit`

The module is header-only. The movers are yours: `movedBy` takes any mover type with the members it
lists, and `TopDownMovement`'s walkers are two.

## Install

```cmake
add_subdirectory(modules/ActorKit)
target_link_libraries(my-game PRIVATE ActorKit::ActorKit)
```

| CMake option | Default | Effect |
|---|---|---|
| `ACTORKIT_BUILD_EXAMPLES` | `ON` when this directory is the top-level project, otherwise `OFF` | Builds the example program as the target `ActorKit-example-getting-started`. The example links `TopDownMovement::TopDownMovement`. |

## The example program

`examples/getting_started.cpp` is a farmyard of two areas. Steer the hero with the arrow keys, WASD or a
d-pad; X or a pad's A button is Interact.

| Try this | You see |
|---|---|
| Walk up to the dog | It wakes when you come within 24 pixels. |
| Press Interact below the bell | The bell rings for a second, and the dog, once awake, trots to it and back. From any other side the bell stays still. |
| Walk through the puddle | It splashes while you stand in it, under your feet. |
| Press Interact at the gate | It opens; press again and it shuts. Shut, it stops you. |
| Walk through the open gate | The screen scrolls into the field. The four hens scatter, then wander, and each lays three eggs, one every four seconds. |
| Walk onto an egg | It is picked up, and the row along the top grows by one. |

## Layout

```
modules/ActorKit/
  CMakeLists.txt                    the ActorKit::ActorKit target and the example option
  include/ActorKit/
    actor.h                         Actor, Id, Side, ScriptRef, Scripts, ActionRow, Actions
    actors.h                        Actors
    mover.h                         movedBy, Mover, Solids
    look.h                          Look, lookOf, LookRule, Looks
    values.h                        OwnValue, OwnValues, Touched, X, Y, FollowedHere, FollowedX, FollowedY
    triggers.h                      On, Interact, Touch, Near, Enter, Becomes, TriggerMap, actedOn, touched, near, entered, becomes
    commands.h                      spawn, remove, Handler, Handlers
    config.h                        Config, AnyBindings, AnyHandlers
    tick.h                          tick
    sprites.h                       Sprites
  examples/getting_started.cpp      the example program: a farmyard
  examples/farm.h                   its places, triggers and values
  examples/scripts/                 its scripts, a file each
  examples/assets/                  its sheets and palette images
```

| Header | Note |
|---|---|
| `sprites.h` | The only header that creates a draw type (`retropp::Sprite`). |
| `tick.h` | Reads the `WorldKit::Camera` it is handed: its area, and whether a scroll or fade is running. |

## License

MIT. See [LICENSE](LICENSE).
