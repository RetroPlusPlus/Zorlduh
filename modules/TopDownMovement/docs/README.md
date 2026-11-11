# TopDownMovement — guide

The guide to the TopDownMovement module, for anyone writing a top-down game on the Polyrhythm engine. Every
page is written against the shipped headers in `include/TopDownMovement/`.

**New here? Start with [getting-started.md](getting-started.md)**: a complete program, a walker in a
walled room, explained part by part. Then read [concepts.md](concepts.md) for the model,
[collision.md](collision.md) for how collision works, and [how-to.md](how-to.md) for recipes.

## What the module is

A top-down game's characters all walk the same way: a direction held moves them, a wall stops them, a
diagonal into a wall slides them along it, they face the way they walk, and a walk cycle plays while
they move. The module is that behavior, written once, in the engine's own vocabulary: the facing is a
`retropp::Dir`, a position a `retropp::Vec2i`, a collision shape a `retropp::IntRect` or
`retropp::ShapePoints`, an animation a `retropp::Animation`, and what a walker draws is a
`retropp::Sprite`.

What the module takes care of:

- moving a walker by the directions held, a pixel at a time, stopping where its collision shape meets
  something solid;
- turning it by the law in [concepts.md](concepts.md#facing);
- playing its walk animation while it moves and resting it on frame 0 while it stands;
- reading the four directions from the engine's input through the game's own actions;
- answering whether a shape touches a solid cell of a collision map;
- the sprite a walker is.

What stays with the game: what is solid (a collision map, a table, anything that answers the question),
the walk animations themselves, the actions and their bindings, and the layer the sprite goes on.

## Index

### Start here

| Page | Covers |
|---|---|
| [getting-started.md](getting-started.md) | A complete program, a walker in a walled room, built as `TopDownMovement-example-getting-started`, explained part by part |
| [concepts.md](concepts.md) | The config and the walker; a tick of walking; the facing law; the walk animation; drawing |
| [collision.md](collision.md) | What is solid and what collides; how the two meet each tick; collision shapes; collision maps; limits |
| [how-to.md](how-to.md) | Recipes: switching maps, combining solids, different rules per walker, a walker slower than a pixel a tick, pinning something to a walker |

### Reference

| Page | Covers | Header |
|---|---|---|
| [walker.md](walker.md) | `WalkerConfig`, `DirectionalAnimations`, `Walker`, `readDirections`, `WalkerSprite` | `walker.h`, `directions.h`, `walker_sprite.h` |
| [collision.md](collision.md) | `CollisionShape`, `TileCollision`, `CellValue`, `SolidQuery` | `collision.h` |

## Coverage

| Surface | Status | Page |
|---|---|---|
| Eight-direction walking; a diagonal into a wall carries on along it | available | concepts.md |
| Four facings and the diagonal facing law | available | concepts.md |
| A walk animation per facing | available | concepts.md, walker.md |
| Collision shapes: rectangle, any point- or curve-built `ShapePoints`, the polygon through anchors | available | collision.md |
| Collision against a tile collision map, a table, or any callable | available | collision.md |
| Reading the directions from `retropp::InputState` | available | walker.md |
| The walker's sprite, sorted by the bottom of its collision shape | available | walker.md |
| A collision map placed anywhere but world pixel (0, 0) | not provided — see [collision.md](collision.md#limits) | collision.md |
| More than one open value on a collision map | not provided — see [collision.md](collision.md#limits) | collision.md |
| Analog (stick) steering, sub-pixel speeds, eight facings | not provided | — |
