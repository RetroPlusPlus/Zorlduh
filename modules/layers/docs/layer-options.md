# `LayerOptions` and `RasterLayerOptions`

```cpp
#include "layers/layer_options.h"

namespace layers {

struct LayerOptions {                       // SpriteLayer, TileLayer
    retropp::LayerScroll scroll{};
    float alpha = 1.0f;
    retropp::BlendMode blend = retropp::BlendMode::Normal;
    std::vector<retropp::ScreenSpaceEffect> effects{};
    std::vector<retropp::Region> regions{};
    retropp::Transform transform{};
    retropp::DisplacementEdge transformEdge = retropp::DisplacementEdge::Blank;
    std::optional<std::uint32_t> advancesEvery{};
};

struct RasterLayerOptions {                 // RasterLayer, ImageLayer
    // every LayerOptions field, in the same order with the same defaults, then:
    retropp::PixelSize fit{};
};

}
```

Everything a layer carries beyond the four things its aggregate owns (key, depth, size, content). The
aggregate copies every field onto the returned `retropp::DrawLayer` unchanged, without knowing what any of
them means. Every default is the engine's own `DrawLayer` default, so a layer given no options draws
exactly as a plain `DrawLayer` would.

Options are written with designated initializers, naming only what differs from the default:

```cpp
layers::SpriteLayer("actors", kActorsZ, {Actors(state, assets)},
                    {.scroll = camera, .regions = RoomClip(state)});
```

## The fields

| Field | Default | What it does | Eases between ticks |
|---|---|---|---|
| `scroll` | `{0, 0}` | Places the layer's content: screen pixel `(x, y)` shows content pixel `(x + scroll.x, y + scroll.y)` | yes |
| `alpha` | `1.0` | The layer's opacity, `[0, 1]`; multiplies every pixel's own alpha | yes |
| `blend` | `Normal` | How the layer combines with what is beneath it — `Normal` is alpha-over; `Add`, `Subtract`, `Multiply`, `Screen`, `Half`, `HalfSubtract` grade it | snaps |
| `effects` | none | Screen-space effects over the whole layer, applied in order | snaps |
| `regions` | none | Effects confined to shapes, in **viewport pixels** — the layer's scroll and transform do not move them | snaps |
| `transform` | identity | A geometric transform of the whole layer: scale, rotation, skew, perspective | yes |
| `transformEdge` | `Blank` | What fills the area a transform exposes: `Blank` shows the layers beneath, `Stretch` clamps | snaps |
| `advancesEvery` | unset | Ticks between one change of this layer's state and the next, so a world that moves every N ticks eases across N; unset takes the frame's `FrameDrawState::advancesEvery` (1 unless the game sets it) | — |
| `fit` (raster layers) | `{0, 0}` | The size the picture is shown at, in viewport pixels; `{0, 0}` is its own size, an axis at 0 keeps its own size on that axis; no filtering | snaps |

"Eases" is the engine's automatic interpolation: a layer that keeps its key from one tick to the next has
its scroll, alpha and transform eased between the two, and everything else takes the newest value.

## `RasterLayerOptions` repeats `LayerOptions`

`fit` belongs to the raster layers alone, and C++20 designated initializers cannot name the fields of a
base class, so `RasterLayerOptions` is its own struct: every `LayerOptions` field in the same order with
the same defaults, then `fit`. The two are kept in step by a test in the kit's suite that compares their
fields one for one; it stops compiling when a field is added to `LayerOptions` alone. A field added to one
is added to the other.

## Things to know

- **Regions do not scroll.** A clip or a shade placed with `regions` stays where it is on screen while the
  layer's content scrolls under it. To confine a scrolling layer to a part of the screen, give the region
  that part of the screen.
- **A region on one layer affects that layer only.** A clip that should crop two layers goes on both; a
  region on the whole frame (`FrameDrawState::regions`) affects everything beneath it.
- **Effect parameters do not ease.** An animated effect is recomputed each tick from the state and
  resubmitted.

## See also

- Engine `docs/guide/draw-state.md` — `DrawLayer`, screen-space effects, `Region`, `stencil()`,
  transforms, `RasterContent::fit`.
- Engine `docs/guide/blend-modes.md` — the blend operators.
- Engine `docs/guide/run-loop-and-timing.md` — interpolation and `advancesEvery`.
