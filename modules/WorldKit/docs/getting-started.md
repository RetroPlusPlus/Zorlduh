# Getting started

This page builds a complete program on the module, run by its [scene](scene.md): a castle and the grounds
outside it, two layouts joined by a seam, and five inhabitants, each written in the world where it
belongs. You steer the marker with the arrow keys, WASD or a d-pad; the camera follows it through a door,
down stairs and across the seam. A guard paces the hall and stops at its door. A gardener tends the
grounds, through the orchard's door and back, and stops at the seam into the castle. A hound roams the
castle and stops at the seam until the marker touches it; from then on it goes anywhere. A bat in the
cellar flies onto the stairs and is gone.

The full source is [`examples/getting_started.cpp`](../examples/getting_started.cpp). It builds as the
`WorldKit-example-getting-started` target whenever `WORLDKIT_BUILD_EXAMPLES` is on, so what you read here
is what compiles.

## Contents

- [1. Build it](#1-build-it)
- [2. Run it](#2-run-it)
- [3. The maps](#3-the-maps)
- [4. The world, with who lives where](#4-the-world-with-who-lives-where)
- [5. The mover](#5-the-mover)
- [6. The inhabitants and their scripts](#6-the-inhabitants-and-their-scripts)
- [7. The input, the state and the handler](#7-the-input-the-state-and-the-handler)
- [8. The scene](#8-the-scene)
- [9. Drawing it](#9-drawing-it)
- [10. Where to go next](#10-where-to-go-next)

## 1. Build it

The program needs the engine as the CMake target `retropp::engine`, LayerKit as `LayerKit::LayerKit`,
CameraKit as `CameraKit::CameraKit` and ScriptKit as `ScriptKit::ScriptKit`, so that WorldKit defines
`ActorKit::ActorKit`, which the scene is part of. Add them and turn the examples on:

```cmake
add_subdirectory(engine)
add_subdirectory(modules/ScriptKit)
add_subdirectory(modules/LayerKit)
add_subdirectory(modules/CameraKit)
set(WORLDKIT_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(modules/WorldKit)
```

```sh
cmake -S . -B build
cmake --build build --target WorldKit-example-getting-started
```

The executable lands in the build tree under `modules/WorldKit/`.

## 2. Run it

| Do this | What you see |
|---|---|
| Start | The hall, the square marker in it, and the guard walking east. The guard is held at the edge of the east door, walks back west, and goes on pacing. |
| Walk east through the door | The wide hall slides in while the marker is carried through. The hound sets off on its round: up to the north wall, where it is held, then right, down and left. |
| Walk up to the north wall | The green garden is there beyond the wall, the gardener standing in it. Walk on up into it and the picture holds still: the garden was already on screen. |
| Stay in the garden | The gardener walks east out through the garden's door, comes back in through it from the orchard, then walks south to the garden's south edge, where the seam into the castle holds it, and round again. Leaves drift down the garden, over the inhabitants and under the trees. |
| Walk east through the garden's door | The orchard slides in, with its trees. |
| Go back into the hall | The gardener stands where it was: the grounds are off screen. It carries on when the grounds come back into view. |
| Walk onto the hound | From then on its round takes it up across the seam into the garden, and back. |
| Step onto the striped stairs in the wide hall | The screen fades out, then in on the cellar. The bat flies east onto the cellar's stairs and is gone. |
| Step off the stairs and back on | The screen fades back up to the wide hall. |

Close the window to quit.

## 3. The maps

Each area is its own map: an `IndexGrid` of cells, ringed by wall, with doorways where its openings are
and stairs where its spots are. The program builds them in code; a game loads them from map PNGs with
`retropp::loadMapPng`.

```cpp
enum class Cell : std::uint16_t { Floor = 0, Wall = 1, Stairs = 2 };
```

```cpp
struct Maps {
    retropp::IndexGrid hall = Room(20, 18, Down(19, 7, 10), {});
    retropp::IndexGrid wideHall = Room(30, 18, Both(Down(0, 7, 10), Across(0, 1, 28)), {{20, 5}, {21, 5}});
    retropp::IndexGrid wideHallFloor = Walkable(wideHall);
    retropp::IndexGrid cellar = Room(12, 10, {}, {{5, 4}, {6, 4}});
    retropp::IndexGrid cellarFloor = Walkable(cellar);
    retropp::IndexGrid garden = Room(30, 12, Both(Across(11, 1, 28), Down(29, 4, 7)), {});
    retropp::IndexGrid gardenCanopy = Canopy(30, 12, {{.x = 4, .y = 1, .width = 6, .height = 3},
                                                      {.x = 18, .y = 1, .width = 7, .height = 3}});
    retropp::IndexGrid orchard = Room(20, 12, Down(0, 4, 7), {});
    retropp::IndexGrid orchardCanopy = Canopy(20, 12, {{.x = 3, .y = 1, .width = 4, .height = 3},
                                                       {.x = 10, .y = 8, .width = 4, .height = 3},
                                                       {.x = 15, .y = 2, .width = 3, .height = 4}});
};
```

| Map | What it is |
|---|---|
| `hall` | 20 × 18 cells, a door in the east wall at rows 7–10 |
| `wideHall` | 30 × 18, a door in the west wall at rows 7–10, the north wall open from cell 1 to 28, stairs at cells (20, 5) and (21, 5) |
| `cellar` | 12 × 10, stairs at (5, 4) and (6, 4) |
| `garden` | 30 × 12, the south wall open from cell 1 to 28, a door in the east wall at rows 4–7 |
| `orchard` | 20 × 12, a door in the west wall at rows 4–7 |
| `wideHallFloor`, `cellarFloor` | the collision maps of the two rooms with stairs: their walls as they are, every other cell floor |
| `gardenCanopy`, `orchardCanopy` | the trees, drawn over the two grounds areas: `Canopy` in rectangles of cells, `Nothing` everywhere else |

A layout has one open collision value, `Cell::Floor`, and every other value is solid. The stairs are
drawn as stairs from the room's map and walked on through the room's collision map, `Walkable`'s copy
with the stairs turned to floor. Each map is its own; how they join is said once, in the world.

The canopy maps are drawn through a sheet of their own, uploaded with index 0 as its transparent index,
so the ground shows through between the leaves:

```cpp
               .overhead = renderer.uploadAtlas(overhead.data(), 24, 8, retropp::TransparentIndices::of({0})).atlasId,
```

## 4. The world, with who lives where

The world is two layouts, the castle and the grounds, and each area, layout and the world itself lists
the inhabitants that start there:

```cpp
enum Layout : std::uint8_t { Castle, Grounds };
enum CastleArea : std::uint8_t { Hall, WideHall, Cellar };
enum GroundsArea : std::uint8_t { Garden, Orchard };
```

```cpp
WorldKit::World CastleWorld(const Maps& maps, const Art& art) {
    using WorldKit::Side;
    using WorldKit::Transition;
    return WorldKit::World{
        .layouts = {
            WorldKit::Layout{
                .name = "castle",
                .areas = {
                    WorldKit::Area{.name = "hall", .layers = {Ground(maps.hall, art, art.hall)}, .actorSlots = kActorSlots,
                                   .collision = &maps.hall,
                                   .openings = {{.name = "east door", .side = Side::East, .span = {7, 10},
                                                 .transition = Transition::Scroll,
                                                 .leadsTo = {.area = CastleArea::WideHall, .opening = "west door"}}},
                                   .actors = {{.actor = Guard(art), .at = {40, 64}}}},
                    WorldKit::Area{.name = "wide hall", .layers = {Ground(maps.wideHall, art, art.hall)},
                                   .actorSlots = kActorSlots,
                                   .collision = &maps.wideHallFloor,
                                   .openings = {{.name = "west door", .side = Side::West, .span = {7, 10},
                                                 .transition = Transition::Scroll,
                                                 .leadsTo = {.area = CastleArea::Hall, .opening = "east door"}},
                                                {.name = "stairs down", .at = {.x = 20, .y = 5, .width = 2, .height = 1},
                                                 .transition = Transition::Fade,
                                                 .leadsTo = {.area = CastleArea::Cellar, .opening = "stairs up"}},
                                                {.name = "north edge", .side = Side::North, .span = {1, 28},
                                                 .transition = Transition::Seamless,
                                                 .leadsTo = {.layout = Layout::Grounds, .area = GroundsArea::Garden,
                                                             .opening = "south edge"}}}},
                    WorldKit::Area{.name = "cellar", .layers = {Ground(maps.cellar, art, art.cellar)},
                                   .actorSlots = kActorSlots,
                                   .collision = &maps.cellarFloor,
                                   .openings = {{.name = "stairs up", .at = {.x = 5, .y = 4, .width = 2, .height = 1},
                                                 .transition = Transition::Fade,
                                                 .leadsTo = {.area = CastleArea::WideHall, .opening = "stairs down"}}},
                                   .actors = {{.actor = Bat(art), .at = {24, 30}, .atEdge = WorldKit::AtEdge::Despawn,
                                               .awakeWhen = WorldKit::AwakeWhen::OnScreen}}},
                },
                .actors = {{.actor = Hound(art), .in = CastleArea::WideHall, .at = {120, 80}}},
                .cellSize = kCell,
                .open = Cell::Floor,   // the one walkable value: a wall is solid
                .view = kView,
            },
            WorldKit::Layout{
                .name = "grounds",
                .areas = {
                    // The canopy is above the actor slot, so the inhabitants walk under the trees; the garden
                    // reserves a depth between them for the falling leaves the program draws itself.
                    WorldKit::Area{.name = "garden",
                                   .layers = {Ground(maps.garden, art, art.garden),
                                              WorldKit::ReservedLayer{.name = "leaves", .z = kLeavesZ},
                                              {.name = "canopy", .tiles = &maps.gardenCanopy, .art = OverheadArt(art),
                                               .z = kCanopyZ}},
                                   .actorSlots = kActorSlots,
                                   .collision = &maps.garden,
                                   .openings = {{.name = "south edge", .side = Side::South, .span = {1, 28},
                                                 .transition = Transition::Seamless,
                                                 .leadsTo = {.layout = Layout::Castle, .area = CastleArea::WideHall,
                                                             .opening = "north edge"}},
                                                {.name = "east door", .side = Side::East, .span = {4, 7},
                                                 .transition = Transition::Scroll,
                                                 .leadsTo = {.area = GroundsArea::Orchard, .opening = "west door"}}}},
                    WorldKit::Area{.name = "orchard",
                                   .layers = {Ground(maps.orchard, art, art.garden),
                                              {.name = "canopy", .tiles = &maps.orchardCanopy, .art = OverheadArt(art),
                                               .z = kCanopyZ}},
                                   .actorSlots = kActorSlots,
                                   .collision = &maps.orchard,
                                   .openings = {{.name = "west door", .side = Side::West, .span = {4, 7},
                                                 .transition = Transition::Scroll,
                                                 .leadsTo = {.area = GroundsArea::Garden, .opening = "east door"}}}},
                },
                .actors = {{.actor = Gardener(art), .in = GroundsArea::Garden, .at = {96, 40},
                            .awakeWhen = WorldKit::AwakeWhen::InLayout}},
                .cellSize = kCell,
                .open = Cell::Floor,
                .view = kView,
            },
        },
        .actors = {{.name = "marker", .actor = Marker(art), .in = {Layout::Castle, CastleArea::Hall}, .at = {76, 68}}},
    };
}
```

**The areas.** An area is its name, its map layers, its actor slot, its collision map, its openings and
its inhabitants. Every area's `"ground"` is a map layer drawn through the area's palette (`Ground`); the
garden and the orchard draw a `"canopy"` over their actor slot, and the garden reserves the depth
`"leaves"` between them for a layer the program builds itself ([9. Drawing it](#9-drawing-it)).

**The openings.** Each says how it is gone through and where it leads:

| Opening | Kind | Leads to |
|---|---|---|
| the hall's `"east door"` ↔ the wide hall's `"west door"` | `Scroll`: the next area slides in, lined up with the gap | the other room of the castle |
| the wide hall's `"stairs down"` ↔ the cellar's `"stairs up"` | `Fade`: the screen darkens and lightens on the entrance | the other room of the castle |
| the wide hall's `"north edge"` ↔ the garden's `"south edge"` | `Seamless`: the two maps meet there for good | the other layout: its `leadsTo` names it |
| the garden's `"east door"` ↔ the orchard's `"west door"` | `Scroll` | the other area of the grounds |

**The inhabitants.** Where an entry is written is how far its actor may roam, its reach:

| Inhabitant | Written in | Reach | Its settings |
|---|---|---|---|
| the marker | the world's `actors`, named `"marker"` | anywhere | — |
| the guard | the hall's `actors` | the hall | — |
| the bat | the cellar's `actors` | the cellar | `atEdge = Despawn`: gone at the edge of its reach; `awakeWhen = OnScreen`: still until the cellar is on screen |
| the hound | the castle's `actors`, starting in the wide hall | the castle | — |
| the gardener | the grounds' `actors`, starting in the garden | the grounds | `awakeWhen = InLayout`: still while the grounds are off screen |

`at` is the inhabitant's top-left in its area's pixels. The marker's name is how the program tells the
camera whom to follow. `WorldKit::validate(world)` is called once after building it, and checks the
openings and the entries alike ([areas.md](areas.md#validate)).

## 5. The mover

Every inhabitant moves with a mover of the program's own, a `Rover`:

```cpp
// The program's own mover: `speed` pixels a tick on each axis of its heading, a pixel at a time, stopping
// where its box would meet a solid, and facing the last axis it was headed along.
class Rover {
public:
    explicit Rover(retropp::Vec2i& at) : at_(&at) {}
    void bind(retropp::Vec2i& at) { at_ = &at; }

    void walk(const RoverConfig& config, Way way, const ActorKit::Solids& solids) {
```

`walk` takes a `Way`, four flags, and the `ActorKit::Solids` the scene hands it each tick: the area's
walls, the solid inhabitants beside it, and what its reach holds it inside. ActorKit takes any mover type
with a `walk`, a `facing`, a `collision` and a `frame` ([actors/movers.md](actors/movers.md)); a game
with a movement module hands its walkers in the same way. The marker's rover is driven by the input's
`way`; each other inhabitant's by its own `Heading`, which its scripts set.

## 6. The inhabitants and their scripts

An inhabitant is an `ActorKit::Actor`, made by a function of the art:

```cpp
ActorKit::Actor Marker(const Art& art) {
    return {.slot = "actors",
            .solid = true,
            .look = LookOf(art, kMarker, art.hall),
            .mover = ActorKit::movedBy<Rover>(RoverConfig{}, &Input::way),
            .setsOff = {ActorKit::On::Touch}};
}
```

```cpp
// Passable, so the marker can walk onto it and touch it.
ActorKit::Actor Hound(const Art& art) {
    return {.slot = "actors",
            .ownValues = {{Heading, retropp::Dir::None}},
            .look = LookOf(art, kHound, art.hall),
            .scripts = {&roam, &goAnywhere},
            .mover = ActorKit::movedBy<Rover>(RoverConfig{}, Heading)};
}
```

The marker is solid and sets off `Touch` in what it walks onto. The hound owns a `Heading`, which its
rover walks by; the guard, the gardener and the bat are written the same way, with scripts of their own.
Each script starts on `ActorKit::Enter`, when the camera comes into the inhabitant's area, and loops:

```cpp
// The guard paces the hall: east to the door, where it is held at the edge for the rest of the leg,
// and back west.
const ScriptKit::Script pace{
    .name = "pace",
    .on = ActorKit::Enter,
    .body = {loop({set(Heading, retropp::Dir::Right), wait(160), set(Heading, retropp::Dir::Left), wait(160)})},
};
```

| Script | Inhabitant | What it does |
|---|---|---|
| `pace` | the guard | east for 160 ticks and west for 160, held at the door's edge, and at the west wall, for the rest of each leg |
| `roam` | the hound | a round of four legs with a pause between: up to the north edge, right to the east wall, down, left; the leg up the longer, so once the hound goes anywhere it takes it across the seam |
| `goAnywhere` | the hound | on `ActorKit::Touch`, asks the program for `GoAnywhere` |
| `tend` | the gardener | east through the garden's door to the orchard's far wall, back west into the middle of the garden, south to the garden's south edge, north to the door's rows |

A leg is a heading held for a number of ticks, so anything that stops an inhabitant on its way — the
marker standing in its path — shortens that leg. Each round has legs that end against a wall or a seam
that holds the inhabitant, held there for the rest of the leg, so the next round starts from the same
place again. While the screen scrolls or fades, the scene holds every inhabitant still, scripts
included, so every leg keeps its full length.
| `flit` | the bat | east, onto the cellar's stairs |

What each script's walk meets is the scene's work: the guard's reach is the hall, so the east door holds
it; the gardener's is the grounds, so it goes through the orchard's door and back, and the seam into the
castle holds it; the bat's `atEdge` is `Despawn`, so the stairs, which lead outside the cellar, remove it
([scene.md](scene.md#the-edge-of-a-reach)).

## 7. The input, the state and the handler

```cpp
// What the player does on one tick: the way the pad is held.
struct Input {
    Way way;
};
```

```cpp
struct State {
    WorldKit::Scene scene;
    std::uint32_t ticks = 0;   // how many ticks have run: the leaves fall by it
};
```

```cpp
// The one game decision in the program: the hound, once touched, goes anywhere.
const ActorKit::Handlers<State> handlers{
    {GoAnywhere, [](State& state, ActorKit::Id asker, const ScriptKit::Arguments&) {
         state.scene.actor(asker).reach(WorldKit::Reach::World);   // this is now a global actor
     }},
};
```

The scene is one value in the program's state. The handler is handed the asking inhabitant's id, the
hound's, and widens its reach to the whole world: from its next walk, the seam into the grounds lets it
through ([scene.md](scene.md#reach)).

## 8. The scene

```cpp
    // The scene: the world, the camera the program creates, handed over, and the program's handler. The
    // camera follows the marker, which it looks at from the first frame.
    WorldKit::Camera camera{Layout::Castle, CastleArea::Hall};
    camera.place(world, {.x = 76, .y = 68, .width = kCell, .height = kCell});
    const ActorKit::Config config{.handlers = &handlers};
    State state{.scene = WorldKit::Scene{world, camera, config}};
    state.scene.follow("marker");

    loop.simTick([&](const retropp::InputState& input) {
        ++state.ticks;
        state.scene.tick(readInput(input), state);
    });
```

`WorldKit::Scene{world, camera, config}` spawns every inhabitant the world lists and keeps the camera.
`config` names the handlers; the scene fills its `world` in. `follow("marker")` names the inhabitant the
camera follows. Each tick, `tick` runs every inhabitant and the camera
([scene.md](scene.md#what-the-scene-does-in-a-tick)):

| What happens | Done by the scene |
|---|---|
| each inhabitant's scripts step and its rover walks | through ActorKit, against the area's walls and its reach |
| the marker goes through a door, onto the stairs, across the seam | the camera's scroll, fade or seam, the marker carried; every other inhabitant holds still while a scroll or fade runs |
| the gardener goes through the orchard's door | it walks on in through the doorway on the other side |
| the hound and the gardener meet the seam | held there: it leads outside their reach |
| the bat steps onto the stairs | removed: `AtEdge::Despawn` |
| the gardener or the bat is off screen | frozen by its `awakeWhen`, and woken where it stopped |

## 9. Drawing it

```cpp
    loop.renderLoop([&] {
        frame.layers = state.scene.layers(state);
        // The garden's leaves on a layer of the program's own, at the depth the garden reserves for them:
        // over the inhabitants and under the canopy, the garden alone on screen or beside the wide hall.
        for (const WorldKit::Shown& shown : state.scene.camera().shown(world)) {
            if (shown.layout == Layout::Grounds && shown.area == GroundsArea::Garden) {
                frame.layers.push_back(LayerKit::SpriteLayer("garden:leaves", WorldKit::LayerZ(world, shown, "leaves"),
                                                             {Leaves(state.ticks, maps.garden, art)},
                                                             {.scroll = shown.scroll, .regions = WorldKit::ViewStencil(kView)}));
            }
        }
        frame.regions = state.scene.fade();
        renderer.renderFrame(frame);
    });
```

`state.scene.layers(state)` is the world's map layers and one sprite layer for the `"actors"` slot,
holding the inhabitants of every area on screen: every area's ground at depth 0, the inhabitants at 5,
the canopy at 10 ([scene.md](scene.md#drawing)). With the wide hall and the garden both on screen, the
hound and the gardener are on that one layer and sort by their feet, and each keeps its sprite as it
crosses the seam. `state.scene.fade()` is the color fill over the whole screen while a fade runs.

The falling leaves are the program's own layer, at the depth the garden reserves for them.
`state.scene.camera().shown(world)` lists the garden whenever it is on screen: as the current area, or
beside the wide hall across the seam. `WorldKit::LayerZ(world, shown, "leaves")` is the depth the
reserved layer is drawn at for the garden on screen: 7, as declared, while the garden is alone, and
spread among the areas on screen otherwise, so the leaves stay over every inhabitant and under every
canopy ([drawing.md](drawing.md#how-the-layers-stack)). `Leaves` is the program's component: eight leaf
sprites, in the garden's pixels, placed by `state.ticks`.

## 10. Where to go next

| To | Read |
|---|---|
| see everything the scene does, and every call on it | [scene.md](scene.md) |
| learn the model: areas, layouts, openings, transitions, the camera | [concepts.md](concepts.md) |
| follow recipes: a shut door, a layer over the player, a seam, an actor that roams, one that hides | [how-to.md](how-to.md) |
| write actors of your own: values, triggers, scripts, movers | [actors/README.md](actors/README.md) |
| look up a type | [areas.md](areas.md), [camera.md](camera.md), [drawing.md](drawing.md) |
