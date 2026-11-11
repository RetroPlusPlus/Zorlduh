# Drawing reference

The world on screen. The functions are in `include/WorldKit/world_layers.h`, the one WorldKit header
that builds draw types.

```cpp
#include "WorldKit/world_layers.h"   // AreaLayerKey, WorldLayers, Fade
```

## Contents

- [`WorldLayers`](#worldlayers)
- [`Fade`](#fade)
- [`AreaLayerKey`](#arealayerkey)
- [Layers of things in the world](#layers-of-things-in-the-world)

## `WorldLayers`

```cpp
LayerKit::Layers WorldLayers(const Camera& camera, const Layout& layout, std::int32_t z);
```

One tile layer at depth `z` for each area the camera shows: the current area, and during a scroll the
next one too. Each is built through `LayerKit::TileLayer` under the key `AreaLayerKey(area)`, with the
area's map assembled through its art (`retropp::assembleTilemap`), the map's size in tiles, and the
scroll the camera gives that area, so the kit keeps each layer's cells until the next frame. The grid is
finite (`TileWrap::Blank`), so nothing is drawn outside an area.

```cpp
frame.layers = WorldKit::WorldLayers(state.camera, layout, kWorldZ);
```

Throws `std::out_of_range` when a map holds a value its area's art has no entry for.

## `Fade`

```cpp
LayerKit::Regions Fade(const Camera& camera, const Layout& layout);
```

The fade over the whole screen while one runs: one `retropp::Region` keyed `"world:fade"`, a
rectangle over the active viewport, with a `ColorFill` of `layout.fadeColor` at the camera's fade
opacity. Nothing while no fade runs. It goes on the frame, so it covers every layer:

```cpp
frame.regions = WorldKit::Fade(state.camera, layout);
```

A game with other frame regions appends instead.

## `AreaLayerKey`

```cpp
std::string AreaLayerKey(int area);   // "area:<index>"
```

The key of an area's layer. The same area keeps the same key from frame to frame, so the engine eases
its scroll as the camera moves; a layer of the game's own must not use these keys.

## Layers of things in the world

Things placed in the current area's pixels — the player, the area's inhabitants — go on the game's own
sprite layers, which take the camera's scroll:

```cpp
frame.layers.push_back(LayerKit::SpriteLayer("actors", kActorsZ, {Player(state)}, {.scroll = state.camera.scroll(layout)}));
```

During a scroll the thing being carried is still in the old area's pixels, and so is the camera until
the last tick, so the same scroll keeps it on the right pixel throughout.
