# WorldKit — a world of areas

`WorldKit` builds a game's world on the [Polyrhythm](https://github.com/RetroPlusPlus/Polyrhythm) engine
out of **areas** — dungeon rooms, overworld screens, caves — each with its own maps and art, drawn in
layers at depths the game gives, grouped into **layouts** and joined only by the **openings** in them. A
game says what each area is and where each opening leads; WorldKit says what is solid, takes the thing
the screen follows through each opening — by a scroll, a fade, or seamlessly, with the next area simply
there beyond the wall — keeps the camera within the area, and draws whatever is on screen, with the
game's sprites at each area's actor slots: over a path, under a canopy. What moves through the world is the game's own, or a
movement module's: WorldKit answers its questions and moves nothing itself.

```cpp
// The world, once: each layout's areas, their layers, actor slots and openings.
const WorldKit::World world{.layouts = {
    WorldKit::Layout{
        .name = "overworld",
        .areas = {
            WorldKit::Area{.name = "meadow",
                           .layers = {{.name = "ground", .tiles = &meadowTiles, .art = overworldArt, .z = kGroundZ},
                                      {.name = "overhang", .tiles = &meadowRock, .art = overlayArt, .z = kOverheadZ}},
                           .actorSlots = {{.name = "actors", .z = kActorsZ}},
                           .collision = &meadowCollision,
                           .openings = {{.name = "east door", .side = WorldKit::Side::East, .span = {6, 9},
                                         .transition = WorldKit::Transition::Scroll,
                                         .leadsTo = {.area = Area::Lake, .opening = "west door"}},
                                        {.name = "cave mouth", .at = {.x = 12, .y = 4, .width = 2, .height = 1},
                                         .transition = WorldKit::Transition::Fade,
                                         .leadsTo = {.area = Area::Cave, .opening = "cave exit"}}}},
            ...
        },
        .cellSize = 8, .open = Cell::Open, .view = kView},
    WorldKit::Layout{ /* the dungeon */ },
}};
WorldKit::validate(world);

// Each tick: the player moves against what is solid, and the camera follows it through the openings.
if (!camera.transitioning()) { player.walk(hero, move, camera.solids(world)); }
const WorldKit::Step step = camera.follow(world, player.collision(hero).bounds());
player.place(player.position() + step.move);

// Each frame: the areas' layers, the player among them, the fade over everything.
frame.layers = WorldKit::WorldLayers(camera, world);
frame.layers.push_back(WorldKit::Actors(camera, world, "actors", {Player(state)}));
frame.regions = WorldKit::Fade(camera, world);
```

| Piece | What it is | Page |
|---|---|---|
| `Area`, `MapLayer`, `ReservedLayer`, `ActorSlot`, `Opening`, `Layout`, `World`, `validate` | what a world is: named layouts of named areas, their layers, the depths they reserve for layers of the game's own, their slots and openings, where each leads | [docs/areas.md](docs/areas.md) |
| `ActorZ`, `LayerZ`, `placeOf` | a slot's or a layer's depth, declared or drawn for an area on screen, and a place found by its names | [docs/areas.md](docs/areas.md#actorz), [docs/camera.md](docs/camera.md#actorz-layerz-by-shown) |
| `Camera` | which layout and area the screen is in, where it looks, the scroll or fade running | [docs/camera.md](docs/camera.md) |
| `AreaSolids` | what is solid in an area, for anything that moves | [docs/camera.md](docs/camera.md#areasolids) |
| `WorldLayers`, `Actors`, `Fade`, `TopZ` | the areas on screen as tile layers, each alone at its declared depths and spread among the others when several are on screen, the game's sprites at an actor slot, confined to the view, the fade over them, and the depth above it all | [docs/drawing.md](docs/drawing.md) |

The module is header-only and needs C++20. It includes the headers of the engine,
[LayerKit](../LayerKit/README.md) and [CameraKit](../CameraKit/README.md) alone, and its actors add
[ScriptKit](../ScriptKit/README.md)'s. It is meant to leave this repository as a standalone module.

## Actors

ActorKit is the part of the module for the people, creatures and objects in a world: a hero, a hen, a
gate, a sign. You describe an actor as a plain value and spawn as many from it as you like, at any time.
One call each tick walks every actor, works out what happened between them, and steps their scripts.
Asked, an actor reports where it is, and a set of actors reports its sprites for you to draw.

ActorKit has its own headers, `"ActorKit/…"`, its own namespace, `ActorKit`, and its own target,
`ActorKit::ActorKit`. A game with a system of its own for what lives in the world links
`WorldKit::WorldKit` alone. [docs/actors/concepts.md](docs/actors/concepts.md#overview) opens with an
example of the whole use and explains each line.

| Type or function | Purpose | Reference |
|---|---|---|
| `Actor` | What an actor is: slot, box, solidity, own values, looks, scripts, mover, actions, the triggers it sets off. | [docs/actors/actors.md](docs/actors/actors.md#actor) |
| `Actors`, `Id` | A set of live actors. Spawn and remove them, and ask one where it is. | [docs/actors/actors.md](docs/actors/actors.md#actors) |
| `Config` | The world, your Interact action, your triggers, your values and your commands. | [docs/actors/actors.md](docs/actors/actors.md#config) |
| `tick` | One tick of every actor in a set. | [docs/actors/actors.md](docs/actors/actors.md#tick) |
| `Interact`, `Touch`, `Near`, `Enter`, `Becomes` | ActorKit's own triggers, ready to use in a script's `on`. | [docs/actors/triggers.md](docs/actors/triggers.md#actorkits-own-triggers) |
| `TriggerMap`, `actedOn`, `touched`, `near`, `entered`, `becomes` | Triggers of your own, each bound to a detection. | [docs/actors/triggers.md](docs/actors/triggers.md#triggers-of-your-own) |
| `OwnValue`, `Touched`, `X`, `Y`, `PlayerHere`, `PlayerX`, `PlayerY` | An actor's own values, and the six ActorKit fills in for each actor. | [docs/actors/scripts.md](docs/actors/scripts.md#values) |
| `spawn`, `remove`, `Handlers` | What a script asks of ActorKit, and your handlers for your own commands. | [docs/actors/scripts.md](docs/actors/scripts.md#commands) |
| `movedBy`, `Mover`, `Solids` | What moves an actor: a mover of your type, driven by your input, a value or a function. | [docs/actors/movers.md](docs/actors/movers.md) |
| `Look`, `lookOf`, `Looks`, `Sprites` | What an actor shows, and the sprites of a slot. | [docs/actors/drawing.md](docs/actors/drawing.md) |

The movers are yours: `movedBy` takes any mover type with the members it lists, and `TopDownMovement`'s
walkers are two.

| Header | Note |
|---|---|
| `ActorKit/sprites.h` | The only ActorKit header that creates a draw type (`retropp::Sprite`). The other ActorKit headers are for simulation code. |
| `ActorKit/tick.h` | Reads the `WorldKit::Camera` it is handed: its area, and whether a scroll or fade is running. |

## Using it

```cmake
add_subdirectory(modules/ScriptKit)   # for ActorKit
add_subdirectory(modules/LayerKit)
add_subdirectory(modules/CameraKit)
add_subdirectory(modules/WorldKit)
target_link_libraries(my-game PRIVATE WorldKit::WorldKit)   # the world alone
target_link_libraries(my-game PRIVATE ActorKit::ActorKit)   # the world and its actors
```

The directory's `CMakeLists.txt` defines two INTERFACE targets on one include directory:

| Target | Headers | Expects |
|---|---|---|
| `WorldKit::WorldKit` | `"WorldKit/…"` | the engine as `retropp::engine`, the layer kit as `LayerKit::LayerKit` and the camera as `CameraKit::CameraKit` |
| `ActorKit::ActorKit` | `"ActorKit/…"` | `WorldKit::WorldKit` and ScriptKit as `ScriptKit::ScriptKit`; defined when `ScriptKit::ScriptKit` exists as the directory is added |

| CMake option | Default | Effect |
|---|---|---|
| `WORLDKIT_BUILD_EXAMPLES` | `ON` when this directory is the top-level project, otherwise `OFF` | Builds `WorldKit-example-getting-started`, the castle [docs/getting-started.md](docs/getting-started.md) walks through, and `WorldKit-example-farm`, the farmyard [docs/actors/getting-started.md](docs/actors/getting-started.md) walks through. The farm is built when `ActorKit::ActorKit` and `TopDownMovement::TopDownMovement` exist. |

## Documentation

**Start with [docs/getting-started.md](docs/getting-started.md)**, then [docs/concepts.md](docs/concepts.md)
for the model. The full index is [docs/README.md](docs/README.md).

For the actors, start with [docs/actors/getting-started.md](docs/actors/getting-started.md), then
[docs/actors/concepts.md](docs/actors/concepts.md). Their index is
[docs/actors/README.md](docs/actors/README.md): every page, a task index, a name index, and what
ActorKit provides and what it leaves to your game.

## Layout

```
modules/WorldKit/
  CMakeLists.txt                    the WorldKit::WorldKit and ActorKit::ActorKit targets and the example option
  include/WorldKit/
    area.h                          Side, Transition, Span, Destination, Opening, CellValue, MapLayer,
                                    ReservedLayer, ActorSlot, Area, CameraLaw, Layout, World, Place,
                                    validate, ActorZ, LayerZ, placeOf
    solids.h                        AreaSolids
    camera.h                        Step, Shown, ActorZ and LayerZ by Shown, Camera
    world_layers.h                  AreaLayerKey, ActorsKey, TopZ, ViewStencil, WorldLayers, Actors, Fade
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
  examples/getting_started.cpp      the getting-started program: the castle
  examples/farm/
    farm.cpp                        the actors' example program: a farmyard
    farm.h                          its places, values and commands
    scripts/                        its scripts, a file each
    shaders/                        the pond's ripple shader
    assets/                         its sheets and palette images
  docs/                             the guide
  docs/actors/                      the actors' guide
```

`world_layers.h` is the only header that builds draw types, so a game's simulation can include the rest
without anything to draw.
