# Concepts

The model behind the module: a world is areas joined by openings; anything may move through it; the
camera follows one thing and takes it through the openings; the areas on screen are drawn.

## Contents

- [Areas](#areas)
- [Openings](#openings)
- [Movement plugs in](#movement-plugs-in)
- [A tick of following](#a-tick-of-following)
- [A scroll](#a-scroll)
- [A fade](#a-fade)
- [The camera and its law](#the-camera-and-its-law)
- [Drawing](#drawing)
- [Where each part lives in a game](#where-each-part-lives-in-a-game)

## Areas

An area is one place: a tile map, the art it is drawn through, a collision map the same size, and its
openings. A dungeon room, an overworld screen, a cave, a shop: the module does not distinguish them.
Every position in an area is in that area's own pixels, from its map's top-left; the area's size is its
map's. Areas have no position in a larger world. Two areas sit side by side only for as long as a
scroll between them runs, and then only as far as the opening between them says.

Because an area is its own map, nothing outside it exists: no void cells, nothing to clip. An area
smaller than the screen sits in the middle with the backdrop around it.

Areas are kept in a `Layout` and named by their indices. A game writes an enum whose values are the
indices, so its openings read `.leadsTo = {.area = Area::Lake, ...}`, and whatever the module hands back
about an area (the area entered, the areas shown) is one of those values.

## Openings

An opening is a way out of an area and, through its `leadsTo`, a way into another. It is one of two
kinds:

- An **edge opening** is a gap in a wall: `side` says which wall and `span` which cells along it, down
  the east and west walls, across the north and south ones. A wall takes any number of them, so long as
  they do not overlap.
- A **spot opening** is a rectangle of cells inside the area, `at`: stairs, a cave mouth, a hole.

Each opening carries its `transition`, how it is gone through, and its destination: an area and the
name of an opening in it. The destination is an entrance; whether it is also an exit is its own
business. An opening with no destination is only an entrance. So a door that works both ways is two
openings that lead to each other, and a one-way drop is an opening whose other end leads nowhere.

The cells themselves are the map's: a doorway cell is open in the collision map, a wall cell is not.
The opening only says where the way out is and where it goes. A scroll opening is in a wall and leads
to an opening in the facing wall of the next area, through a span of the same length, so the two can be
lined up; a fade goes between any two openings.

## Movement plugs in

The module moves nothing. What is solid in an area is a query — `camera.solids(layout)` — that answers
whether a rectangle of pixels touches anything solid: a cell whose collision value is not the layout's
`open`, or anywhere off the map except straight out through an exit. Anything that moves asks it: a
walker from a movement module (the query has the `solidBox` member such modules look for), or a mover a
game writes in a few lines. The module never sees what moves, only the box of the one thing the screen
follows, handed to `follow` each tick.

The way out through an exit is what lets the followed thing cross the edge: the cells straight out from
an edge opening's span, as far out as anything goes, are open. Everywhere else off the map is solid, as
is everything past an opening that leads nowhere.

## A tick of following

```cpp
const WorldKit::Step step = camera.follow(layout, box);
thing.moveBy(step.move);
if (step.entered) { /* the thing is now in area *step.entered */ }
```

Each tick the game hands the camera the box of the thing the screen follows, and applies the move it
hands back. While nothing is running: the camera looks at the box by the layout's law, and the box's
center going out through an opening starts that opening's transition — out past a wall through a gap's
span, or into a spot from outside it. The move is nothing.

While a transition runs the game moves nothing of its own (`transitioning()` says so) and the move is
what the transition asks: the carry during a scroll, the jump at the middle of a fade, the change into
the next area's pixels when either ends. `entered` is the next area on the tick the camera's area
changes.

## A scroll

The next area is set against the wall the thing went out through, lined up so its entrance span is
level with the exit span. Over `layout.scrollTicks` ticks the camera pans from where it was to where the
law puts it in the next area for the thing's landing point, and the thing is carried `layout.carry`
pixels straight in, a share each tick. Both areas are on screen, each on its own layer, and anything
placed in the old area's pixels (the thing being carried) stays placed by the camera's scroll. On the
last tick the move also takes the thing into the next area's pixels — the camera makes the same change
— and `entered` is the next area.

## A fade

Over `layout.fadeTicks` ticks the screen darkens to `layout.fadeColor`; at the middle the thing jumps
to the entrance — centered on a spot, or just inside a wall's gap and centered along it — the next area
becomes current and `entered` says so; over the same again the screen lightens. The jump is the move
handed back on that tick, in the next area's pixels. A thing that lands on a spot does not go straight
back through it: a spot starts a fade only when the center comes into it from outside.

## The camera and its law

The camera within an area is CameraKit's: a `CameraKit::Camera` the module places each tick by the
layout's **camera law** over the current area's rectangle, with the followed box's center as the focus.
The law is `CameraKit::follow` unless the layout names another: any function of the shape
`(bounds, view, focus) → position`, so a side-scroller hands in its own and the rest of the module is
unchanged. A scroll's pan is the same camera's.

## Drawing

The areas on screen are tile layers: one for the current area, two during a scroll, each assembled from
its map through its art, keyed `"area:<index>"`, and placed by the camera's scroll for that area. A
game's own layers of things placed in the world take `camera.scroll(layout)`. The fade is one region
over the whole screen, returned only while a fade runs. All of it is in `world_layers.h`, the one header
that builds draw types.

## Where each part lives in a game

| Part | Built | Lives in |
|---|---|---|
| `Layout` | once, after the maps load, then `validate`d | wherever the game keeps loaded data |
| `Camera` | at the start, then `place`d at the first thing it follows | the game state |
| `camera.solids(layout)` | every tick | the tick, handed to whatever moves |
| `camera.follow(layout, box)` | every tick, after the thing has moved | the tick |
| `WorldLayers(...)`, `Fade(...)`, `camera.scroll(layout)` | every frame | the components that draw the world |
