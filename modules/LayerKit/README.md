# LayerKit — the layer kit

`LayerKit` lets a game on the [Polyrhythm](https://github.com/RetroPlusPlus/Polyrhythm) engine write
everything on screen as a **component**: a function, named for the thing it is, that takes the state it
shows and returns the engine primitives it is. The kit supplies the types components return, one
**aggregate** per kind of layer content that gathers components onto a layer, and the options a layer
carries.

```cpp
LayerKit::Layers PlayScreen(const GameState& state, const Assets& assets) {
    const retropp::LayerScroll camera{state.camera.x, state.camera.y - kHudHeight};
    return {
        LayerKit::TileLayer("world", kWorldZ, WorldTiles(state, assets), kMapWidth, kMapHeight,
                          {.scroll = camera}),
        LayerKit::SpriteLayer("actors", kActorsZ, {Player(state, assets), Enemies(state, assets)},
                            {.scroll = camera}),
        LayerKit::SpriteLayer("hud", kHudZ, {Hearts(state.player.halfHearts, assets)}),
    };
}
```

| Aggregate | Gathers | Page |
|---|---|---|
| `SpriteLayer` | sprite components, declared as a list | [docs/sprite-layer.md](docs/sprite-layer.md) |
| `TileLayer` | a finite grid of tile cells, placed by scroll | [docs/tile-layer.md](docs/tile-layer.md) |
| `RasterLayer` | a picture painted on the CPU | [docs/raster-layer.md](docs/raster-layer.md) |
| `ImageLayer` | a truecolor image the renderer holds | [docs/image-layer.md](docs/image-layer.md) |

The kit is header-only, depends on the engine alone (`retropp::engine`), includes no header from the
game it sits in, and needs C++20. It is meant to leave this repository as a standalone module.

## Using it

```cmake
add_subdirectory(modules/LayerKit)
target_link_libraries(my-game PRIVATE LayerKit::LayerKit)
```

The directory's `CMakeLists.txt` defines the INTERFACE target `LayerKit` (alias `LayerKit::LayerKit`) and
expects the engine as the target `retropp::engine`. `LAYERKIT_BUILD_EXAMPLES` (on by default when the
directory is the top-level project) builds `LayerKit-example-getting-started`, the program
[docs/getting-started.md](docs/getting-started.md) walks through.

## Documentation

**Start with [docs/getting-started.md](docs/getting-started.md)**, then [docs/concepts.md](docs/concepts.md)
for the model. The full index is [docs/README.md](docs/README.md).

## Layout

```
modules/LayerKit/
  CMakeLists.txt                    the LayerKit::LayerKit target and the example option
  include/LayerKit/
    types.h                         Sprites, Cells, Regions, Layers, Raster
    layer_options.h                 LayerOptions, RasterLayerOptions
    sprite_layer.h                  SpriteLayer
    tile_layer.h                    TileLayer
    raster_layer.h                  RasterLayer
    image_layer.h                   ImageLayer
  examples/getting_started.cpp      the getting-started program
  docs/                             the guide
```
