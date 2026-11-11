# Camera reference

The camera, what it hands back each tick, and the solid query. The types are in
`include/WorldKit/camera.h` and `include/WorldKit/solids.h`.

```cpp
#include "WorldKit/camera.h"   // Step, Shown, Camera
#include "WorldKit/solids.h"   // AreaSolids
```

## Contents

- [`Camera`](#camera)
- [`Step`](#step)
- [`Shown`](#shown)
- [`AreaSolids`](#areasolids)

## `Camera`

```cpp
class Camera {
public:
    Camera(int layout, int area) noexcept;

    int layout() const noexcept;
    int area() const noexcept;
    bool transitioning() const noexcept;
    AreaSolids solids(const World& world) const noexcept;

    void place(const World& world, retropp::IntRect box);
    Step follow(const World& world, retropp::IntRect box);

    retropp::Vec2i position() const noexcept;
    retropp::LayerScroll scroll(const World& world) const noexcept;
    std::vector<Shown> shown(const World& world) const;
    float fade(const World& world) const noexcept;
};
```

| Member | What it does |
|---|---|
| `Camera(layout, area)` | A camera in area `area` of layout `layout`, looking at its top-left. |
| `layout()`, `area()` | The place the camera is in. During a scroll, still the area being left until the last tick; during a fade, the next place from the middle; across a seam, the next place from the tick the box's center crosses the wall. |
| `transitioning()` | Whether a scroll or fade is running. A game moves nothing of its own while one is. A seam runs nothing. |
| `solids(world)` | What is solid in the current area ([`AreaSolids`](#areasolids)). |
| `place(world, box)` | Looks at `box` at once by the layout's camera law, with no transition: for the start, and after a jump. A transition in progress is dropped. A box placed on a spot does not go through it. |
| `follow(world, box)` | One tick ([`Step`](#step)). While nothing runs: follows `box` as below, and the box's center going out through an opening that leads somewhere starts its transition — past a wall within a gap's span, or into a spot from outside it — or, through a seam, makes the next place current at once with the move the change into its pixels. While a scroll runs: the camera slides, and the move is this tick's share of the carry, plus on the last tick the change into the next area's pixels. While a fade runs: the screen darkens, then at the layout's `fadeTicks` the move is the jump to the entrance and the place changes, then the screen lightens over the next layout's `fadeTicks`. |
| `position()` | The camera's position, in the current area's pixels (the area being left, during a scroll). |
| `scroll(world)` | The scroll for a layer of things placed in the current area's pixels: the position less the view's origin. |
| `shown(world)` | The areas on screen with each layer's scroll ([`Shown`](#shown)): the current area; each area joined to it seamlessly whose attached rectangle is in view, once each; and during a scroll the next area. |
| `fade(world)` | How dark the screen is, 0 to 1: rising to 1 at the middle of a fade and back to 0 at its end; 0 otherwise. |

**How it follows.** The law says where the camera should be for the box, over the current area's
rectangle reaching across each seam into the area beyond. On each axis along which that rectangle is
at least as big as the view, the camera moves only when the box moves, with the box's own step on that
axis, toward the law's place and no further, and never against the box's step; while the box stands,
nothing moves. In an area on its own that is `CameraKit::follow` exactly. After a seam or a scroll has
left the camera away from the law's place, the camera stays there and comes to the place only as the
box walks toward it. On an axis along which the rectangle is smaller than the view, the camera holds
still until the next transition. `place`, and the middle of a fade, go straight to the law's place.

**A scroll in numbers.** With the defaults (32 ticks, a carry of 16) and a box that steps out through an
east door of a 160-pixel-wide area: the next area's left edge is 160 px right; the camera slides 160 px
right over 32 ticks and not at all up or down; after 16 ticks it is halfway and the box has been carried
8 px; on tick 32 the move is the last share of the carry less (160, 0), the camera is in the next area's
pixels, and `entered` is the next place. A south door whose entrance span starts 6 cells left of the
exit span sets the next area 48 px right and 128 px down; the camera slides 128 px down and stays where
it was across, so the next area's left edge is 48 px in from the left of the screen — unless the layout's
`scrollAligns` and the next area is at least a view wide, when the camera also moves the 48 px across.
An area narrower than the view lands against the door's side of the screen either way.

**A fade's landing.** Onto a spot entrance, the box is centered on the spot's cells. Through a wall
entrance, the box is one cell in from that wall and centered along the entrance's span.

**A seam in numbers.** A 20-cell-wide hall whose whole north wall is open to a 30-cell-wide treasury,
lined up centered: the treasury's top-left is (−40, −128) in the hall's pixels. Walking up the hall, the
camera follows the box past the wall into the treasury. When the box's center passes y < 0, the move is
(40, 128), the camera's position changes by the same, `entered` is the treasury, and every layer keeps
its scroll. The hall's columns fit the view, so if the box crossed off center the camera is now 40 px
from the treasury's own place for it; it goes there only as the box walks that way.

## `Step`

```cpp
struct Step {
    retropp::Vec2i move{};            // how far to move the thing followed this tick
    std::optional<Place> entered{};   // the place just entered, on the tick the camera's area changes
};
```

The game applies `move` to the thing's position every tick; it is zero whenever the thing moves on its
own. `entered` is the place to change music, spawn an area's inhabitants, or record where the player is.

## `Shown`

```cpp
struct Shown {
    int layout = 0;
    int area = 0;
    retropp::LayerScroll scroll{};   // what a layer of this area's pixels takes
};
```

The current area's scroll is `camera.scroll(world)`; another area's is that less the offset the area
sits at. `WorldLayers` builds a layer from each.

## `AreaSolids`

```cpp
struct AreaSolids {
    const World* world = nullptr;
    int layout = 0;
    int area = 0;

    bool solidAt(retropp::Vec2i pixel) const noexcept;
    bool solidBox(retropp::IntRect box) const noexcept;
};
```

The solid query for one area, in that area's pixels:

- A cell is solid unless its collision value is the layout's `open`.
- Every cell off the map is solid, except the cells straight out through an edge opening that leads
  somewhere. Past a scroll or a fade those are open as far out as anything goes, so the thing followed
  can cross the edge. Past a seam they are the next area's cells, read from its collision map where it
  is attached, and solid beyond its edge. Past a wall with no opening, past an opening that leads
  nowhere, and past a corner, it is solid.
- `solidBox` is true when any pixel of the box is in a solid cell; an empty box is never solid. Pixels
  map to cells by `cellSize`, rounding toward negative infinity.

It has the `solidBox(retropp::IntRect)` member a movement module's solid query is, so it is handed to
such a module as it is. The world must outlive it; it is cheap, so a game builds one per call.
