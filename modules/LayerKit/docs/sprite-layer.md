# `SpriteLayer`

```cpp
#include "LayerKit/sprite_layer.h"

namespace LayerKit {
[[nodiscard]] retropp::DrawLayer SpriteLayer(std::string_view key, std::int32_t z,
                                             std::initializer_list<Sprites> children,
                                             LayerOptions options = {});
}
```

Gathers sprite components onto one sprite layer. Each child is a `Sprites` — the return value of a sprite
component — and the children are declared as a list.

## What the returned layer carries

| Field | Value |
|---|---|
| `key` | `key` |
| `z` | `z` |
| `size` | the active viewport, `retropp::Renderer::defaultViewport.size()`, read at the call |
| `content` | `retropp::SpriteContent` whose `sprites` span holds every child's sprites, concatenated in declaration order |
| everything else | copied from `options` ([layer-options.md](layer-options.md)) |

## Examples

```cpp
// The player and every actor on one layer that follows the camera.
frame.layers.push_back(LayerKit::SpriteLayer(
    "actors", kActorsZ, {Player(state, assets), Actors(state, assets)},
    {.scroll = {state.camera.x, state.camera.y - kHudHeight}}));

// A HUD: no scroll, so a sprite at (8, 4) is drawn at screen pixel (8, 4).
frame.layers.push_back(LayerKit::SpriteLayer("hud", kHudZ, {Hearts(state.player.halfHearts, assets)}));

// A child that is not always there: an empty Sprites contributes nothing.
LayerKit::SpriteLayer("actors", kActorsZ, {
    Player(state, assets),
    state.swinging ? Sword(state, assets) : LayerKit::Sprites{},
});
```

## Things to know

- **Placement.** A sprite's `x`/`y` are in the layer's space, before scroll: screen position is
  `(x − scroll.x, y − scroll.y)`. A world layer scrolled by the camera places sprites in world pixels; a
  layer at scroll `{0, 0}` places them on the screen.
- **Order within the layer.** Each sprite's own `Sprite::z` orders it first; sprites with equal `z` keep
  the order they were submitted in, which is the children's declaration order. A top-down game writes each
  actor's foot y into `Sprite::z` and the nearer one draws in front.
- **Sprite keys are unique across the whole frame**, over every sprite layer, not per layer — and must
  name the object, never its index in a list ([keys-and-depth.md](keys-and-depth.md)).
- **Storage.** The sprites are kept under `key` until the next `SpriteLayer` call with that key. One call
  per key per frame: a second call with the same key replaces what the first kept, and the layer the first
  call returned can no longer be drawn ([concepts.md](concepts.md#why-the-aggregates-keep-content)).
- **No children** (`SpriteLayer("k", z, {})`) returns a layer with an empty span, which draws nothing.

## See also

- Engine `docs/guide/sprites.md` — every `Sprite` field: placement, art, flips, `z`, alpha, blend,
  transforms, sprite effects, and what interpolates.
- Engine `docs/guide/draw-state.md` — `SpriteContent` and the layer model.
