# Collision

How a walker collides: what is solid, what of the walker collides, and how the two meet on every tick.
The types are in `include/TopDownMovement/collision.h`.

```cpp
#include "TopDownMovement/collision.h"  // CollisionShape, TileCollision, CellValue, SolidQuery
```

## Contents

- [What is solid and what collides](#what-is-solid-and-what-collides)
- [How it ties together](#how-it-ties-together)
- [`CollisionShape`](#collisionshape)
- [`TileCollision`](#tilecollision)
- [Any solid query: `SolidQuery`](#any-solid-query-solidquery)
- [Limits](#limits)
- [See also](#see-also)

## What is solid and what collides

Collision has two halves, and a game supplies both.

**What is solid** is a *solid query*: anything that answers one question — does this rectangle of
world pixels touch anything solid? `walk` takes it as its third argument, fresh on every call, so the
walker never holds on to it.

The usual answer is a collision map: an `IndexGrid` with one value per cell, read by `TileCollision`.
Every cell whose value isn't the open one is solid, and so is everything off the map.

```cpp
enum class Cell : std::uint16_t { Open = 0, Wall = 1 };
const retropp::IndexGrid map = retropp::loadMapPng("assets/maps/room.png");
const TopDownMovement::TileCollision walls{.map = &map, .cellSize = 8, .open = Cell::Open};

walker.walk(config, move, walls);
```

A game that keeps no map can answer from anything else. A table of solid cells:

```cpp
const std::set<std::pair<int, int>> solidCells{{5, 3}, {6, 3}, {6, 4}};

const auto table = [&](retropp::IntRect box) {
    for (int cy = box.y / 8; cy <= (box.y + box.height - 1) / 8; ++cy) {
        for (int cx = box.x / 8; cx <= (box.x + box.width - 1) / 8; ++cx) {
            if (solidCells.contains({cx, cy})) return true;
        }
    }
    return false;
};

walker.walk(config, move, table);
```

A table of passable cells is the same with the test turned round: a box is solid when any cell it
covers is *not* in the table, `!passableCells.contains({cx, cy})`. (Integer division rounds toward
zero; a table asked about negative pixels floors instead, as `TileCollision` does.)

Queries combine with `||`. A map plus the doors that are shut and the blocks that have been pushed:

```cpp
const auto solids = [&](retropp::IntRect box) {
    return walls.solidBox(box) || doors.solidBox(box) || blockAt(box);
};
```

**What collides** is the walker's collision shape, `WalkerConfig::collision`, in the walker's own pixels
relative to its top-left. The usual one is a box over the bottom of the art, so a character's head can
overlap a wall above it while its feet cannot:

```cpp
const TopDownMovement::WalkerConfig hero{
    .collision = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8},  // in a 16×16 frame
    ...
};
```

It can also be a circle, a capsule, any polygon, or the polygon through named points in the art
([`CollisionShape`](#collisionshape)).

## How it ties together

On each tick:

1. **The game calls `walker.walk(config, directions, solids)`.** The config carries the collision shape;
   `solids` is the query.
2. **`walk` works out the step.** Right minus left gives the x direction, down minus up the y
   direction.
3. **For each pixel along x, up to `config.speed`,** it takes the position one pixel on and asks
   `config.collision.hits(solids, next)`: the shape moved to `next`, tested against the query.
4. **`hits` asks the query.** A shape built from a box is one call, `solidBox` with the box moved to
   `next`; any other shape is one call per row of pixels it covers.
5. **`TileCollision::solidBox` turns the box into cells** (pixel ÷ `cellSize`, rounding down) and checks
   each cell it spans: off the map, or holding any value but `open`, means solid.
6. **Back in `walk`:** solid means that pixel is blocked and x stops there, flush with the wall. Clear
   means the walker takes the pixel and tries the next.
7. **Then y does the same.** An axis that is blocked does not stop the other, and that is the slide
   along a wall.

A walker keeps only its position. Every pixel it moves is tested before it is taken, from wherever it
stands, and nothing about touching a wall is kept from one tick to the next.

## `CollisionShape`

```cpp
class CollisionShape {
public:
    CollisionShape();                                    // covers nothing
    CollisionShape(retropp::IntRect box);                // every pixel of the box
    CollisionShape(const retropp::ShapePoints& shape);   // every pixel whose center the shape contains
    static CollisionShape fromAnchors(std::span<const retropp::Anchor> anchors,
                                      std::initializer_list<std::string_view> labels);

    bool empty() const noexcept;
    retropp::IntRect bounds() const noexcept;            // the smallest rectangle holding every covered pixel
    bool covers(retropp::Vec2i pixel) const noexcept;
    CollisionShape at(retropp::Vec2i offset) const;      // the same shape moved
    template <SolidQuery Q> bool hits(const Q& solids, retropp::Vec2i offset = {}) const;
};
```

The constructors convert implicitly, so `.collision` takes any of them:

```cpp
.collision = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8},
.collision = retropp::ShapePoints::capsule({4, 12}, {12, 12}, 4),
.collision = retropp::ShapePoints{.points = {{3, 10}, {13, 10}, {11, 16}, {5, 16}}},
.collision = TopDownMovement::CollisionShape::fromAnchors(kHeroAnchors, {"heel-l", "toe-l", "toe-r", "heel-r"}),
```

- **A box** covers every pixel in `[x, x + width) × [y, y + height)`. An empty box covers nothing.
- **A `ShapePoints`** covers every pixel whose center, `(x + 0.5, y + 0.5)`, is inside it by
  `ShapePoints::contains`: the same test a region drawn with that shape uses, so a collision shape
  covers what it would color on screen. `radius` and `strokeWidth` apply, and a curve boundary works
  as well as points. A rectangle built as a `ShapePoints` covers exactly what the same box covers.
- **`fromAnchors`** is the polygon through the named anchors, in the order given. The anchors are in
  the art's pixels, which are the walker's own.

The coverage is worked out once, when the shape is built. Moving a shape and testing it costs one query
for a box and one per covered row for anything else, however complex the outline; a rectangle built as
a `ShapePoints` is tested row by row.

**Throws.** Building from a `ShapePoints` throws `std::invalid_argument` for a shape with no points
and no curve, for an inverted shape (its outside is infinite, so it would be solid everywhere), and for
a shape with a non-identity `transform` (move its points instead). `fromAnchors` throws
`std::out_of_range` for a label not in the table and `std::invalid_argument` for fewer than three
labels.

## `TileCollision`

```cpp
struct TileCollision {
    const retropp::IndexGrid* map = nullptr;   // the collision map
    int cellSize = 8;                          // the side of one cell, in pixels
    CellValue open{};                          // the one value that is not solid

    bool solidAt(retropp::Vec2i pixel) const noexcept;
    bool solidBox(retropp::IntRect box) const noexcept;
};
```

- A cell is solid unless its value is `open`. Every pixel off the map is solid, and with no map
  everything is.
- Pixels map to cells by `cellSize`, rounding toward negative infinity, so a pixel left of or above the
  map lands on a cell off it.
- `solidBox` is true when any pixel of the box is in a solid cell; an empty box is never solid.
- `open` is a `CellValue`, which takes the map's own enumerator (`Cell::Open`) or a number.
- `TileCollision` points at the map; the map must outlive it. It is cheap to build, so a game can build
  one per call (a `World` with a `solids()` function that returns one over its own map, for example).

## Any solid query: `SolidQuery`

```cpp
template <class Q>
concept SolidQuery = requires(const Q& q, retropp::IntRect box) { { q.solidBox(box) } -> std::convertible_to<bool>; }
                  || std::predicate<const Q&, retropp::IntRect>;
```

Anything with a `solidBox(retropp::IntRect)` member, or anything callable with a `retropp::IntRect`
returning `bool`. A game's world type that has `solidBox` is passed to `walk` as it is.

## Limits

- **A `TileCollision` map starts at world pixel (0, 0).** A map placed elsewhere in a larger world is
  read through a query that subtracts its origin first:
  `[&](retropp::IntRect b) { return room.solidBox({b.x - origin.x, b.y - origin.y, b.width, b.height}); }`.
- **A `TileCollision` has one open value.** A map whose values mean several kinds of ground (floor,
  water, ledge) is read through a query that decides per value.

## See also

- [concepts.md](concepts.md) — a tick of walking, and the facing law that runs after it.
- [how-to.md](how-to.md) — switching maps, combining solids, different rules per walker.
- The engine's `docs/guide/tilemaps.md` — map PNGs and `IndexGrid`; `docs/guide/draw-state.md` —
  `ShapePoints`.
