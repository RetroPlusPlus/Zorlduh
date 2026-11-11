# Getting started

This page builds a complete program on the module: three areas — a hall with a door in its east wall
that scrolls into a wider hall, and stairs in that hall that fade down to a cellar with stairs back up —
and a marker you steer with the arrow keys, WASD or a d-pad. The marker's own movement is a few lines in
the program; WorldKit tells it what is solid, carries it through each opening and draws whatever is on
screen.

The full source is [`examples/getting_started.cpp`](../examples/getting_started.cpp). It builds as the
`WorldKit-example-getting-started` target whenever `WORLDKIT_BUILD_EXAMPLES` is on, so what you read
here is what compiles.

## Contents

- [1. Build it](#1-build-it)
- [2. Three maps](#2-three-maps)
- [3. The world: areas and their openings](#3-the-world-areas-and-their-openings)
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
marker. Walk out through the gap in the east wall and the next hall scrolls in while the marker is
carried through; it is wider than the screen, so the screen follows the marker across it and stops at
its edges. Step onto the striped stairs and the screen fades out, then in on a small purple cellar, the
marker standing on its stairs. Step off and back on to fade up again. Close the window to quit.

## 2. Three maps

Each area is its own map: an `IndexGrid` of cells, ringed by wall, with doorways where its openings are
and stairs where its spots are. The program builds them in code; a game loads them from map PNGs with
`retropp::loadMapPng`. The same grid is drawn and walked against.

```cpp
enum class Cell : std::uint16_t { Floor = 0, Wall = 1, Stairs = 2 };

struct Maps {
    retropp::IndexGrid hall = Room(20, 18, {{19, 7}, {19, 8}, {19, 9}, {19, 10}}, {});
    retropp::IndexGrid wideHall = Room(30, 18, {{0, 7}, {0, 8}, {0, 9}, {0, 10}}, {{20, 5}, {21, 5}});
    retropp::IndexGrid cellar = Room(12, 10, {}, {{5, 4}, {6, 4}});
};
```

The hall's doorway is cells 7–10 of its east wall; the wide hall's is cells 7–10 of its west wall. The
two maps know nothing of each other. How they join is said once, below.

## 3. The world: areas and their openings

```cpp
enum Area : std::uint8_t { Hall, WideHall, Cellar };

WorldKit::Layout World(const Maps& maps, const Art& art) {
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
                                         .leadsTo = {.area = Area::Cellar, .opening = "stairs up"}}}},
            WorldKit::Area{.tiles = &maps.cellar, .art = CatalogThrough(art, art.cellar), .collision = &maps.cellar,
                           .openings = {{.name = "stairs up", .at = {.x = 5, .y = 4, .width = 2, .height = 1},
                                         .transition = Transition::Fade,
                                         .leadsTo = {.area = Area::WideHall, .opening = "stairs down"}}}},
        },
        .cellSize = kCell,
        .open = Cell::Floor,
        .view = kView,
    };
}
```

An **area** is its tile map, the art it is drawn through (a `retropp::TileCatalog`: which tile and
palette each map value is), its collision map and its openings. The areas are named by the program's
`Area` enum, whose values are their indices in the list.

An **opening** is a way out. `"east door"` is an *edge* opening: a gap in the east wall, cells 7–10 of
it (`.span`). `"stairs down"` is a *spot* opening: a rectangle of cells inside the area (`.at`). Each
says how it is gone through — `Scroll` slides the next area in, lined up with the gap; `Fade` darkens the
screen and lightens it on the next area — and where it leads: an area and the name of an opening in it.
Every opening here has one leading back, so each link works both ways; a link is one-way unless its
other end leads back.

The cellar's art is a different palette, so an area's look is its own.

`WorldKit::validate(layout)` is called once after building it. It throws, naming the opening, for a
destination that does not exist, a scroll that is not from a wall to the facing wall, two openings that
overlap, and the rest of [areas.md](areas.md#validate).

## 4. The marker moves against what is solid

The marker is an 8×8 box. Each tick, if no transition is running, it moves a pixel along each axis held
when the box there is clear:

```cpp
if (!camera.transitioning()) {
    const WorldKit::AreaSolids solids = camera.solids(layout);
    // ... moved = marker shifted by dx; if (!solids.solidBox(moved)) marker = moved; the same for dy ...
}
```

`camera.solids(layout)` is the current area's **solid query**: a wall cell is solid, so is everything off
the map — except straight out through an exit, which is how the marker gets to cross the edge. The
marker's movement is the program's own; a game using a movement module hands the same query to it
([how-to.md](how-to.md#plug-in-a-movement-module)).

## 5. The camera follows it through the openings

```cpp
WorldKit::Camera camera{Area::Hall};
camera.place(layout, marker);          // at the start

// Each tick, after the marker has moved:
const WorldKit::Step step = camera.follow(layout, marker);
marker.x += step.move.x;
marker.y += step.move.y;
```

The **camera** is which area the screen is in and where it looks. `follow` takes the box of the thing
the screen follows and does three things: it looks at the box by the layout's camera law
(`CameraKit::follow` unless the layout says otherwise, so the screen follows the marker across the wide
hall and stops at its edges); it notices the box's center going out through an opening and starts that
opening's transition; and while a transition runs it hands back a **move** the program applies to the
marker — the carry through a door during a scroll, the jump onto the stairs at the middle of a fade, and
the change into the next area's pixels when either ends. The marker does nothing else during a
transition; `transitioning()` is how the program knows to hold still.

## 6. Drawing it

```cpp
frame.layers = WorldKit::WorldLayers(camera, layout, 0);
frame.layers.push_back(LayerKit::SpriteLayer("marker", 10, {{markerSprite}}, {.scroll = camera.scroll(layout)}));
frame.regions = WorldKit::Fade(camera, layout);
```

`WorldLayers` is one tile layer per area on screen — the current area, or both areas while a scroll
runs — each assembled from its map through its art and placed where the camera says. The marker's own
layer takes `camera.scroll(layout)`, the scroll for things placed in the current area's pixels. `Fade`
is the color fill over the whole screen while a fade runs, and nothing otherwise.

## 7. Where to go next

- The model: areas, openings, how movement plugs in, transitions, the camera — [concepts.md](concepts.md).
- Recipes: a side-scroller, a one-way drop, a shut door, a law of your own — [how-to.md](how-to.md).
- Every type and member — [areas.md](areas.md), [camera.md](camera.md), [drawing.md](drawing.md).
