# WorldKit — guide

The guide to the WorldKit module, for anyone building a game on the Polyrhythm engine whose world is
more than one screen. Every page is written against the shipped headers in `include/WorldKit/`.

**New here? Start with [getting-started.md](getting-started.md)**: a complete program, four areas
joined by a door, a staircase and a seam, explained part by part. Then read [concepts.md](concepts.md)
for the model and [how-to.md](how-to.md) for recipes.

## What the module is

A game's world is a set of places: the rooms of a dungeon, the screens of an overworld, a cave, a house.
Each has its own map and its own art, and the player goes from one to the next through a door, a gap in
a wall, a staircase — or simply walks on, where two maps meet. The module is that structure, written
once: an **area** is a place with a tile map, the art it is drawn through, and a collision map; a
**layout** is a set of areas that share a cell size, a view and the settings of their transitions — an
overworld, a dungeon; a **world** is its layouts; an **opening** is a way out of an area that leads to an
opening in another, in the same layout or any other, and says how it is gone through: the next area
scrolls in, the screen fades to it, or the two are joined seamlessly and the next area is there beyond
the wall whenever it is in view. Nothing outside an area exists, so nothing outside one is ever drawn.

What the module takes care of:

- the world's data: layouts, areas and their openings, checked once so a wrong link is found at startup;
- what is solid in an area, for anything that moves — a walker from a movement module, a mover the game
  writes itself; the module moves nothing;
- noticing when the thing the screen follows goes out through an opening, and taking it through: a
  scroll that slides the next area in and carries the thing through, a fade that lands it at the
  entrance, or across a seam, the change into the next area's pixels with nothing on screen moving;
- the camera: within an area by a camera law the game may replace, following only where there is room
  to follow and only as the thing moves;
- the areas on screen as tile layers, confined to the part of the screen the world is given, and the
  fade over everything.

What stays with the game: the maps and art, the naming of layouts and areas, what moves and how, the
layers of things placed in the world (which take the camera's scroll), and whatever fills the rest of the
screen.

## Index

### Start here

| Page | Covers |
|---|---|
| [getting-started.md](getting-started.md) | A complete program, four areas joined by a door, stairs and a seam, built as `WorldKit-example-getting-started`, explained part by part |
| [concepts.md](concepts.md) | Areas, layouts and openings; how movement plugs in; a tick of following; scrolls, fades and seams; the camera and its law; drawing; where each part lives in a game |
| [how-to.md](how-to.md) | Recipes: a side-scroller's edges, a one-way drop, a door that is shut, different art per area, a camera law of your own, a movement module, the fade's look, another layout, a seam, a reserved part of the screen |

### Reference

| Page | Covers | Header |
|---|---|---|
| [areas.md](areas.md) | `Side`, `Transition`, `Span`, `Destination`, `Opening`, `CellValue`, `Area`, `CameraLaw`, `Layout`, `World`, `Place`, `validate` | `area.h` |
| [camera.md](camera.md) | `Camera`, `Step`, `Shown`, `AreaSolids` | `camera.h`, `solids.h` |
| [drawing.md](drawing.md) | `WorldLayers`, `Fade`, `AreaLayerKey`, `ViewStencil` | `world_layers.h` |

## Coverage

| Surface | Status | Page |
|---|---|---|
| Areas with their own maps and art; openings in walls and at spots; one-way and two-way links | available | areas.md |
| Several layouts in one world, each with its own cell size, view and settings; openings leading between them | available | areas.md |
| Checks on the data: names, placement, overlap, destinations, scroll and seam geometry | available | areas.md |
| What is solid: the collision map, off the map, the way out through an exit, the next area past a seam | available | camera.md |
| A scroll to the next area, sliding along the exit's axis, carrying the thing through | available | camera.md |
| A fade to the next area, landing at the entrance | available | camera.md |
| A seamless join: the next area drawn beyond the wall, walked into with nothing moving on screen | available | camera.md |
| The camera within an area through `CameraKit::follow` or a law of the game's own; following only where there is room, and only as the thing moves | available | areas.md, camera.md |
| One tile layer per area on screen, confined to the view; the fade as a frame region | available | drawing.md |
| Openings that open and close (a locked door, a shutter) | not provided — see [how-to.md](how-to.md#shut-a-door) | how-to.md |
| A scroll or a seam from a spot | not provided | — |
| More than one thing followed; things in other areas while they are off screen | not provided | — |
