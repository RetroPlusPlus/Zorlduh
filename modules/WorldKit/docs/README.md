# WorldKit — guide

The guide to the WorldKit module, for anyone building a game on the Polyrhythm engine whose world is
more than one screen. Every page is written against the shipped headers in `include/WorldKit/`.

**New here? Start with [getting-started.md](getting-started.md)**: a complete program, three areas
joined by a door and a staircase, explained part by part. Then read [concepts.md](concepts.md) for the
model and [how-to.md](how-to.md) for recipes.

## What the module is

A game's world is a set of places: the rooms of a dungeon, the screens of an overworld, a cave, a house.
Each has its own map and its own art, and the player goes from one to the next through a door, a gap in
a wall, a staircase. The module is that structure, written once: an **area** is a place with a tile
map, the art it is drawn through, and a collision map; an **opening** is a way out of an area that leads
to an opening in another; going out through one scrolls the next area in or fades to it. Nothing outside
an area exists, so nothing outside one is ever drawn.

What the module takes care of:

- the world's data: areas and their openings, checked once so a wrong link is found at startup;
- what is solid in an area, for anything that moves — a walker from a movement module, a mover the game
  writes itself; the module moves nothing;
- noticing when the thing the screen follows goes out through an opening, and running the opening's
  transition: a scroll that lines the next area up with the gap and carries the thing through, or a fade
  that lands it at the entrance;
- the camera within an area, through a camera law the game may replace;
- the areas on screen as tile layers, and the fade over everything.

What stays with the game: the maps and art, the naming of areas, what moves and how, and the layers
of things placed in the world (which take the camera's scroll).

## Index

### Start here

| Page | Covers |
|---|---|
| [getting-started.md](getting-started.md) | A complete program, three areas joined by a door and stairs, built as `WorldKit-example-getting-started`, explained part by part |
| [concepts.md](concepts.md) | Areas and openings; how movement plugs in; a tick of following; scrolls and fades; the camera and its law; drawing; where each part lives in a game |
| [how-to.md](how-to.md) | Recipes: a side-scroller's edges, a one-way drop, a door that is shut, different art per area, a camera law of your own, a movement module, the fade's look |

### Reference

| Page | Covers | Header |
|---|---|---|
| [areas.md](areas.md) | `Side`, `Transition`, `Span`, `Destination`, `Opening`, `CellValue`, `Area`, `CameraLaw`, `Layout`, `validate` | `area.h` |
| [camera.md](camera.md) | `Camera`, `Step`, `Shown`, `AreaSolids` | `camera.h`, `solids.h` |
| [drawing.md](drawing.md) | `WorldLayers`, `Fade`, `AreaLayerKey` | `world_layers.h` |

## Coverage

| Surface | Status | Page |
|---|---|---|
| Areas with their own maps and art; openings in walls and at spots; one-way and two-way links | available | areas.md |
| Checks on the data: names, placement, overlap, destinations, scroll geometry | available | areas.md |
| What is solid: the collision map, off the map, the way out through an exit | available | camera.md |
| A scroll to the next area, lined up with the opening, carrying the thing through | available | camera.md |
| A fade to the next area, landing at the entrance | available | camera.md |
| The camera within an area through `CameraKit::follow` or a law of the game's own | available | areas.md, camera.md |
| One tile layer per area on screen; the fade as a frame region | available | drawing.md |
| Openings that open and close (a locked door, a shutter) | not provided — see [how-to.md](how-to.md#shut-a-door) | how-to.md |
| Transitions other than a straight scroll and a fade; a scroll from a spot | not provided | — |
| More than one thing followed; things in other areas while they are off screen | not provided | — |
