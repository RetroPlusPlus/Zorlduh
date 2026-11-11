# Drawing reference

The world on screen. The functions are in `include/WorldKit/world_layers.h`, the one WorldKit header
that builds draw types.

```cpp
#include "WorldKit/world_layers.h"   // AreaLayerKey, ViewStencil, WorldLayers, Fade
```

## Contents

- [`WorldLayers`](#worldlayers)
- [`Fade`](#fade)
- [`AreaLayerKey`](#arealayerkey)
- [`ViewStencil`](#viewstencil)
- [Layers of things in the world](#layers-of-things-in-the-world)

## `WorldLayers`

```cpp
LayerKit::Layers WorldLayers(const Camera& camera, const World& world, std::int32_t z);
```

One tile layer at depth `z` for each area the camera shows: the current area, each area reached
through seams that is in view (as many seams away as the layout's `seamDepth` allows), and during a
scroll the next area. Each is built through
`LayerKit::TileLayer` under the key `AreaLayerKey(layout, area)`, with the area's map assembled through
its art (`retropp::assembleTilemap`), the map's size in tiles, the scroll the camera gives that area, and
the regions `ViewStencil(view)` for the current layout's view, so the kit keeps each layer's cells until
the next frame and nothing of the world is drawn outside the view. The grid is finite
(`TileWrap::Blank`), so nothing is drawn outside an area either.

```cpp
frame.layers = WorldKit::WorldLayers(state.camera, world, kWorldZ);
```

Throws `std::out_of_range` when a map holds a value its area's art has no entry for.

## `Fade`

```cpp
LayerKit::Regions Fade(const Camera& camera, const World& world);
```

The fade over the whole screen while one runs: one `retropp::Region` keyed `"world:fade"`, a
rectangle over the active viewport, with a `ColorFill` of the current layout's `fadeColor` at the
camera's fade opacity. Nothing while no fade runs. It goes on the frame, so it covers every layer:

```cpp
frame.regions = WorldKit::Fade(state.camera, world);
```

A game with other frame regions appends instead.

## `AreaLayerKey`

```cpp
std::string AreaLayerKey(int layout, int area);   // "area:<layout>:<area>"
```

The key of an area's layer. The same area keeps the same key from frame to frame, so the engine eases
its scroll as the camera moves; a layer of the game's own must not use these keys.

## `ViewStencil`

```cpp
LayerKit::Regions ViewStencil(const CameraKit::View& view);
```

The regions that keep a layer inside `view`: one `retropp::stencil` whose shape is the view's rectangle
on the screen, with everything outside it see-through. `WorldLayers` puts it on every area layer. A game
drawing something of its own in the world's pixels — a layer of actors, an effect over the ground — can
put it on that layer too, so that layer stops at the view's edge as the world does.

## Layers of things in the world

Things placed in the current area's pixels — the player, the area's inhabitants — go on the game's own
sprite layers, which take the camera's scroll:

```cpp
frame.layers.push_back(LayerKit::SpriteLayer("actors", kActorsZ, {Player(state)}, {.scroll = state.camera.scroll(world)}));
```

During a scroll the thing being carried is still in the old area's pixels, and so is the camera until
the last tick, so the same scroll keeps it on the right pixel throughout. Across a seam the thing and
the camera change pixels on the same tick, so the same holds. Things that belong to an area other than
the current one — in a neighbor across a seam, already in view — go on a layer taking that area's
scroll from `shown` ([how-to.md](how-to.md#put-things-in-an-area)).
