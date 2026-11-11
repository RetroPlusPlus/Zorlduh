# Drawing reference

The world on screen. The functions are in `include/WorldKit/world_layers.h`, the one WorldKit header
that builds draw types.

```cpp
#include "WorldKit/world_layers.h"   // AreaLayerKey, ActorsKey, TopZ, ViewStencil, WorldLayers, Actors, Fade
```

## Contents

- [`WorldLayers`](#worldlayers)
- [`Actors`](#actors)
- [`Fade`](#fade)
- [`TopZ`](#topz)
- [`AreaLayerKey`, `ActorsKey`](#arealayerkey-actorskey)
- [`ViewStencil`](#viewstencil)
- [How the layers stack](#how-the-layers-stack)

## `WorldLayers`

```cpp
LayerKit::Layers WorldLayers(const Camera& camera, const World& world);
```

One tile layer for each map layer of each area the camera shows: the current area, each area reached
through seams that is in view (as many seams away as the layout's `seamDepth` allows), and during a
scroll the next area. Each is built through `LayerKit::TileLayer` at its map layer's `z`, under the key
`AreaLayerKey(layout, area, name)`, with the layer's map assembled through its art
(`retropp::assembleTilemap`), the map's size in tiles, the scroll the camera gives that area, and the
regions `ViewStencil(view)` for the current layout's view, so the kit keeps each layer's cells until the
next frame and nothing of the world is drawn outside the view. The grid is finite (`TileWrap::Blank`),
so nothing is drawn outside an area either.

```cpp
frame.layers = WorldKit::WorldLayers(state.camera, world);
```

The layers come area by area in the order the camera shows them, and within an area in the order it
lists them; the engine draws them by depth, so the order of the list does not matter. Throws
`std::out_of_range` when a map holds a value its layer's art has no entry for.

## `Actors`

```cpp
retropp::DrawLayer Actors(const Camera& camera, const World& world, std::string_view slot,
                          std::initializer_list<LayerKit::Sprites> children,
                          LayerKit::LayerOptions options = {});
```

The sprites `children` on one sprite layer, through `LayerKit::SpriteLayer`, at the depth of the current
area's actor slot named `slot`: over the area's map layers below the slot and under those above it. The
layer is keyed `ActorsKey(slot)`, takes the camera's scroll, so a sprite placed in the current area's
pixels lands on its map, and is confined to the current layout's view like the map layers. The children
are a list of components, as `SpriteLayer` takes them: a sprite's own `z` orders it within the layer,
then the list's order.

```cpp
frame.layers.push_back(WorldKit::Actors(state.camera, world, "actors", {Player(state), Enemies(state)}));
```

The regions in `options` are kept, with the view's stencil added after them; the scroll is the
camera's, whatever `options` holds; every other option passes onto the layer. Throws
`std::out_of_range` when the current area has no slot by that name.

The depth is the current area's. During a scroll the thing being carried is still in the old area's
pixels, and so is the camera until the last tick, so the layer keeps the old area's depth and scroll
until the scroll ends; across a seam the thing and the camera change areas on the same tick, and the
layer takes the new area's depth then. Two areas joined by a seam usually give a slot the same depth,
so nothing changes on screen at the crossing.

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

## `TopZ`

```cpp
std::int32_t TopZ(const World& world);
```

The highest depth of any map layer or actor slot of any area of any layout. A layer drawn over the
whole world — an effect, the HUD — takes a depth above it:

```cpp
inline std::int32_t HudZ(const WorldKit::World& world) { return WorldKit::TopZ(world) + 1; }
```

It is a property of the world, not of a frame: the areas on screen change, the world's top does not,
so a depth above the world is chosen once. For a world with no layers it is the lowest `std::int32_t`.

## `AreaLayerKey`, `ActorsKey`

```cpp
std::string AreaLayerKey(int layout, int area, std::string_view layer);   // "area:<layout>:<area>:<layer>"
std::string ActorsKey(std::string_view slot);                             // "actors:<slot>"
```

The key of a map layer's tile layer, and of the sprite layer `Actors` draws at a slot. The same layer
keeps the same key from frame to frame, so the engine eases its scroll as the camera moves. An actor
slot's key names no area, so the layer keeps its key as the camera goes from area to area. A layer of
the game's own must not use these keys.

## `ViewStencil`

```cpp
LayerKit::Regions ViewStencil(const CameraKit::View& view);
```

The regions that keep a layer inside `view`: one `retropp::stencil` whose shape is the view's rectangle
on the screen, with everything outside it see-through. `WorldLayers` puts it on every map layer and
`Actors` on its sprite layer. A game drawing something else of its own in the world's pixels — a
neighbor's inhabitants, an effect over the ground — can put it on that layer too, so that layer stops
at the view's edge as the world does.

## How the layers stack

Depth is the engine's z axis: a layer with a higher `z` is drawn over one with a lower `z`. An area's
map layers and its actor slots are all on that axis, so one area can say:

| Depth | What | Kind |
|---|---|---|
| 0 | the ground | map layer |
| 5 | a path, under the player's feet | map layer |
| 10 | the player and the area's inhabitants | actor slot `"actors"` |
| 15 | a canopy or a roof, over their heads | map layer |

A cell that draws nothing — everywhere but the path on the path's layer, the gaps between the canopy's
leaves — is a slot of the sheet whose pixels are all transparent indices, so the layers below show
through it ([areas.md](areas.md#maplayer)).

Things that belong to an area other than the current one — in a neighbor across a seam, already in
view — go on a layer at that area's slot depth taking that area's scroll from `shown`
([how-to.md](how-to.md#put-things-in-an-area)).
