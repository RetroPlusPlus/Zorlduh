# `ImageLayer`

```cpp
#include "layers/image_layer.h"

namespace layers {
[[nodiscard]] retropp::DrawLayer ImageLayer(std::string_view key, std::int32_t z,
                                            retropp::ImageId image,
                                            RasterLayerOptions options = {});
}
```

Shows a truecolor image the renderer already holds — a title card, a painted backdrop, a portrait — on
one raster layer. The image is registered once, and the layer names it by its `retropp::ImageId` every
frame.

## What the returned layer carries

| Field | Value |
|---|---|
| `key` | `key` |
| `z` | `z` |
| `size` | the active viewport, `retropp::Renderer::defaultViewport.size()`, read at the call |
| `content` | `retropp::RasterContent{.fit = options.fit, .image = image}` |
| everything else | copied from `options` ([layer-options.md](layer-options.md)) |

## Registering the image

```cpp
// From a PNG, once at load. The path is a string literal.
const retropp::ImageId titleCard = renderer.loadImage("assets/title.png");

// Or from pixels in memory.
const retropp::ImageId badge = renderer.uploadImage(std::span<const retropp::Rgba8>(pixels), 16, 16);
```

The renderer owns the image's texture from then on, at the image's own size and depth (an 8-bit PNG at 8
bits per channel, a 16-bit PNG at 16). Keep the `ImageId` with the rest of the game's art handles.

## How it is drawn

- **At the image's own size**, top-left corner where the scroll places it; outside it the layers beneath
  show through. Its alpha is per pixel, so it composites by its own edges.
- **`options.fit`** shows it at another size, texel for texel with no filtering; an axis left at 0 keeps
  the image's own size on that axis.
- **The layer's alpha, blend, transform, effects and regions** apply to it as to any layer.
- **Its texture is bound, never uploaded,** on every frame — the renderer already holds it.
- **An id the renderer never returned** (including `retropp::ImageId{}`) draws nothing.

## Examples

```cpp
// A title card centred on a 160×144 screen, 144×112, fading in by the state's fade value.
layers::ImageLayer("title-card", kTitleZ, assets.titleCard,
                   {.scroll = {-8, -16}, .alpha = state.titleFade});

// A backdrop shown at exactly the play area's size, whatever size the PNG is.
layers::ImageLayer("backdrop", kBackdropZ, assets.backdrop,
                   {.scroll = {0, -16}, .fit = {.width = 160, .height = 128}});
```

## Things to know

- **`ImageLayer` keeps nothing.** A layer that names an image carries no span, so there is no storage and
  a second call with the same key invalidates nothing — though the engine still requires layer keys to be
  unique within a frame.
- **Indexed art stays indexed.** Tiles and sprites draw from indexed atlases through palettes; an image is
  full colour and is shown only by a raster layer.

## See also

- Engine `docs/guide/images-and-transparency.md` — "A truecolor image as a layer: `loadImage`".
- Engine `docs/guide/draw-state.md` — "An image the renderer holds".
- [raster-layer.md](raster-layer.md) — a picture painted on the CPU instead.
