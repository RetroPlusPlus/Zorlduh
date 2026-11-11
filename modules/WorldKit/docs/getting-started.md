# Getting started

This page builds a complete program on the module: four areas — a hall with a door in its east wall
that scrolls into a wider hall; stairs in that hall that fade down to a cellar with stairs back up; and
a garden joined seamlessly along the wider hall's north wall, so it is simply there beyond the wall and
walking into it changes nothing on screen — and a marker you steer with the arrow keys, WASD or a d-pad.
The marker's own movement is a few lines in the program; WorldKit tells it what is solid, takes it
through each opening and draws whatever is on screen.

The full source is [`examples/getting_started.cpp`](../examples/getting_started.cpp). It builds as the
`WorldKit-example-getting-started` target whenever `WORLDKIT_BUILD_EXAMPLES` is on, so what you read
here is what compiles.

## Contents

- [1. Build it](#1-build-it)
- [2. Four maps](#2-four-maps)
- [3. The world: a layout of areas and their openings](#3-the-world-a-layout-of-areas-and-their-openings)
- [4. The marker moves against what is solid](#4-the-marker-moves-against-what-is-solid)
- [5. The camera follows it through the openings](#5-the-camera-follows-it-through-the-openings)
- [6. Drawing it](#6-drawing-it)
- [7. Where to go next](#7-where-to-go-next)

## 1. Build it

The module needs the engine as the CMake target `retropp::engine`, LayerKit as `LayerKit::LayerKit`
and CameraKit as `CameraKit::CameraKit`. Add them and turn the examples on:

```cmake
add_subdirectory(engine)
add_subdirectory(modules/LayerKit)
add_subdirectory(modules/CameraKit)
set(WORLDKIT_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(modules/WorldKit)
```

```sh
cmake -S . -B build
cmake --build build --target WorldKit-example-getting-started
```

The executable lands in the build tree under `modules/WorldKit/`. Run it: a walled hall with a square
marker. Walk out through the gap in the east wall and the next hall slides in while the marker is
carried through; it is wider than the screen, so the screen follows the marker across it and stops at
its edges. Its north wall is open along its whole length, and the green garden is there beyond it: walk
up into it and nothing jumps — the garden was already on screen, and the marker simply carries on.
Step onto the striped stairs in the wide hall and the screen fades out, then in on a small purple
cellar, the marker standing on its stairs. Step off and back on to fade up again. Close the window to
quit.

## 2. Four maps

Each area is its own map: an `IndexGrid` of cells, ringed by wall, with doorways where its openings are
and stairs where its spots are. The program builds them in code; a game loads them from map PNGs with
`retropp::loadMapPng`. The same grid is drawn and walked against.

```cpp
enum class Cell : std::uint16_t { Floor = 0, Wall = 1, Stairs = 2 };

struct Maps {
    retropp::IndexGrid hall = Room(20, 18, Down(19, 7, 10), {});
    retropp::IndexGrid wideHall = Room(30, 18, Both(Down(0, 7, 10), Across(0, 1, 28)), {{20, 5}, {21, 5}});
    retropp::IndexGrid cellar = Room(12, 10, {}, {{5, 4}, {6, 4}});
    retropp::IndexGrid garden = Room(30, 12, Across(11, 1, 28), {});
};
```

The hall's doorway is cells 7–10 of its east wall; the wide hall's is cells 7–10 of its west wall. The
wide hall's north wall and the garden's south wall are open from cell 1 to cell 28 — the whole wall but
the corners. The maps know nothing of each other. How they join is said once, below.

## 3. The world: a layout of areas and their openings

```cpp
enum Layout : std::uint8_t { Castle };
enum Area : std::uint8_t { Hall, WideHall, Cellar, Garden };

WorldKit::Layout CastleLayout(const Maps& maps, const Art& art) {
    return WorldKit::Layout{
        .areas = {
            WorldKit::Area{.tiles = &maps.hall, .art = CatalogThrough(art, art.hall), .collision = &maps.hall,
                           .openings = {{.name = "east door", .side = Side::East, .span = {7, 10},
                                         .transition = Transition::Scroll,
                                         .leadsTo = {.area = Area::WideHall, .opening = "west door"}}}},
            WorldKit::Area{.tiles = &maps.wideHall, .art = CatalogThrough(art, art.hall), .collision = &maps.wideHall,
                           .openings = {{.name = "west door", .side = Side::West, .span = {7, 10},
                                         .transition = Transition::Scroll,
                                         .leadsTo = {.area = Area::Hall, .opening = "east door"}},
                                        {.name = "stairs down", .at = {.x = 20, .y = 5, .width = 2, .height = 1},
                                         .transition = Transition::Fade,
                                         .leadsTo = {.area = Area::Cellar, .opening = "stairs up"}},
                                        {.name = "north edge", .side = Side::North, .span = {1, 28},
                                         .transition = Transition::Seamless,
                                         .leadsTo = {.area = Area::Garden, .opening = "south edge"}}}},
            WorldKit::Area{.tiles = &maps.cellar, .art = CatalogThrough(art, art.cellar), .collision = &maps.cellar,
                           .openings = {{.name = "stairs up", .at = {.x = 5, .y = 4, .width = 2, .height = 1},
                                         .transition = Transition::Fade,
                                         .leadsTo = {.area = Area::WideHall, .opening = "stairs down"}}}},
            WorldKit::Area{.tiles = &maps.garden, .art = CatalogThrough(art, art.garden), .collision = &maps.garden,
                           .openings = {{.name = "south edge", .side = Side::South, .span = {1, 28},
                                         .transition = Transition::Seamless,
                                         .leadsTo = {.area = Area::WideHall, .opening = "north edge"}}}},
        },
        .cellSize = kCell,
        .open = Cell::Floor,
        .view = kView,
    };
}

const WorldKit::World world{.layouts = {CastleLayout(maps, art)}};
WorldKit::validate(world);
```

An **area** is its tile map, the art it is drawn through (a `retropp::TileCatalog`: which tile and
palette each map value is), its collision map and its openings. A **layout** is a list of areas with the
settings they share; the program has one, and a game with an overworld and a dungeon has two. The
**world** is the list of layouts. Layouts and areas are named by the program's enums, whose values are
their indices in the lists.

An **opening** is a way out. `"east door"` is an *edge* opening: a gap in the east wall, cells 7–10 of
it (`.span`). `"stairs down"` is a *spot* opening: a rectangle of cells inside the area (`.at`). Each
says how it is gone through and where it leads: an area (and a layout, when it is not the opening's
own) and the name of an opening in it. The three kinds:

- `Scroll` slides the next area in, lined up with the gap, and carries the marker through.
- `Fade` darkens the screen and lightens it on the next area, the marker at the entrance.
- `Seamless` joins the two areas at the gap for good: the next area is drawn beyond the wall whenever it
  is in view, what is solid continues into it, and walking through only makes it the current one. The
  far end must be seamless too and lead back, so the join is the same from both sides.

Every opening here has one leading back, so each link works both ways; a link is one-way unless its
other end leads back. The cellar's and the garden's art are other palettes, so an area's look is its own.

`WorldKit::validate(world)` is called once after building it. It throws, naming the layout, area and
opening, for a destination that does not exist, a scroll that is not from a wall to the facing wall, a
seam whose far end does not match, two openings that overlap, and the rest of
[areas.md](areas.md#validate).

## 4. The marker moves against what is solid

The marker is an 8×8 box. Each tick, if no transition is running, it moves a pixel along each axis held
when the box there is clear:

```cpp
if (!camera.transitioning()) {
    const WorldKit::AreaSolids solids = camera.solids(world);
    // ... moved = marker shifted by dx; if (!solids.solidBox(moved)) marker = moved; the same for dy ...
}
```

`camera.solids(world)` is the current area's **solid query**: a wall cell is solid, so is everything off
the map — except straight out through an exit, which is how the marker gets to cross the edge, and past
a seam, where the next area's own cells answer. The marker's movement is the program's own; a game
using a movement module hands the same query to it ([how-to.md](how-to.md#plug-in-a-movement-module)).

## 5. The camera follows it through the openings

```cpp
WorldKit::Camera camera{Layout::Castle, Area::Hall};
camera.place(world, marker);          // at the start

// Each tick, after the marker has moved:
const WorldKit::Step step = camera.follow(world, marker);
marker.x += step.move.x;
marker.y += step.move.y;
```

The **camera** is which layout and area the screen is in and where it looks. `follow` takes the box of
the thing the screen follows and does three things. It follows the box by the layout's camera law
(`CameraKit::follow` unless the layout says otherwise), on each axis along which there is room to: the
wide hall is wider than the screen, so the screen follows the marker across it and stops at its edges;
the hall is one screen wide, so it holds still. The camera moves only when the marker moves, with the
marker's own step, and never on its own. It notices the box's center going out through an opening and
starts that opening's transition, or across a seam makes the next area current at once. And while a
transition runs it hands back a **move** the program applies to the marker — the carry through a door
during a scroll, the jump onto the stairs at the middle of a fade, and the change into the next area's
pixels when either ends, or at once across a seam. The marker does nothing else during a transition;
`transitioning()` is how the program knows to hold still. Across a seam nothing runs, and the marker
walks on.

## 6. Drawing it

```cpp
frame.layers = WorldKit::WorldLayers(camera, world, 0);
frame.layers.push_back(LayerKit::SpriteLayer("marker", 10, {{markerSprite}}, {.scroll = camera.scroll(world)}));
frame.regions = WorldKit::Fade(camera, world);
```

`WorldLayers` is one tile layer per area on screen — the current area, any area reached through seams
that is in view, and the next area while a scroll runs — each assembled from its map through its art,
placed where the camera says, and confined to the layout's view, so a part of the screen kept for
something else is never drawn over. The marker's own layer takes `camera.scroll(world)`, the scroll for
things placed in the current area's pixels. `Fade` is the color fill over the whole screen while a fade
runs, and nothing otherwise.

## 7. Where to go next

- The model: areas, layouts, openings, how movement plugs in, transitions, the camera — [concepts.md](concepts.md).
- Recipes: a side-scroller, a one-way drop, a shut door, another layout, a seam, a reserved strip — [how-to.md](how-to.md).
- Every type and member — [areas.md](areas.md), [camera.md](camera.md), [drawing.md](drawing.md).
