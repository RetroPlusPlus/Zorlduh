# Concepts

The model the kit is built on, and the rules that follow from it.

## Contents

- [A frame is a value](#a-frame-is-a-value)
- [Components](#components)
- [Aggregates](#aggregates)
- [Screens](#screens)
- [Why the aggregates keep content](#why-the-aggregates-keep-content)
- [Where state lives](#where-state-lives)
- [Depth and identity](#depth-and-identity)
- [Showing and hiding](#showing-and-hiding)

## A frame is a value

The engine draws whatever `retropp::FrameDrawState` it is handed: a list of `retropp::DrawLayer`s, each a
key, a depth (`z`), a size, a scroll and one kind of content — tile cells, sprites, or a raster of
pixels. The frame is described whole every frame, and the renderer works out what changed and uploads
only that (engine: `docs/guide/draw-state.md`, `docs/guide/rendering.md`).

So the picture is a function of the game's state. The kit gives that function a shape: it is built out of
smaller functions, each named for the thing it shows, each returning that thing.

## Components

A **component** is a function that takes the state it shows as ordinary parameters and returns the
engine primitives it is. It is named for the thing — `Player(…)`, `Hearts(…)`, `WorldTiles(…)` — because
calling it is declaring that thing, not performing a step.

| Kind | Example | Returns |
|---|---|---|
| A leaf, or a group of leaves | a player, a row of hearts, a room's tiles, a chart | its primitives: `Sprites`, `Cells`, `Regions` or a `Raster` |
| An aggregate | `SpriteLayer`, `TileLayer`, `RasterLayer`, `ImageLayer` | one `retropp::DrawLayer` |
| A screen | the play screen, a title screen | its `Layers` |

```cpp
using Sprites = std::vector<retropp::Sprite>;
using Cells   = std::vector<retropp::TileCell>;
using Regions = std::vector<retropp::Region>;
using Layers  = std::vector<retropp::DrawLayer>;
struct Raster;  // a painted picture that owns its pixels — declared in raster-layer.md
```

A component is pure: the same parameters return the same primitives, and nothing is remembered between
calls. A parent component returns what its children returned — a row of hearts returns the sprites of
each heart — and a component that changes how something is shown takes the child's primitives and
returns them changed (a blend set on each sprite, a region added).

A **leaf never builds a `DrawLayer`.** Whether a thing has a layer of its own, and at what depth, is
decided where it is gathered, by an aggregate.

## Aggregates

An **aggregate** gathers content onto one layer. Each owns four things about the layer it returns — its
**key**, its **depth**, its **size** and its **content** — and copies everything else from its options:

```cpp
retropp::DrawLayer SpriteLayer(std::string_view key, std::int32_t z,
                               std::initializer_list<Sprites> children, LayerOptions options = {});
retropp::DrawLayer TileLayer(std::string_view key, std::int32_t z, Cells cells,
                             int widthInTiles, int heightInTiles, LayerOptions options = {});
retropp::DrawLayer RasterLayer(std::string_view key, std::int32_t z, Raster raster,
                               RasterLayerOptions options = {});
retropp::DrawLayer ImageLayer(std::string_view key, std::int32_t z, retropp::ImageId image,
                              RasterLayerOptions options = {});
```

- **Size** is the active viewport, read from `retropp::Renderer::defaultViewport` on every call. Content
  smaller than the screen is placed by the layer's scroll, and outside it the layers beneath show through.
- **Options** carry the scroll, alpha, blend, effects, regions, transform, transform edge and advance
  cadence (and, for raster layers, `fit`). Every default is the engine's own, so a layer given no options
  draws exactly as a plain `DrawLayer` would ([layer-options.md](layer-options.md)).

## Screens

A **screen** returns its layers, declared as a list. Which components appear is decided inside the list:

```cpp
layers::Layers PlayScreen(const GameState& state, const Assets& assets) {
    return {
        layers::TileLayer("world", kWorldZ, WorldTiles(state, assets), kMapWidth, kMapHeight,
                          {.scroll = cameraScroll(state)}),
        layers::SpriteLayer("actors", kActorsZ, {
            Player(state, assets),
            state.swordSwing ? Sword(state, assets) : layers::Sprites{},
        }, {.scroll = cameraScroll(state)}),
    };
}
```

The render callback assigns the screen's layers to the frame and submits it:

```cpp
loop.renderLoop([&] {
    frame.layers = PlayScreen(state, assets);
    renderer.renderFrame(frame);
});
```

## Why the aggregates keep content

A layer's content holds **spans**, not copies: `TileContent::cells`, `SpriteContent::sprites` and
`RasterContent::pixels` point at memory the renderer reads during `renderFrame`. A component returns its
primitives by value, and those values are gone by the time the frame is drawn — so something has to hold
them. That is the aggregates' one piece of bookkeeping:

- `SpriteLayer`, `TileLayer` and `RasterLayer` each keep what they were handed in a program-wide table,
  under the layer's key. The returned layer's span points into that table.
- What is kept for a key stays valid until **the next call with the same key** replaces it.
- `ImageLayer` keeps nothing: a layer that names an image the renderer holds has no span.

Three rules follow:

1. **One call per key per frame.** A second call with the same key replaces what the first kept, and the
   layer the first call returned can no longer be drawn. (The engine requires layer keys to be unique
   within a frame anyway.)
2. **Calls with different keys never disturb each other.**
3. **Keys are names, never counters.** Every key ever used keeps its last content until the program
   exits; a key built from a frame number or a running count grows the table without limit.

Components never see this storage, and no component takes an out-parameter to provide it.

## Where state lives

- **Everything that changes over time advances in the tick** — positions, timers, animation cursors,
  fades, a raster's generation. The engine runs the tick at a fixed rate and the render callback once per
  displayed frame, so anything advanced in render runs at the monitor's refresh rate instead (engine:
  `docs/guide/run-loop-and-timing.md`).
- **Components only read.** They take state as parameters and return primitives.
- **Logic has no draw types.** The code that decides what happens holds no `retropp::Sprite` or
  `DrawLayer`; the components translate its state into those.

## Depth and identity

- **Depth is which layer.** "In front of" is a higher `z`; within one sprite layer, a sprite's own
  `Sprite::z` orders it. No two layers in a frame share a `z`. A game keeps every depth in one table
  ([keys-and-depth.md](keys-and-depth.md)).
- **A key names the object.** The renderer matches each layer and sprite to last tick's by its key and
  eases what moved — a layer's scroll, alpha and transform; a sprite's position, alpha and transform —
  and snaps everything else. A new key appears where it is put, with no easing. So a key is chosen per
  object, for what should count as the same thing from one tick to the next.

## Showing and hiding

What is on screen is exactly what the screen returned this frame. To hide a thing, leave it out: the
component is not called, or returns an empty list, and a layer that is not wanted is left out of the
screen's list. What was beneath it is still drawn, because each frame is the list the screen returned and
nothing was ever drawn over it — the model has no hide call, visibility flag or clear to manage. Content
smaller than the screen works the same way: a tile grid or a raster ends, and the layers beneath show
through outside it.
