# Collision

Collision has two parts, and you supply both:

- **Solids**: what in the world is solid. You pass it to `walk` each tick.
- **A collision shape**: which pixels of the walker collide. You set it in the `WalkerConfig`.

Each tick, `walk` tests the shape against the solids before every pixel it moves.

```cpp
#include "TopDownMovement/collision.h"   // CollisionShape, TileCollision, CellValue, SolidQuery
```

`walker.h` and `input_walker.h` include this header.

## Contents

- [Overview](#overview)
- [Solids](#solids)
- [Collision shapes](#collision-shapes)
- [How a tick tests collision](#how-a-tick-tests-collision)
- [`CollisionShape`](#collisionshape)
- [`TileCollision`](#tilecollision)
- [`CellValue`](#cellvalue)
- [`SolidQuery`](#solidquery)
- [Limits](#limits)
- [Cost](#cost)

## Overview

```cpp
enum class Cell : std::uint16_t { Open = 0, Wall = 1 };

// Solids: a collision map with 8-pixel cells. Every cell except Cell::Open is solid.
const retropp::IndexGrid map = retropp::loadMapPng("assets/maps/room.png");
const TopDownMovement::TileCollision walls{.map = &map, .cellSize = 8, .open = Cell::Open};

// The collision shape: a box 10 wide and 8 tall, 3 pixels in and 8 down from the walker's top-left.
const TopDownMovement::WalkerConfig heroConfig{
    .collision = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8},
};

hero.walk(heroConfig, input, walls);
```

The frame size is the size of a cell on the walker's sheet. With 16×16 art, this box covers the bottom
half of the frame, inset 3 pixels on each side: the character's feet. The character's head can overlap
a wall above it, and its feet stop at the wall.

## Solids

`walk` accepts anything that answers one question: does this rectangle of world pixels touch something
solid? There are two ways to provide it.

### A collision map

`TileCollision` reads a `retropp::IndexGrid`: a grid with one number per cell.

```cpp
const TopDownMovement::TileCollision walls{.map = &map, .cellSize = 8, .open = Cell::Open};
```

| Rule | |
|---|---|
| A cell whose value equals `open` | open |
| Any other cell | solid |
| Any pixel outside the map | solid |

The engine's `docs/guide/tilemaps.md` covers loading an `IndexGrid` from a PNG.

### Your own solids

Pass any callable that takes a `retropp::IntRect` and returns `bool`. This one makes a list of shut
doors solid:

```cpp
bool overlaps(retropp::IntRect a, retropp::IntRect b) {
    return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}

const auto doors = [&](retropp::IntRect box) {
    return std::any_of(shutDoors.begin(), shutDoors.end(),
                       [&](retropp::IntRect door) { return overlaps(box, door); });
};

hero.walk(heroConfig, input, doors);
```

An object with a `solidBox(retropp::IntRect)` member also works. A world type that has one can be passed
to `walk` directly.

### Combining solids

Combine sources with `||` in one callable:

```cpp
const auto solids = [&](retropp::IntRect box) { return walls.solidBox(box) || doors(box); };

hero.walk(heroConfig, input, solids);
```

The solids are passed on every call and read only during it, so each walker can have its own, and they
can change from tick to tick.

## Collision shapes

`WalkerConfig::collision` is a `CollisionShape`. It is written in the walker's own pixels, relative to
the walker's top-left, and moves with the walker. It converts implicitly from each of these:

```cpp
// A box.
.collision = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8},

// A circle, a capsule, or any other retropp::ShapePoints.
.collision = retropp::ShapePoints::circle({8, 12}, 4),
.collision = retropp::ShapePoints::capsule({4, 12}, {12, 12}, 4),

// A polygon.
.collision = retropp::ShapePoints{.points = {{3, 10}, {13, 10}, {11, 16}, {5, 16}}},

// The polygon through named anchors in the art.
.collision = TopDownMovement::CollisionShape::fromAnchors(kHeroAnchors, {"heel-l", "toe-l", "toe-r", "heel-r"}),
```

| Form | Pixels covered |
|---|---|
| `retropp::IntRect` | every pixel in `[x, x + width) × [y, y + height)` |
| `retropp::ShapePoints` | every pixel whose center `(x + 0.5, y + 0.5)` is inside the shape, by `ShapePoints::contains`. `radius` and `strokeWidth` apply. |
| `fromAnchors` | the polygon through the named anchors, in the order given |

A `ShapePoints` collision shape covers the same pixels the engine colors when it draws a region with
that shape. The engine's `docs/guide/draw-state.md` covers `ShapePoints`.

A box is the cheapest shape to test ([Cost](#cost)). Use it unless the character needs another outline.

## How a tick tests collision

`walker.walk(config, heading, solids)` does this:

1. Works out the step: -1, 0 or 1 on each axis.
2. On x, up to `config.speed` times: takes the position one pixel along and tests the collision shape
   there against `solids`. If the shape touches something solid, x stops. Otherwise the walker moves
   that pixel.
3. Does the same on y.

Consequences:

- A walker stops with its shape flush against a wall.
- A blocked axis leaves the other free, so a diagonal into a wall moves along the wall.
- Each pixel is tested from the walker's current position, and each tick starts fresh from there.
- A walker whose shape already overlaps something solid moves only when a one-pixel step puts the shape
  fully clear. Spawn walkers in open ground
  ([how-to.md](how-to.md#spawn-clear-of-walls)).

## `CollisionShape`

```cpp
class CollisionShape {
public:
    CollisionShape();
    CollisionShape(retropp::IntRect box);
    CollisionShape(const retropp::ShapePoints& shape);
    static CollisionShape fromAnchors(std::span<const retropp::Anchor> anchors,
                                      std::initializer_list<std::string_view> labels);

    bool empty() const noexcept;
    retropp::IntRect bounds() const noexcept;
    bool covers(retropp::Vec2i pixel) const noexcept;
    CollisionShape at(retropp::Vec2i offset) const;

    template <SolidQuery Q>
    bool hits(const Q& solids, retropp::Vec2i offset = {}) const;
};
```

A set of pixels. The pixels are worked out once, when the shape is constructed.

| Member | Returns |
|---|---|
| `CollisionShape()` | An empty shape. `hits` returns `false` for it. |
| `CollisionShape(box)` | Every pixel of `box`. A box of zero width or height gives an empty shape. |
| `CollisionShape(shape)` | Every pixel whose center is inside `shape`. |
| `fromAnchors(anchors, labels)` | The polygon through the anchors named in `labels`, in that order, looked up in `anchors`. |
| `empty()` | `true` if the shape is empty. |
| `bounds()` | The smallest rectangle containing every covered pixel. All zeros for an empty shape. |
| `covers(pixel)` | `true` if the shape covers `pixel`. |
| `at(offset)` | A copy of the shape moved by `offset`. |
| `hits(solids, offset)` | `true` if the shape, moved by `offset`, touches anything `solids` reports as solid. |

`hits` is the test `walk` uses. Call it yourself to check a position before putting a walker there:

```cpp
if (!heroConfig.collision.hits(walls, spawnPoint)) {
    hero.place(spawnPoint);
}
```

Exceptions:

| Call | Throws | When |
|---|---|---|
| `CollisionShape(shape)` | `std::invalid_argument` | `shape` is empty: zero points and zero curve segments |
| `CollisionShape(shape)` | `std::invalid_argument` | `shape.invert` is set. An inverted shape covers everything outside it. |
| `CollisionShape(shape)` | `std::invalid_argument` | `shape` carries a transform other than the identity. Move the shape's points instead. |
| `fromAnchors` | `std::invalid_argument` | fewer than three labels |
| `fromAnchors` | `std::out_of_range` | a label is missing from `anchors` |

## `TileCollision`

```cpp
struct TileCollision {
    const retropp::IndexGrid* map = nullptr;
    int cellSize = 8;
    CellValue open{};

    bool solidAt(retropp::Vec2i pixel) const noexcept;
    bool solidBox(retropp::IntRect box) const noexcept;
};
```

Solids read from a collision map.

| Field | Meaning | Default |
|---|---|---|
| `map` | A pointer to your collision map, which must outlive the `TileCollision`. | `nullptr` |
| `cellSize` | The width and height of one cell, in pixels. Must be greater than 0. | `8` |
| `open` | The cell value that can be walked on. | `0` |

| Member | Returns |
|---|---|
| `solidAt(pixel)` | `true` if `pixel` is in a solid cell or outside the map. |
| `solidBox(box)` | `true` if any pixel of `box` is in a solid cell or outside the map. `false` for a box of zero width or height. |

- The map's top-left cell is at world pixel (0, 0).
- With `map` left as `nullptr`, everything is solid.
- A `TileCollision` is three small fields. Constructing one per tick is fine.

## `CellValue`

```cpp
struct CellValue {
    std::uint16_t value = 0;

    constexpr CellValue() = default;
    constexpr CellValue(std::uint16_t number);
    template <class E> requires std::is_enum_v<E>
    constexpr CellValue(E cell);
};
```

The type of `TileCollision::open`. It converts implicitly from a number or from any enumerator, so you
can write `.open = Cell::Open` or `.open = 0`.

## `SolidQuery`

```cpp
template <class Q>
concept SolidQuery =
    requires(const Q& query, retropp::IntRect box) { { query.solidBox(box) } -> std::convertible_to<bool>; }
    || std::predicate<const Q&, retropp::IntRect>;
```

The requirement on the `solids` argument of `walk` and `hits`. A type satisfies it in either of two
ways:

- it has a `solidBox(retropp::IntRect)` member that returns something convertible to `bool`, or
- it can be called with a `retropp::IntRect` and returns `bool`.

`TileCollision` satisfies the first. A lambda satisfies the second. If a type satisfies both,
`solidBox` is used.

The rectangle is in world pixels. Return `true` if any pixel of it is solid.

## Limits

**A `TileCollision` map starts at world pixel (0, 0).** For a map placed elsewhere, subtract its origin
in a callable:

```cpp
const auto roomSolids = [&](retropp::IntRect box) {
    return room.solidBox({.x = box.x - origin.x, .y = box.y - origin.y, .width = box.width, .height = box.height});
};
```

**A `TileCollision` has one open value.** For a map with several walkable values, read the cells in a
callable:

```cpp
enum class Ground : std::uint16_t { Floor = 0, Water = 1, Wall = 2 };

// Solid for a swimmer: everything except floor and water, and everything outside the map.
const auto swimmerSolids = [&](retropp::IntRect box) {
    if (box.x < 0 || box.y < 0) {
        return true;
    }
    for (int cy = box.y / kCell; cy <= (box.y + box.height - 1) / kCell; ++cy) {
        for (int cx = box.x / kCell; cx <= (box.x + box.width - 1) / kCell; ++cx) {
            if (cx >= ground.width || cy >= ground.height) {
                return true;
            }
            const std::uint16_t cell = ground.at(cx, cy);
            if (cell != static_cast<std::uint16_t>(Ground::Floor) && cell != static_cast<std::uint16_t>(Ground::Water)) {
                return true;
            }
        }
    }
    return false;
};
```

**Walkers pass through each other.** To make them block each other, include the other walkers in the
solids
([how-to.md](how-to.md#stop-walkers-walking-through-each-other)).

**A walker on a route passes through solids** ([routes.md](routes.md#collision)).

## Cost

`walk` is allocation-free. Its cost is the number of times it asks `solids`.

| Collision shape | Queries per pixel tested |
|---|---|
| built from an `IntRect` | 1 |
| any other | 1 per row of pixels the shape covers |

A tick tests up to `speed` pixels on each axis the walker heads along. A walker with a box shape and
`speed` 1 asks at most 2 queries per tick. A walker with a circle 8 pixels tall and `speed` 2 asks at
most 32.

`TileCollision::solidBox` checks every cell the rectangle overlaps. A rectangle up to one cell in
size overlaps at most four.

Heap allocation:

| | Calls |
|---|---|
| Allocation-free | `walk`, `moveTo`, `face`, `place`, `hits`, `covers`, `frame` |
| Allocate | constructing a `CollisionShape`; `CollisionShape::at` and `Walker::collision`, which return a copy of the shape; copying a `WalkerConfig`, which copies the shape and the animations; `routeFrom` |

Construct shapes and configs once, when the game loads. In per-tick code, use `hits` with an offset
instead of `at`, and `config.collision.bounds()` instead of `walker.collision(config).bounds()`.
