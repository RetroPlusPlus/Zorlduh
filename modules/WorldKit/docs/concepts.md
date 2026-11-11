# Concepts

The model behind the module: a world is layouts of areas joined by openings; anything may move through
it; the camera follows one thing and takes it through the openings; the areas on screen are drawn,
layer by layer, with the things in them at the depths each area gives.

## Contents

- [Areas and layouts](#areas-and-layouts)
- [Openings](#openings)
- [Movement plugs in](#movement-plugs-in)
- [A tick of following](#a-tick-of-following)
- [A scroll](#a-scroll)
- [A fade](#a-fade)
- [A seamless join](#a-seamless-join)
- [The camera and its law](#the-camera-and-its-law)
- [Drawing](#drawing)
- [Where each part lives in a game](#where-each-part-lives-in-a-game)

## Areas and layouts

An area is one place: a collision map, its openings, and the layers it is drawn in — one or more tile
maps the collision map's size, each through its own art at its own depth, and the actor slots, named
depths where the things moving in it are drawn among those layers. A dungeon room, an overworld screen,
a cave, a shop: the module does not distinguish them. Every position in an area is in that area's own
pixels, from its maps' top-left; the area's size is its collision map's. Areas have no position in a larger world. Two areas sit side by side only for as long as a
scroll between them runs, or for good where a seam joins them — and then only as far as the opening
between them says.

Because an area is its own map, nothing outside it exists: no void cells, nothing to clip. Where the
view reaches past an area's edge and no other area is attached there, the layers below show through.

Areas are grouped in layouts: a layout is a list of areas that share a cell size, an open collision
value, a view, a camera law and the settings of their transitions. An overworld is a layout; a dungeon
is another; a world is the list of layouts. Layouts and areas are named by their indices. A game writes
an enum for each, whose values are the indices, so its openings read `.leadsTo = {.layout =
Layout::DungeonOne, .area = Room::Entrance, ...}`, and whatever the module hands back about a place
(the place entered, the areas shown) is a pair of those values.

## Openings

An opening is a way out of an area and, through its `leadsTo`, a way into another. It is one of two
kinds:

- An **edge opening** is a gap in a wall: `side` says which wall and `span` which cells along it, down
  the east and west walls, across the north and south ones. A wall takes any number of them, so long as
  they do not overlap.
- A **spot opening** is a rectangle of cells inside the area, `at`: stairs, a cave mouth, a hole.

Each opening carries its `transition`, how it is gone through, and its destination: a layout (its own
unless it names another), an area and the name of an opening in it, and for a scroll or a seam an
optional `offset` that shifts the next area, in cells, from where the pair of openings lines it up. The
destination is an entrance; whether it is also an exit is its own business. An opening with no
destination is only an entrance. So a door that works both ways is two openings that lead to each
other, and a one-way drop is an opening whose other end leads nowhere.

The cells themselves are the map's: a doorway cell is open in the collision map, a wall cell is not.
The opening only says where the way out is and where it goes. A scroll or a seam is in a wall and leads
to an opening in the facing wall of the next area, through a span of the same length, so the two can be
lined up; a seam's far end is a seam leading back, with the opposite offset, so the join is one join
from both sides; a fade goes between any two openings.

## Movement plugs in

The module moves nothing. What is solid in an area is a query — `camera.solids(world)` — that answers
whether a rectangle of pixels touches anything solid: a cell whose collision value is not the layout's
`open`, or anywhere off the map except straight out through an exit. Anything that moves asks it: a
walker from a movement module (the query has the `solidBox` member such modules look for), or a mover a
game writes in a few lines. The module never sees what moves, only the box of the one thing the screen
follows, handed to `follow` each tick.

The way out through an exit is what lets the followed thing cross the edge: past a scroll or a fade, the
cells straight out from an edge opening's span, as far out as anything goes, are open; past a seam they
are the next area's own cells, read from its collision map where it is attached, and solid beyond its
edge. Everywhere else off the map is solid, as is everything past an opening that leads nowhere.

## A tick of following

```cpp
const WorldKit::Step step = camera.follow(world, box);
thing.moveBy(step.move);
if (step.entered) { /* the thing is now in area step.entered->area of layout step.entered->layout */ }
```

Each tick the game hands the camera the box of the thing the screen follows, and applies the move it
hands back. While nothing is running: the camera follows the box ([below](#the-camera-and-its-law)),
and the box's center going out through an opening starts that opening's transition — out past a wall
through a gap's span, or into a spot from outside it — or, through a seam, makes the next area current
at once. The move is nothing, except across a seam.

While a transition runs the game moves nothing of its own (`transitioning()` says so) and the move is
what the transition asks: the carry during a scroll, the jump at the middle of a fade, the change into
the next area's pixels when either ends. `entered` is the place entered, on the tick the camera's area
changes.

## A scroll

The next area is set against the wall the thing went out through, lined up so its entrance span is
level with the exit span, then shifted by the destination's offset. Over `layout.scrollTicks` ticks the
camera slides along the exit's axis until the next area's wall is at the edge of the view — and across
that axis not at all, unless the layout's `scrollAligns`, when it also moves the least it must for the
view to lie within the next area if that area is at least as big as the view there. The thing is
carried `layout.carry` pixels straight in, a share each tick. Both areas are on screen, each on its own
layer, and anything placed in the old area's pixels (the thing being carried) stays placed by the
camera's scroll. On the last tick the move also takes the thing into the next area's pixels — the camera
makes the same change — and `entered` is the next area. Where the next area then sits on screen is where
following goes on from: an area smaller than the view lands against the door's side of the screen and
stays there; a bigger one comes into its own place as the thing walks that way.

## A fade

Over the layout's `fadeTicks` the screen darkens to the layout's `fadeColor`; at the middle the thing
jumps to the entrance — centered on a spot, or just inside a wall's gap and centered along it — the next
area becomes current, the camera is placed by the law, and `entered` says so; over the next layout's
`fadeTicks` the screen lightens in its `fadeColor`. The jump is the move handed back on that tick, in the
next area's pixels. A thing that lands on a spot does not go straight back through it: a spot starts a
fade only when the center comes into it from outside.

## A seamless join

Two areas joined at a gap by a seam behave as one map there. The next area is drawn attached beyond the
wall whenever its rectangle is in view; the solid query continues into it past the wall within the
span; the camera's law reaches straight across the seam, so the camera follows the thing up to and over
the wall without stopping. When the thing's center crosses the wall, the next area becomes current on
that tick: the move is the change into its pixels, the camera makes the same change so nothing on
screen moves, nothing runs, and `entered` is the place. Walking back crosses the same way.

What is drawn reaches through seams: the areas joined to the current one, the areas joined to those,
and so on, each drawn where the chain of seams attaches it whenever it is in view — so a room hanging
off a small room's side is there in the view from the big room beside them. The layout's `seamDepth`
limits how many seams away that reaches (0, the default, for any number). What the camera's law reaches
across and what is solid stay one seam deep: the thing followed only ever touches the next area, and
crossing into it makes that area current.

## The camera and its law

The camera within an area is CameraKit's: a `CameraKit::Camera` the module places by the layout's
**camera law** — `CameraKit::follow` unless the layout names another: any function of the shape
`(bounds, view, focus) → position`, so a side-scroller hands in its own and the rest of the module is
unchanged. The rectangle handed to the law is the current area's, reaching across each seam into the
area beyond.

How the camera follows is the module's. On each axis along which that rectangle is at least as big as
the view, the camera moves only when the followed thing moves, with the thing's own step, toward where
the law puts it and no further, and never against the thing's motion. In an area on its own that is the
law exactly: the thing stays centered until an edge stops the camera, and walking back from the edge
the camera stays until the thing is centered again. After a seam or a scroll has left the camera away
from the law's place, it means the camera stays where it is and comes to that place only as the thing
walks toward it — never on its own. On an axis along which the rectangle is smaller than the view, the
camera holds still from one transition to the next.

## Drawing

The areas on screen are tile layers: one per map layer of the current area, of each area reached
through seams that is in view, and of the next area during a scroll, each assembled from its map through
its art, at its own depth, keyed `"area:<layout>:<area>:<layer>"`, placed by the camera's scroll for that
area, and confined to the layout's view — the part of the screen the world is given; the rest is the
game's.

Depth is the engine's z axis, and the map layers and the actor slots share it. The things a game draws
in the current area go on one sprite layer per slot, built by `Actors` at the current area's depth for
that slot, scrolled and confined like the map layers: so the player walks over a path, under a canopy,
into the shade of a cave mouth, by depth alone. A cell of a layer that draws nothing is a slot of its
sheet whose pixels are all transparent indices, and the layers below show through it. Whatever is drawn
over the whole world — effects, a HUD — takes a depth above `TopZ(world)`.

The fade is one region over the whole screen, returned only while a fade runs. All of it is in
`world_layers.h`, the one header that builds draw types.

## Where each part lives in a game

| Part | Built | Lives in |
|---|---|---|
| `World` | once, after the maps load, then `validate`d | wherever the game keeps loaded data |
| `Camera` | at the start, then `place`d at the first thing it follows | the game state |
| `camera.solids(world)` | every tick | the tick, handed to whatever moves |
| `camera.follow(world, box)` | every tick, after the thing has moved | the tick |
| `WorldLayers(...)`, `Actors(...)`, `Fade(...)` | every frame | the components that draw the world |
| the depths of the map layers and the actor slots, `TopZ(world)` | once, with the world | the game's one depth table |
