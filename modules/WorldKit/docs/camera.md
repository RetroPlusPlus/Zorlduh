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
    explicit Camera(int area) noexcept;

    int area() const noexcept;
    bool transitioning() const noexcept;
    AreaSolids solids(const Layout& layout) const noexcept;

    void place(const Layout& layout, retropp::IntRect box);
    Step follow(const Layout& layout, retropp::IntRect box);

    retropp::Vec2i position() const noexcept;
    retropp::LayerScroll scroll(const Layout& layout) const noexcept;
    std::vector<Shown> shown(const Layout& layout) const;
    float fade(const Layout& layout) const noexcept;
};
```

| Member | What it does |
|---|---|
| `Camera(area)` | A camera in `area`, looking at its top-left. |
| `area()` | The area the camera is in. During a scroll, still the area being left until the last tick; during a fade, the next area from the middle. |
| `transitioning()` | Whether a scroll or fade is running. A game moves nothing of its own while one is. |
| `solids(layout)` | What is solid in the current area ([`AreaSolids`](#areasolids)). |
| `place(layout, box)` | Looks at `box` at once by the layout's camera law, with no transition: for the start, and after a jump. A transition in progress is dropped. A box placed on a spot does not go through it. |
| `follow(layout, box)` | One tick ([`Step`](#step)). While nothing runs: looks at `box` by the law, and the box's center going out through an opening that leads somewhere starts its transition — past a wall within a gap's span, or into a spot from outside it. While a scroll runs: the camera pans, and the move is this tick's share of the carry, plus on the last tick the change into the next area's pixels. While a fade runs: the screen darkens, then at `fadeTicks` the move is the jump to the entrance and the area changes, then the screen lightens; the fade ends at `2 × fadeTicks`. |
| `position()` | The camera's position, in the current area's pixels (the area being left, during a scroll). |
| `scroll(layout)` | The scroll for a layer of things placed in the current area's pixels: the position less the view's origin. |
| `shown(layout)` | The areas on screen with each layer's scroll ([`Shown`](#shown)): the current area, and during a scroll the next one. |
| `fade(layout)` | How dark the screen is, 0 to 1: rising to 1 at the middle of a fade and back to 0 at its end; 0 otherwise. |

**A scroll in numbers.** With the defaults (32 ticks, a carry of 16) and a box that steps out through an
east door of a 160-pixel-wide area: the next area's left edge is 160 px right; the camera pans from where
it was to where the law puts it in the next area for the box's center carried 16 px in; after 16 ticks the
camera is halfway and the box has been carried 8 px; on tick 32 the move is the last share of the carry
less (160, 0), the camera is in the next area's pixels, and `entered` is the next area. A south door
whose entrance span starts 6 cells left of the exit span sets the next area 48 px right and 128 px down.

**A fade's landing.** Onto a spot entrance, the box is centered on the spot's cells. Through a wall
entrance, the box is one cell in from that wall and centered along the entrance's span.

## `Step`

```cpp
struct Step {
    retropp::Vec2i move{};          // how far to move the thing followed this tick
    std::optional<int> entered{};   // the area just entered, on the tick the camera's area changes
};
```

The game applies `move` to the thing's position every tick; it is zero whenever the thing moves on its
own. `entered` is the place to change music, spawn an area's inhabitants, or record where the player is.

## `Shown`

```cpp
struct Shown {
    int area = 0;
    retropp::LayerScroll scroll{};   // what a layer of this area's pixels takes
};
```

The current area's scroll is `camera.scroll(layout)`; during a scroll the next area's is that less the
offset the next area sits at. `WorldLayers` builds a layer from each.

## `AreaSolids`

```cpp
struct AreaSolids {
    const Layout* layout = nullptr;
    int area = 0;

    bool solidAt(retropp::Vec2i pixel) const noexcept;
    bool solidBox(retropp::IntRect box) const noexcept;
};
```

The solid query for one area, in that area's pixels:

- A cell is solid unless its collision value is the layout's `open`.
- Every cell off the map is solid, except the cells straight out through an edge opening that leads
  somewhere: those are open as far out as anything goes, so the thing followed can cross the edge. Past a
  wall with no opening, past an opening that leads nowhere, and past a corner, it is solid.
- `solidBox` is true when any pixel of the box is in a solid cell; an empty box is never solid. Pixels
  map to cells by `cellSize`, rounding toward negative infinity.

It has the `solidBox(retropp::IntRect)` member a movement module's solid query is, so it is handed to
such a module as it is. The layout must outlive it; it is cheap, so a game builds one per call.
