# ActorKit

Actors for a top-down game on the [Polyrhythm](https://github.com/RetroPlusPlus/Polyrhythm) engine: the
people, creatures and objects in a [WorldKit](../WorldKit/README.md) world.

An actor is anything you spawn into an area: a hero, a hen, a gate, a sign. You describe one as a plain
value and spawn as many from it as you like, at any time. One call each tick walks every actor, works
out what happened between them, and steps their scripts. Asked, an actor reports where it is, and a set
of actors reports its sprites for you to draw.

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

// Every frame: for each area on screen, the sprites of one slot, on that area's layer.
for (const WorldKit::Shown& shown : state.camera.shown(world)) {
    const WorldKit::Place place{.layout = shown.layout, .area = shown.area};
    frame.layers.push_back(
        AreaSlot(world, shown, "actors", ActorKit::Sprites(state.actors, config, place, "actors", state)));
}
```

[docs/concepts.md](docs/concepts.md#overview) explains each line and lists the names the example takes
from your game; `AreaSlot` is your component, in
[docs/drawing.md](docs/drawing.md#a-layer-for-each-area).

## Contents

| Type or function | Purpose | Reference |
|---|---|---|
| `Actor` | What an actor is: slot, box, solidity, own values, looks, scripts, mover, actions, the triggers it sets off. | [docs/actors.md](docs/actors.md#actor) |
| `Actors`, `Id` | A set of live actors. Spawn and remove them, and ask one where it is. | [docs/actors.md](docs/actors.md#actors) |
| `Config` | The world, your Interact action, your triggers, your values and your commands. | [docs/actors.md](docs/actors.md#config) |
| `tick` | One tick of every actor in a set. | [docs/actors.md](docs/actors.md#tick) |
| `Interact`, `Touch`, `Near`, `Enter`, `Becomes` | ActorKit's own triggers, ready to use in a script's `on`. | [docs/triggers.md](docs/triggers.md#actorkits-own-triggers) |
| `TriggerMap`, `actedOn`, `touched`, `near`, `entered`, `becomes` | Triggers of your own, each bound to a detection. | [docs/triggers.md](docs/triggers.md#triggers-of-your-own) |
| `OwnValue`, `Touched`, `X`, `Y`, `PlayerHere`, `PlayerX`, `PlayerY` | An actor's own values, and the six ActorKit fills in for each actor. | [docs/scripts.md](docs/scripts.md#values) |
| `spawn`, `remove`, `Handlers` | What a script asks of ActorKit, and your handlers for your own commands. | [docs/scripts.md](docs/scripts.md#commands) |
| `movedBy`, `Mover`, `Solids` | What moves an actor: a mover of your type, driven by your input, a value or a function. | [docs/movers.md](docs/movers.md) |
| `Look`, `lookOf`, `Looks`, `Sprites` | What an actor shows, and the sprites of a slot. | [docs/drawing.md](docs/drawing.md) |

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

## Documentation

| Page | Read it for |
|---|---|
| **[Getting started](docs/getting-started.md)** | **Start here.** A complete example program, a farmyard, explained step by step. |
| [Concepts](docs/concepts.md) | How the module works. |
| [Actors](docs/actors.md) | The actor value, a set of actors, the config and the tick. Reference. |
| [Triggers](docs/triggers.md) | What starts an actor's scripts. Reference. |
| [Scripts](docs/scripts.md) | Scripts on an actor, values, commands, sharing a script. Reference. |
| [Movers](docs/movers.md) | What moves an actor and what drives it. Reference. |
| [Drawing](docs/drawing.md) | Looks, slots, sprites and their keys. Reference. |
| [How-to](docs/how-to.md) | Recipes for specific tasks. |
| [Troubleshooting](docs/troubleshooting.md) | Symptoms, causes and fixes. |
| [Testing](docs/testing.md) | Testing code that uses the module. |
| [Guide index](docs/README.md) | Every page, a task index, a name index, and what the module provides and what it leaves to your game. |

## Layout

```
modules/ActorKit/
  CMakeLists.txt                    the ActorKit::ActorKit target and the example option
  include/ActorKit/
    actor.h                         Actor, Id, Side, ScriptRef, Scripts, ActionRow, Actions
    actors.h                        Actors
    mover.h                         movedBy, Mover, Solids
    look.h                          Look, lookOf, LookRule, Looks
    values.h                        OwnValue, OwnValues, Touched, X, Y, PlayerHere, PlayerX, PlayerY
    triggers.h                      On, Interact, Touch, Near, Enter, Becomes, TriggerMap, actedOn, touched, near, entered, becomes
    commands.h                      spawn, remove, Handler, Handlers
    config.h                        Config, AnyBindings, AnyHandlers
    tick.h                          tick
    sprites.h                       Sprites
  examples/getting_started.cpp      the example program: a farmyard
  examples/farm.h                   its places, values and commands
  examples/scripts/                 its scripts, a file each
  examples/shaders/                 the pond's ripple shader
  examples/assets/                  its sheets and palette images
  docs/                             the guide
```

| Header | Note |
|---|---|
| `sprites.h` | The only header that creates a draw type (`retropp::Sprite`). The other headers are for simulation code. |
| `tick.h` | Reads the `WorldKit::Camera` it is handed: its area, and whether a scroll or fade is running. |

## License

MIT. See [LICENSE](LICENSE).
