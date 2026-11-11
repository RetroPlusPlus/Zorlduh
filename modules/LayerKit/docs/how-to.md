# How-to

Recipes for common screen tasks. Each is a component or a screen fragment; the state and asset names
(`state`, `assets`, `kHudZ`, …) stand for the game's own.

## Contents

- [Put a HUD strip across the top](#put-a-hud-strip-across-the-top)
- [Share one camera between layers](#share-one-camera-between-layers)
- [Clip layers to a rectangle](#clip-layers-to-a-rectangle)
- [Fade a layer](#fade-a-layer)
- [Tint a layer](#tint-a-layer)
- [Draw nearer actors in front](#draw-nearer-actors-in-front)
- [Show a layer only sometimes](#show-a-layer-only-sometimes)
- [Paint a raster that uploads only when it changes](#paint-a-raster-that-uploads-only-when-it-changes)
- [Show a full-color image at a fixed size](#show-a-full-color-image-at-a-fixed-size)
- [Write an aggregate of your own](#write-an-aggregate-of-your-own)

## Put a HUD strip across the top

A HUD is a small tile grid at scroll `{0, 0}` with a sprite layer over it, both above the world:

```cpp
LayerKit::Cells HudStrip(const Assets& assets);                 // 20×2 cells: the strip's background
LayerKit::Sprites Hearts(int halfHearts, const Assets& assets);  // placed in screen pixels

LayerKit::TileLayer("hud-strip", kHudBackZ, HudStrip(assets), 20, 2),
LayerKit::SpriteLayer("hud", kHudZ, {Hearts(state.player.halfHearts, assets)}),
```

The strip's grid ends after 2 rows, so everything below it shows the layers beneath.

## Share one camera between layers

The world and everything in it scroll together. Compute the scroll once from the state and pass it to
each layer that follows the camera; the HUD takes none:

```cpp
LayerKit::Layers PlayScreen(const GameState& state, const Assets& assets) {
    const retropp::LayerScroll camera{state.camera.x, state.camera.y - kHudHeight};
    return {
        LayerKit::TileLayer("world", kWorldZ, WorldTiles(state, assets), kMapWidth, kMapHeight,
                          {.scroll = camera}),
        LayerKit::SpriteLayer("actors", kActorsZ, {Player(state, assets), Actors(state, assets)},
                            {.scroll = camera}),
        LayerKit::SpriteLayer("hud", kHudZ, {Hearts(state.player.halfHearts, assets)}),
    };
}
```

Subtracting the HUD height puts the camera's world position at the top-left of the area below the HUD.
The camera advances in the tick; the engine eases each layer's scroll between ticks.

## Clip layers to a rectangle

To show a layer only inside part of the screen, give it a `TransparentOutside` stencil over that part. A
component returns the regions:

```cpp
LayerKit::Regions RoomClip(const GameState& state) {
    const ScreenRect r = roomOnScreen(state);   // the room's rectangle in screen pixels
    return retropp::stencil(retropp::ShapePoints::rectangle({r.x, r.y}, r.width, r.height),
                            retropp::StencilMode::TransparentOutside);
}

LayerKit::TileLayer("world", kWorldZ, WorldTiles(state, assets), kMapWidth, kMapHeight,
                  {.scroll = camera, .regions = RoomClip(state)}),
LayerKit::SpriteLayer("actors", kActorsZ, {Actors(state, assets)},
                    {.scroll = camera, .regions = RoomClip(state)}),
```

Regions are in screen pixels and do not scroll with the layer, and a layer's regions affect that layer
only — so a clip that must crop both the world and the actors goes on both. A component that has nothing
to clip returns an empty `Regions`.

## Fade a layer

A fade is the layer's `alpha`, advanced in the tick and read by the screen. The engine eases `alpha`
between ticks:

```cpp
// In the tick: advance a retropp::TweenPlayer<float> and store state.titleFade = fadeIn.value();

LayerKit::ImageLayer("title-card", kTitleZ, assets.titleCard,
                   {.scroll = {-8, -16}, .alpha = state.titleFade});
```

To fade the whole screen instead, put a `ColorFill` region on the frame (engine:
`docs/guide/draw-state.md`, "Whole-frame color").

## Tint a layer

A tint is a `ColorFill` region with an empty shape — the whole layer — combined by `Multiply`:

```cpp
LayerKit::Regions Tint(retropp::Rgba8 color) {
    return {retropp::Region{
        .key = "tint",
        .shape = {},
        .effects = {retropp::ScreenSpaceEffect{.kind = retropp::ScreenSpaceEffectKind::ColorFill,
                                               .fill = color}},
        .blend = retropp::BlendMode::Multiply,
    }};
}

LayerKit::TileLayer("world", kWorldZ, WorldTiles(state, assets), kMapWidth, kMapHeight,
                  {.scroll = camera, .regions = Tint({255, 170, 140})}),
```

To combine a tint with a clip on one layer, return both from one component — a layer takes one list of
regions.

## Draw nearer actors in front

In a top-down view, an actor lower on the screen is nearer. Put every actor on one sprite layer and write
each one's foot y into `Sprite::z`; the layer sorts by it:

```cpp
retropp::Sprite{.key = "actor:" + std::to_string(actor.id),
                .x = actor.x, .y = actor.y,
                .z = actor.y + kActorHeight,
                .size = {.width = 16, .height = 16},
                .atlas = assets.actors.atlasId, .tile = frameOf(actor), .palette = assets.palette}
```

`Sprite::z` snaps rather than eases, so a pair of actors crossing swaps order on the tick they cross.

## Show a layer only sometimes

A layer that is not shown is left out of the list. Build the list, then add what is conditional:

```cpp
LayerKit::Layers PlayScreen(const GameState& state, const Assets& assets) {
    LayerKit::Layers layers{
        LayerKit::TileLayer("world", kWorldZ, WorldTiles(state, assets), kMapWidth, kMapHeight,
                          {.scroll = cameraScroll(state)}),
    };
    if (state.paused) {
        layers.push_back(LayerKit::TileLayer("pause", kPauseZ, PauseBox(assets), 12, 5,
                                           {.scroll = {-32, -48}}));
    }
    return layers;
}
```

Within a sprite layer, a child that is not shown is an empty `Sprites`:
`state.swinging ? Sword(state, assets) : LayerKit::Sprites{}`.

## Paint a raster that uploads only when it changes

Keep a generation in the state that the tick increments whenever something the picture depends on
changes, and copy it into the `Raster`:

```cpp
// In the tick:
if (state.minimapDirty) { ++state.minimapGeneration; state.minimapDirty = false; }

LayerKit::Raster Minimap(const GameState& state) {
    LayerKit::Raster raster{.width = 40, .height = 32, .generation = state.minimapGeneration};
    raster.pixels.resize(40 * 32 * 4);
    paintMinimap(state, raster.pixels);
    return raster;
}
```

Start the generation at 1: generation 0 uploads every frame. The upload is saved, the painting is not —
a picture that is expensive to paint is painted in the tick and held in the state
([raster-layer.md](raster-layer.md#generation)).

## Show a full-color image at a fixed size

Load it once, then name it with `fit` set to the size it should take on screen:

```cpp
const retropp::ImageId backdrop = renderer.loadImage("assets/backdrop.png");   // once

LayerKit::ImageLayer("backdrop", kBackdropZ, backdrop,
                   {.scroll = {0, -16}, .fit = {.width = 160, .height = 128}});
```

Each screen pixel takes the image texel under it, with no filtering, so a PNG drawn at the target size
shows exactly; a larger one drops texels.

## Write an aggregate of your own

A game that needs another kind of layer writes an aggregate in the same shape as the kit's:

1. It takes a **key**, a **depth**, the **content** (by value) and an **options** struct, and returns a
   `retropp::DrawLayer`.
2. If the content holds a span, it **keeps the content under the key** — a function-local static map from
   key to content — and points the span into it. If it holds none, it keeps nothing.
3. Its **size** is `retropp::Renderer::defaultViewport.size()`, read at the call.
4. It **copies every options field** onto the layer and owns nothing else.

`tile_layer.h` is the template: thirty lines that do exactly those four things.
