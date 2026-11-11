# `TileLayer`

```cpp
#include "layers/tile_layer.h"

namespace layers {
[[nodiscard]] retropp::DrawLayer TileLayer(std::string_view key, std::int32_t z, Cells cells,
                                           int widthInTiles, int heightInTiles,
                                           LayerOptions options = {});
}
```

Puts a finite grid of tile cells on one tile layer. `cells` is row-major, `widthInTiles` across and
`heightInTiles` down — `widthInTiles × heightInTiles` cells — and each cell names its own atlas, tile and
palette.

## What the returned layer carries

| Field | Value |
|---|---|
| `key` | `key` |
| `z` | `z` |
| `size` | the active viewport, `retropp::Renderer::defaultViewport.size()`, read at the call |
| `content` | `retropp::TileContent{widthInTiles, heightInTiles, cells, TileWrap::Blank}` |
| everything else | copied from `options` ([layer-options.md](layer-options.md)) |

## Placing the grid

The layer covers the screen and the grid is placed by `options.scroll`: screen pixel `(x, y)` shows grid
pixel `(x + scroll.x, y + scroll.y)`. So:

| Scroll | The grid's top-left corner is at screen pixel |
|---|---|
| `{0, 0}` | (0, 0) |
| `{-8, -16}` | (8, 16) |
| `{camera.x, camera.y}` | (−camera.x, −camera.y) — the camera's world position is at the screen's top-left |

The grid is finite (`retropp::TileWrap::Blank`): outside it the layer shows nothing and the layers beneath
show through. A box, a panel or a HUD strip is a small grid placed by scroll, not a screen-sized grid with
empty cells around it.

## Examples

```cpp
// A world map larger than the screen, scrolled by the camera, under a 16 px HUD strip.
layers::TileLayer("world", kWorldZ, WorldTiles(state, assets), kMapWidth, kMapHeight,
                  {.scroll = {state.camera.x, state.camera.y - 16}});

// A 20×2 HUD strip across the top of the screen.
layers::TileLayer("hud-strip", kHudBackZ, HudStrip(assets), 20, 2);

// An 18×4 text box with its top-left corner at (8, 104).
layers::TileLayer("textbox", kTextboxZ, TextboxFrame(assets), 18, 4, {.scroll = {-8, -104}});
```

## Things to know

- **`cells` is taken by value** and moved into the kept table under `key`; it stays valid until the next
  `TileLayer` call with that key. One call per key per frame
  ([concepts.md](concepts.md#why-the-aggregates-keep-content)).
- **Size the vector to the grid.** `cells.size()` should equal `widthInTiles × heightInTiles`; the kit
  passes both through unchecked.
- **Upload cost.** The renderer hashes a tile layer's cells each frame and uploads them only when they
  changed, so returning the same cells every frame costs a hash, not an upload.
- **Always finite.** The kit's tile layers end at the grid's edge; a layer that repeats or clamps
  (`TileWrap::Repeat` / `Clamp`) is a `DrawLayer` built directly. Likewise
  `TileContent::contentChanged` — the declaration that skips the per-frame hash for a very large map — is
  left unset.
- **One map, many sheets.** Every cell names its own atlas and palette, so one layer mixes any number of
  sheets and palettes.

## See also

- Engine `docs/guide/draw-state.md` — `TileContent`, wrap modes, the per-cell fields.
- Engine `docs/guide/tilemaps.md` — assembling cells from a map PNG through a catalog.
- Engine `docs/guide/tiles-and-color.md` — atlases, palettes and recoloring.
