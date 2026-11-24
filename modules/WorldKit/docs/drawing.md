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
scroll the next area. A [reserved layer](areas.md#reservedlayer) is yours to build, and `WorldLayers` skips it. Each
is built through `LayerKit::TileLayer` at the depth its map layer is drawn at
([How the layers stack](#how-the-layers-stack)), under the key
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

The sprites `children` on one sprite layer, through `LayerKit::SpriteLayer`, at the depth the current
area's actor slot named `slot` is drawn at: over the area's map layers below the slot and under those
above it. The
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

The depth is the current area's, as `ActorZ(world, camera.shown(world).front(), slot)` gives it. During
a scroll the thing being carried is still in the old area's pixels, and so is the camera until the last
tick, so the layer keeps the old area's slot and scroll until the scroll ends; across a seam the thing
and the camera change areas on the same tick, and the layer takes the new area's slot then. The
current area is first among the areas on screen, so its slot is drawn over every other area's slot at
the same declared depth.

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

The highest depth any layer of the world is drawn at, whatever is on screen. A layer drawn over the
whole world — an effect, the HUD — takes a depth above it:

```cpp
inline std::int32_t HudZ(const WorldKit::World& world) { return WorldKit::TopZ(world) + 1; }
```

It is worked out over every map layer, reserved layer and actor slot of every area, with N the number
of areas in the world:

| A layer declared at `z` | Drawn at most at |
|---|---|
| `z` of 0 or more | `z × N + N − 1`, with every area of the world on screen and it last |
| `z` below 0 | `z`, its area alone on screen |

So a world of 17 areas whose highest depth is 15 has a top of 271. It is a property of the world:
the areas on screen change from frame to frame, and the world's top stays the same, so a depth above
the world is chosen once. For an empty world it is the lowest `std::int32_t`.

## `AreaLayerKey`, `ActorsKey`

```cpp
std::string AreaLayerKey(int layout, int area, std::string_view layer);   // "area:<layout>:<area>:<layer>"
std::string ActorsKey(std::string_view slot);                             // "actors:<slot>"
```

The key of a map layer's tile layer, and of the sprite layer `Actors` draws at a slot. The same layer
keeps the same key from frame to frame, so the engine eases its scroll as the camera moves. An actor
slot's key names no area, so the layer keeps its key as the camera goes from area to area. A layer of
the game's own takes a key of its own. A [`Scene`](scene.md#drawing) keys its layer for each slot, across
the areas on screen, with [`SceneLayerKey`](scene.md#scenelayerkey), `"scene:<slot>"`.

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

An area alone on screen is drawn at the depths the game declared. When more than one area is on
screen, WorldKit draws every layer at a lower declared depth under every layer at a higher one, across
all of them, and gives two areas' layers at one declared depth an order of their own, the current
area's first; keys and scrolls stay as they are.

The areas on screen are the ones `camera.shown(world)` lists, the current area first. With S of them,
a layer declared at `d` in the area at position k is drawn at `d × S + k`. The depths above, with the
area alone and then with a second area of only a ground and an `"actors"` slot scrolling in:

| Layer | Declared | The area alone | The area (0) and the second (1) |
|---|---|---|---|
| the area's ground | 0 | 0 | 0 |
| the second's ground | 0 | — | 1 |
| the area's path | 5 | 5 | 10 |
| the area's `"actors"` | 10 | 10 | 20 |
| the second's `"actors"` | 10 | — | 21 |
| the area's canopy | 15 | 15 | 30 |

Every layer of a frame has a depth of its own, and the declared depths can be consecutive numbers. The depths
change when a neighbor comes into or leaves view and when the current area changes; the order of every
lower declared depth under every higher one stays, and so does every layer's key and scroll.

Things that belong to an area other than the current one — in a neighbor across a seam, already in
view — go on a layer at the depth `ActorZ(world, shown, slot)` gives for that area, taking that area's
scroll from `shown` ([how-to.md](how-to.md#put-things-in-an-area)). A [`Scene`](scene.md#drawing)
draws each area's slots on layers built the same way, at `ActorZ(world, shown, slot)`. Across a seam, two
actors at one slot in two areas are ordered by their areas, the current area's in front.
