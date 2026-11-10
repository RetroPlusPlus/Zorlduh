# layers — guide

The guide to the layer kit, for anyone writing a game on the Polyrhythm engine in components. Every page
is written against the shipped headers in `include/layers/`.

**New here? Start with [getting-started.md](getting-started.md)** — a complete program on screen,
explained part by part. Then read [concepts.md](concepts.md) for the model, and [how-to.md](how-to.md)
for recipes. The aggregate pages are the full reference for each function.

## What the kit is

The engine draws a frame that is described whole, as data: a list of layers at depths, each holding tile
cells, sprites or a raster of pixels. The kit is the layer between that and a game's own code. It lets
every on-screen thing be a function that returns what it is, and it owns the one piece of bookkeeping
that model needs: keeping the content a layer's spans point at alive until the renderer has drawn it.

What the kit takes care of:

- the return types a component uses (`Sprites`, `Cells`, `Regions`, `Raster`, `Layers`);
- one aggregate per kind of layer content, which takes a key, a depth and the content, and returns a
  `retropp::DrawLayer`;
- the storage behind each returned layer's span, kept per layer key;
- every other layer field, passed through unchanged with the engine's own defaults.

What stays with the game: the state, the tick that advances it, the components that read it, the depth
and key table, and the art.

## How these pages are organized

One page per aggregate, plus the shared options, the model, keys and depths, and recipes. Each reference
page gives the function's signature, what it does, working examples, the things to know, and links to the
engine guide pages it builds on. Engine pages are named as `docs/guide/<page>.md` in the engine
repository.

## Index

### Start here

| Page | Covers |
|---|---|
| [getting-started.md](getting-started.md) | A complete program — tiles, a sprite, an image and a painted raster through the kit — built as `layers-example-getting-started`, explained part by part |
| [concepts.md](concepts.md) | Components, aggregates and screens; what each returns; why the aggregates keep content; where state lives |
| [how-to.md](how-to.md) | Recipes: a HUD strip, a camera shared by layers, clipping to a rectangle, fades and tints, depth sorting, a raster that uploads only when it changes, an image at a fitted size, an aggregate of your own |

### Reference

| Page | Covers | Header |
|---|---|---|
| [sprite-layer.md](sprite-layer.md) | `SpriteLayer` — sprite components gathered onto one layer | `sprite_layer.h` |
| [tile-layer.md](tile-layer.md) | `TileLayer` — a finite tile grid placed by scroll | `tile_layer.h` |
| [raster-layer.md](raster-layer.md) | `RasterLayer` and `Raster` — a picture painted on the CPU | `raster_layer.h`, `types.h` |
| [image-layer.md](image-layer.md) | `ImageLayer` — a truecolor image the renderer holds | `image_layer.h` |
| [layer-options.md](layer-options.md) | `LayerOptions` and `RasterLayerOptions`, field by field | `layer_options.h` |
| [keys-and-depth.md](keys-and-depth.md) | Choosing layer keys, sprite keys and depths; the depth table | — |

## Coverage

| Surface | Status | Page |
|---|---|---|
| Return types `Sprites`, `Cells`, `Regions`, `Layers`, `Raster` | available | concepts.md, raster-layer.md |
| `SpriteLayer` | available | sprite-layer.md |
| `TileLayer` (finite grid, `TileWrap::Blank`) | available | tile-layer.md |
| `RasterLayer` (painted pixels, `generation`, `fit`) | available | raster-layer.md |
| `ImageLayer` (an `ImageId`, `fit`) | available | image-layer.md |
| `LayerOptions`, `RasterLayerOptions` | available | layer-options.md |
| A raster layer over a hosted machine's picture (`Vm::video()`) | not provided — build the `DrawLayer` directly, as the engine's `draw-state.md` shows | — |
| A tile layer that repeats or clamps instead of ending | not provided — `TileLayer` grids are always finite | tile-layer.md |
| A tile layer's `contentChanged` declaration | not provided — the engine hashes the cells each frame | tile-layer.md |
