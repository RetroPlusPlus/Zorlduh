# TopDownMovement — guide

The guide to the TopDownMovement module, for anyone writing a top-down game on the Polyrhythm engine. Every
page is written against the shipped headers in `include/TopDownMovement/`.

**New here? Start with [getting-started.md](getting-started.md)**: a complete program, three walkers in a
walled room, each moved a different way, explained part by part. Then read [concepts.md](concepts.md) for
the model, [routes.md](routes.md) for walking the engine's routes, [collision.md](collision.md) for how
collision works, and [how-to.md](how-to.md) for recipes.

## What the module is

A top-down game's characters all walk the same way: a direction moves them, a wall stops them, a
diagonal into a wall slides them along it, they face the way they go, and a walk cycle plays while they
move. What tells them where to go differs: the player's pad, an enemy's rule, a scripted route. The
module is that walking, written once, in the engine's own vocabulary — a facing is a `retropp::Dir`, a
position a `retropp::Vec2i`, a collision shape a `retropp::IntRect` or `retropp::ShapePoints`, an
animation a `retropp::Animation`, a route a `retropp::SpritePath`, and what a walker draws is a
`retropp::Sprite` — with one way in for each of the three.

What the module takes care of:

- walking a position the game keeps, a pixel at a time, stopping where the walker's collision shape
  meets something solid;
- 8-way or 4-way movement, chosen per config;
- turning the walker by the rules in [concepts.md](concepts.md#facing), with the pad's press order for a
  walker the pad steers;
- 4 or 8 sprite directions, chosen per config, and the walk animation for the one the sprite faces;
- moving a walker along the engine's `SpritePath`, facing along it and walking while it moves, however
  slowly;
- reading the four directions from the engine's input through the game's own actions;
- answering whether a shape touches a solid cell of a collision map;
- the sprite a walker is.

What stays with the game: where each walker stands (the positions are the game's, in its state), what is
solid (a collision map, a table, anything that answers the question), what heading a walker that is not
the player takes, the routes themselves, the walk animations, the actions and their bindings, and the
layer the sprite goes on.

## Index

### Start here

| Page | Covers |
|---|---|
| [getting-started.md](getting-started.md) | A complete program, a hero, a wanderer and a guard in a walled room, built as `TopDownMovement-example-getting-started`, explained part by part |
| [concepts.md](concepts.md) | The config and the walker; the position a walker is bound to; the three pathways; a tick of walking under each `movement`; the two facings and the two `sprites` settings; the walk animation; drawing; where each part lives |
| [routes.md](routes.md) | The route pathway: the engine's pathing pieces, `routeFrom`, `follow`, facing from travel, waits, interrupts, rounding |
| [collision.md](collision.md) | What is solid and what collides; how the two meet each tick; collision shapes; collision maps; limits |
| [how-to.md](how-to.md) | Recipes: switching maps, combining solids, rules per walker, slower than a pixel a tick, pinning something to a walker, a walker and its position in one copyable value, patrols, detours, wandering, four directions only, diagonal sprites, turning to face something |

### Reference

| Page | Covers | Header |
|---|---|---|
| [walker.md](walker.md) | `Ways`, `Facing`, `DirectionalAnimations`, `WalkerConfig`, `Walker`, `InputWalker`, `readDirections`, `WalkerSprite` | `directions.h`, `walker.h`, `input_walker.h`, `walker_sprite.h` |
| [routes.md](routes.md) | `routeFrom`, `follow` | `route.h` |
| [collision.md](collision.md) | `CollisionShape`, `TileCollision`, `CellValue`, `SolidQuery` | `collision.h` |

## Coverage

| Surface | Status | Page |
|---|---|---|
| A walker bound to a position the game keeps | available | concepts.md |
| 8-way movement; a diagonal into a wall carries on along it | available | concepts.md |
| 4-way movement, by a heading or by the pad | available | concepts.md |
| Four facings and the diagonal rules, with the press order for the pad | available | concepts.md |
| 4 or 8 sprite directions, a walk animation for each | available | concepts.md, walker.md |
| The free pathway: a heading from the game's own rule, against solids | available | concepts.md, how-to.md |
| Routes, waits, sequence modes and detours, through the engine's `SpritePath` | available | routes.md |
| Walking slower than a pixel a tick, on a route | available | routes.md, how-to.md |
| Collision shapes: rectangle, any point- or curve-built `ShapePoints`, the polygon through anchors | available | collision.md |
| Collision against a tile collision map, a table, or any callable | available | collision.md |
| Reading the directions from `retropp::InputState` | available | walker.md |
| The walker's sprite, sorted by the bottom of its collision shape | available | walker.md |
| Collision on a route | not provided — a route goes where it is drawn ([routes.md](routes.md#nothing-collides-on-a-route)) | routes.md |
| A route that crosses from one area into another | not provided — a route's points are in one area's pixels | routes.md |
| Speeds below a pixel a tick off a route | not provided — `speed` is whole pixels; a slower walker takes a route ([how-to.md](how-to.md#walk-slower-than-a-pixel-a-tick)) | how-to.md |
| A collision map placed anywhere but world pixel (0, 0) | not provided — see [collision.md](collision.md#limits) | collision.md |
| More than one open value on a collision map | not provided — see [collision.md](collision.md#limits) | collision.md |
| Analog (stick) steering | not provided | — |
