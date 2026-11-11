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
        .areas = {
            WorldKit::Area{.layers = {{.name = "ground", .tiles = &meadowTiles, .art = overworldArt, .z = kGroundZ},
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
| `Area`, `MapLayer`, `ActorSlot`, `Opening`, `Layout`, `World`, `validate` | what a world is: layouts of areas, their layers and slots, their openings, where each leads | [docs/areas.md](docs/areas.md) |
| `Camera` | which layout and area the screen is in, where it looks, the scroll or fade running | [docs/camera.md](docs/camera.md) |
| `AreaSolids` | what is solid in an area, for anything that moves | [docs/camera.md](docs/camera.md#areasolids) |
| `WorldLayers`, `Actors`, `Fade`, `TopZ` | the areas on screen as tile layers at their depths, the game's sprites at an actor slot, confined to the view, the fade over them, and the depth above it all | [docs/drawing.md](docs/drawing.md) |

The module is header-only, depends on the engine, [LayerKit](../LayerKit/README.md) and
[CameraKit](../CameraKit/README.md), includes no header from the game it sits in, and needs C++20. It is
meant to leave this repository as a standalone module.

## Using it

```cmake
add_subdirectory(modules/LayerKit)
add_subdirectory(modules/CameraKit)
add_subdirectory(modules/WorldKit)
target_link_libraries(my-game PRIVATE WorldKit::WorldKit)
```

The directory's `CMakeLists.txt` defines the INTERFACE target `WorldKit` (alias `WorldKit::WorldKit`) and
expects the engine as `retropp::engine`, the layer kit as `LayerKit::LayerKit` and the camera as
`CameraKit::CameraKit`. `WORLDKIT_BUILD_EXAMPLES` (on by default when the directory is the top-level
project) builds `WorldKit-example-getting-started`, the program
[docs/getting-started.md](docs/getting-started.md) walks through.

## Documentation

**Start with [docs/getting-started.md](docs/getting-started.md)**, then [docs/concepts.md](docs/concepts.md)
for the model. The full index is [docs/README.md](docs/README.md).

## Layout

```
modules/WorldKit/
  CMakeLists.txt                    the WorldKit::WorldKit target and the example option
  include/WorldKit/
    area.h                          Side, Transition, Span, Destination, Opening, CellValue, MapLayer,
                                    ActorSlot, Area, CameraLaw, Layout, World, Place, validate, ActorZ
    solids.h                        AreaSolids
    camera.h                        Step, Shown, Camera
    world_layers.h                  AreaLayerKey, ActorsKey, TopZ, ViewStencil, WorldLayers, Actors, Fade
  examples/getting_started.cpp      the getting-started program
  docs/                             the guide
```

`world_layers.h` is the only header that builds draw types, so a game's simulation can include the rest
without anything to draw.
