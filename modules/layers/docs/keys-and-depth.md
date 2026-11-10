# Keys and depth

Every layer and every sprite carries a key, and every layer a depth. Choosing them is part of designing a
component: the key decides what the engine treats as the same thing from one tick to the next, and the
depth decides what is in front.

## Depth

- **A layer's `z`** orders layers back to front. No two layers in a frame share a `z`.
- **A sprite's `Sprite::z`** orders sprites within their layer. It may repeat; equal values keep the
  order they were submitted in.
- **Depth is which layer.** A thing that must be in front of another is on a higher layer, or — within
  one sprite layer — has a higher `Sprite::z`.

Keep every depth in one header, so the whole stacking order is read in one place and two components never
collide:

```cpp
// render/layers.h — every depth in the game, back to front.
inline constexpr std::int32_t kWorldZ   = 0;
inline constexpr std::int32_t kActorsZ  = 10;   // every actor's Sprite::z is its foot y
inline constexpr std::int32_t kEffectsZ = 20;
inline constexpr std::int32_t kHudZ     = 100;
```

Leave gaps between values, so a layer added later fits between two without renumbering.

## Layer keys

A layer key is the layer's identity: unique within a frame, stable from frame to frame, and named for
what the layer shows — `"world"`, `"actors"`, `"hud"`, `"textbox"`.

The kit adds one rule of its own: the key is also where the aggregate keeps the layer's content. So:

- **one aggregate call per key per frame**, and
- **a key is a name, not a counter.** A key built from a frame number or an ever-growing count leaves a
  kept entry behind for every value. A key built from a bounded set — `"menu:" + std::to_string(depth)`
  for a stack of at most a few windows — is fine.

## Sprite keys

A sprite key is unique **across every sprite layer in the frame**, not only within its own layer, and it
names the object:

| The object | Its key | So that |
|---|---|---|
| A singleton | a literal: `"player"`, `"cursor"` | — |
| Something spawned | its own id, minted at spawn: `"actor:" + std::to_string(actor.id)` | a spawn or a death never shifts another object's key |
| A fixed grid of things | its cell: `brick:<row>:<column>` | — |
| One of several copies of the same thing | the thing and its index in a bounded set: `"heart:" + std::to_string(i)` | heart 2 stays heart 2 as the others change |
| A part of another object | the parent's key plus a suffix: `"player:shadow"` | the table of keys reads as the table of objects |

**Never key by position in the list being built** (`"s" + std::to_string(sprites.size())`). When the
population changes, every later key lands on a different object, and the engine eases each one from the
wrong object's last position.

## Easing and cutting

The engine eases a layer or sprite that keeps its key — a layer's scroll, alpha and transform, a sprite's
position, alpha and transform — and puts a new key straight where it is told. So:

- **To move smoothly**, keep the key.
- **To jump** — a respawn, a teleport, a world layer arriving in a new area — change the key for that
  moment: `"actor:" + std::to_string(newId)` after a respawn, or a world layer keyed per area
  (`"world:" + area`) so arriving is a cut.
- **A cursor that should land on its new choice, not slide to it**, includes the choice in its key.

## See also

- Engine `docs/guide/sprites.md` — "Identity — `key`", and what interpolates and what snaps.
- Engine `docs/guide/draw-state.md` — "Layer identity vs depth", "Layer-key uniqueness is a contract".
