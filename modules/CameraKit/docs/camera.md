# Camera reference

`View`, `Camera` and the `follow` law.

```cpp
#include "CameraKit/camera.h"   // View, Camera
#include "CameraKit/laws.h"     // follow
```

## Contents

- [`View`](#view)
- [`Camera`](#camera)
- [`follow`](#follow)

## `View`

```cpp
struct View {
    retropp::PixelSize size{};    // the screen the camera shows
    retropp::Vec2i origin{};      // where it sits on a layer
};
```

The whole screen is `{.size = screen}`. A play area under a 16 px HUD strip is
`{.size = {160, 128}, .origin = {0, 16}}`.

## `Camera`

```cpp
class Camera {
public:
    Camera();                                                   // at (0, 0)
    explicit Camera(retropp::Vec2i position) noexcept;

    void place(retropp::Vec2i position) noexcept;
    void pan(retropp::Vec2i to, std::uint32_t ticks);
    void advance() noexcept;

    retropp::Vec2i position() const noexcept;
    bool panning() const noexcept;
    retropp::LayerScroll scroll(const View& view) const noexcept;
};
```

| Member | What it does |
|---|---|
| `Camera(position)` | A camera showing world pixel `position` at the view's top-left. |
| `place(position)` | Be at `position` now. A pan in progress is dropped. |
| `pan(to, ticks)` | Move from here to `to` in a straight line over `ticks` ticks, arriving on the last. With no ticks, the camera is placed at `to` at once. The path is the engine's `Tween<Vec2>` with linear easing, against the cadence in `TweenPlayer<Vec2>::defaultTiming` when the pan starts. |
| `advance()` | One tick of a pan: the position becomes the path's value at that tick, rounded to the nearest pixel (halves away from zero). Nothing happens when the camera is not panning. |
| `position()` | The world pixel shown at the view's top-left. |
| `panning()` | Whether a pan is in progress. False again on the tick the pan arrives. |
| `scroll(view)` | `{position.x − view.origin.x, position.y − view.origin.y}`: what a layer of world pixels takes to show the position at `view.origin`. |

A pan over 32 ticks from (0, 0) to (160, 0) is at (80, 0) after 16 ticks and at (160, 0) after 32; one
over 4 ticks from (0, 0) to (10, 0) passes (3, 0), (5, 0) and (8, 0).

## `follow`

```cpp
constexpr retropp::Vec2i follow(retropp::IntRect bounds, retropp::PixelSize view, retropp::Vec2i focus) noexcept;
```

Where the camera should be to look at `focus` through a view of `view` pixels while showing nothing
outside `bounds`. Each axis is decided on its own:

| `bounds` on the axis | the view's start on the axis |
|---|---|
| at least as long as the view | `focus − view / 2`, kept between the bounds' start and its end less the view: centered on the focus, stopped at the edges |
| shorter than the view | the bounds' start less half the difference: the bounds centered in the view, holding still |

| `bounds` | `view` | `focus` | result |
|---|---|---|---|
| `{0, 0, 320, 128}` | 160×128 | (160, 64) | (80, 0) |
| `{0, 0, 320, 128}` | 160×128 | (10, 64) | (0, 0) |
| `{0, 0, 320, 128}` | 160×128 | (310, 64) | (160, 0) |
| `{0, 0, 160, 128}` | 160×128 | anywhere | (0, 0) |
| `{0, 0, 96, 80}` | 160×128 | anywhere | (−32, −24) |
| `{160, 0, 320, 128}` | 160×128 | (320, 64) | (240, 0) |

`bounds` need not start at the origin: a law is asked about rectangles wherever they lie.
