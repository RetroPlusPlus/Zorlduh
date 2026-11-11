# `RasterLayer` and `Raster`

```cpp
#include "LayerKit/raster_layer.h"
#include "LayerKit/types.h"

namespace LayerKit {

struct Raster {
    std::vector<std::uint8_t> pixels;  // row-major, width × height × bytesPerPixel(format) bytes
    int width = 0;
    int height = 0;
    retropp::RasterPixelFormat format = retropp::RasterPixelFormat::Rgba8888;
    std::uint64_t generation = 0;
};

[[nodiscard]] retropp::DrawLayer RasterLayer(std::string_view key, std::int32_t z, Raster raster,
                                             RasterLayerOptions options = {});
}
```

Shows a picture a component paints on the CPU — a chart, a waveform, a minimap, a software-drawn effect —
on one raster layer. The component returns a `Raster`, which owns its pixels, and `RasterLayer` keeps it.

## What the returned layer carries

| Field | Value |
|---|---|
| `key` | `key` |
| `z` | `z` |
| `size` | the active viewport, `retropp::Renderer::defaultViewport.size()`, read at the call |
| `content` | `retropp::RasterContent{pixels, width, height, format, fit, generation}`, `pixels` spanning the kept `Raster` and `fit` from `options.fit` |
| everything else | copied from `options` ([layer-options.md](layer-options.md)) |

## How it is drawn

- **Texel for texel**, at the raster's own `width × height`, with its top-left corner where the scroll
  places it (`{-8, -10}` puts it at (8, 10)). Outside it the layer shows nothing.
- **`options.fit`** shows it at another size: the picture fills exactly `fit`, each viewport pixel taking
  the texel under it, with no filtering. An axis left at 0 keeps the picture's own size on that axis.
- **The layer's alpha, blend, transform, effects and regions** apply to it as to any layer.
- **Too few bytes** — fewer than `width × height × bytesPerPixel(format)` — and the raster is not drawn.

## `generation`

The renderer keeps one texture per raster layer key and decides whether to upload by `generation`, never
by looking at the pixels:

| `generation` | Upload |
|---|---|
| a non-zero number this key has not shown yet | once |
| the same non-zero number as last time | none — the texture already holds it |
| `0` | every frame |

So give each new picture a new number, and keep the number while the picture is the same. The usual way
is a counter in the game's state that the tick increments whenever something the picture depends on
changes; the component copies it into the `Raster`.

## Examples

```cpp
// A 160×32 chart of recent samples, repainted when the samples change.
LayerKit::Raster Scope(const Samples& samples) {
    LayerKit::Raster raster{.width = 160, .height = 32, .generation = samples.version};
    raster.pixels.resize(160 * 32 * 4);
    plot(samples, raster.pixels);
    return raster;
}

// Shown 112 px down the screen, squeezed to half height.
LayerKit::RasterLayer("scope", kScopeZ, Scope(state.samples),
                    {.scroll = {0, -112}, .fit = {.width = 160, .height = 16}});
```

## Things to know

- **`raster` is taken by value** and moved into the kept table under `key`; it stays valid until the next
  `RasterLayer` call with that key. One call per key per frame
  ([concepts.md](concepts.md#why-the-aggregates-keep-content)).
- **Painting still happens every frame** when the component is called every frame — `generation` saves
  the upload, not the painting. A picture that is expensive to paint is painted in the tick, held in the
  state, and copied out by the component.
- **A stable key matters twice:** it keeps the kit's storage, and it is what the renderer's texture is
  kept under, so a changing key uploads every frame whatever the generation.
- **Format.** `RasterPixelFormat::Rgba8888` (red, green, blue, alpha, one byte each) is the engine's
  format; alpha is per pixel, so a raster can have transparent areas.
- **A hosted machine's picture** (`Vm::video()`), whose pixels the platform holds, is shown by a
  `DrawLayer` built directly; the engine's `draw-state.md` shows it.

## See also

- Engine `docs/guide/draw-state.md` — `RasterContent`, "A raster your program draws itself".
- [image-layer.md](image-layer.md) — a picture the renderer holds, uploaded once.
