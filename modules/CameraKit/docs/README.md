# CameraKit — guide

The guide to the CameraKit module, for anyone writing a game on the Polyrhythm engine whose world is
larger than its screen. Every page is written against the shipped headers in `include/CameraKit/`.

**New here? Start with [getting-started.md](getting-started.md)**: a complete program, a marker on a map
twice the screen's size, explained part by part. Then read [concepts.md](concepts.md) for the model and
[how-to.md](how-to.md) for recipes.

## What the module is

A game whose world is larger than its screen has a camera: the world pixel the screen's top-left shows.
Everything about it is one of three questions. Where is it now? Where should it be? How does it get
there? The module answers them with a value, a law and two moves, in the engine's own vocabulary: a
position is a `retropp::Vec2i`, the rectangle the camera may show a `retropp::IntRect`, the screen a
`retropp::PixelSize`, and what a layer takes a `retropp::LayerScroll`.

What the module takes care of:

- holding where the camera is, as a plain value in the game's state;
- the follow law: center a point, stop at the edges of what may be shown, center what is smaller than
  the screen;
- moving the camera somewhere at once, or in a straight line over a number of ticks;
- the scroll a layer takes to show the camera's position where the screen sits on it.

What stays with the game: what the camera looks at (a player, a cutscene's target), what it may show (a
map, an area), and which layers take its scroll.

## Index

### Start here

| Page | Covers |
|---|---|
| [getting-started.md](getting-started.md) | A complete program, a marker on a map twice the screen's size, built as `CameraKit-example-getting-started`, explained part by part |
| [concepts.md](concepts.md) | The camera value; the view and its scroll; laws; moves; where each part lives in a game |
| [how-to.md](how-to.md) | Recipes: a HUD strip, a cutscene pan, holding the camera still, a law of your own, two cameras |

### Reference

| Page | Covers | Header |
|---|---|---|
| [camera.md](camera.md) | `View`, `Camera`, `follow` | `camera.h`, `laws.h` |

## Coverage

| Surface | Status | Page |
|---|---|---|
| The camera as a value; place; scroll for a view | available | camera.md |
| The follow law: center, stop at the edges, center a small rectangle | available | camera.md |
| A straight pan over N ticks, resolved through the engine's tween | available | camera.md |
| A side-scrolling law: a vertical band, look-ahead, a floor lock | not provided | — |
| Eased pans, pans along a path, screen shake | not provided | — |
