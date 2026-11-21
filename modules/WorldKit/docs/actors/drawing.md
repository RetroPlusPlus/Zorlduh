# Drawing

What an actor shows and how it gets on screen: looks, the slot an actor is drawn at, depth, sprite
keys, and the `Sprites` call your frame makes.

```cpp
#include "ActorKit/look.h"      // Look, lookOf, LookRule, Looks
#include "ActorKit/sprites.h"   // Sprites
```

`actor.h` includes `look.h`. `sprites.h` is the only ActorKit header that creates a draw type.

## Contents

- [Overview](#overview)
- [What an actor shows](#what-an-actor-shows)
- [Slots](#slots)
  - [A layer for each area](#a-layer-for-each-area)
- [Depth](#depth)
- [Keys](#keys)
- [Fresh keys](#fresh-keys)
- [`Look`](#look)
- [`LookRule` and `Looks`](#lookrule-and-looks)
- [`Sprites`](#sprites)

## Overview

Once, after the art is loaded:

```cpp
// Looks, made from your sheet.
const ActorKit::Look gateShut = ActorKit::lookOf(sheet, kGateShut, gatePalette);
const ActorKit::Look gateOpen = ActorKit::lookOf(sheet, kGateOpen, gatePalette);

// The gate shows gateOpen while GateOpen holds, and gateShut the rest of the time.
const ActorKit::Actor gate{.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 16},
                           .solid = !GateOpen,
                           .look = {{GateOpen, gateOpen}, {gateShut}}};
```

Every frame:

```cpp
// The world's layers, then, for every area on screen, its actors at each of its two slots.
frame.layers = WorldKit::WorldLayers(state.camera, world);
for (const WorldKit::Shown& shown : state.camera.shown(world)) {
    const WorldKit::Place place{.layout = shown.layout, .area = shown.area};
    frame.layers.push_back(
        AreaSlot(world, shown, "underfoot", ActorKit::Sprites(state.actors, config, place, "underfoot", state)));
    frame.layers.push_back(
        AreaSlot(world, shown, "actors", ActorKit::Sprites(state.actors, config, place, "actors", state)));
}
frame.regions = WorldKit::Fade(state.camera, world);
renderer.renderFrame(frame);
```

Names from your game:

| Name | What it is |
|---|---|
| `sheet` | the `retropp::AtlasManifest` of your sprite sheet, from `Renderer::loadAtlas` |
| `kGateShut`, `kGateOpen` | slot numbers on that sheet |
| `gatePalette` | a `retropp::PaletteId` |
| `GateOpen` | a `ScriptKit::Value<bool>` |
| `state` | your game state, with `actors` and `camera` |
| `config` | your `ActorKit::Config` |
| `world` | your `WorldKit::World`, whose areas have the actor slots `"underfoot"` and `"actors"` |
| `AreaSlot` | your component that puts one area's sprites at one slot on a layer of their own ([Slots](#slots) has it, whole) |
| `frame`, `renderer` | your `retropp::FrameDrawState` and `retropp::Renderer` |

ActorKit reports sprites and your game draws them. `ActorKit::Sprites` returns the sprites of the
actors in one area at one slot. `WorldKit::WorldLayers`, `WorldKit::Camera::shown` and `WorldKit::Fade`
are WorldKit's (its `docs/drawing.md` and `docs/camera.md`): the map's layers, the areas on screen with
the scroll each takes, and the fade over the screen.

## What an actor shows

Each frame an actor shows one of three things:

| The actor has | It shows |
|---|---|
| a look whose condition holds | the first such look in its `look` list |
| a mover, and every look's condition false or an empty `look` | its mover's current frame |
| an empty `mover`, and every look's condition false or an empty `look` | The actor is live and `Sprites` skips it. |

```cpp
// Asleep, the dog shows its sleeping look. Awake, it shows its walker's frame.
const ActorKit::Actor dog{.slot = "actors", .solid = true,
                          .ownValues = {{Awake, false}, {Heading, retropp::Dir::None}},
                          .look = {{!Awake, dogAsleep}},
                          .mover = ActorKit::movedBy<TopDownMovement::Walker>(dogWalk, Heading)};
```

`Awake` and `Heading` are values the dog owns, `dogAsleep` is an `ActorKit::Look` and `dogWalk` a
`TopDownMovement::WalkerConfig`.

- **Conditions are read on the frame the actor is drawn.** A condition reads the actor's own values,
  ActorKit's six and your game's, like any expression of the actor's.
- **Order matters.** The first look that holds is shown. Put an unconditional look last, as the
  default.
- **A look can be mirrored.** `Look::flipX` and `Look::flipY` flip the art.
- **An actor with a box, scripts and an empty look is an area that reacts:** a tripwire, a zone that
  starts a script when the hero walks in.
- **One sprite per actor.** For a shadow or a carried item, add sprites of your own to the same layer,
  placed from [`Actors::position`](actors.md#place-position-box-and-facing).

## Slots

`Actor::slot` names the sprite layer the actor is drawn on. In a WorldKit world each area lists its
actor slots, each with a name and a depth:

```cpp
.actorSlots = {{.name = "underfoot", .z = 5}, {.name = "actors", .z = 10}},
```

| Slot | Typical actors |
|---|---|
| `"actors"` | everything that walks, and what stands among it: the hero, enemies, chests, gates |
| `"underfoot"` | what is drawn under their feet: plates, puddles, rugs |
| a slot above a canopy layer | what flies over it |

### A layer for each area

Draw each area's actors on a layer of their own: one layer for each area on screen and each slot, keyed
by the area, at that slot's depth, with that area's scroll. `AreaSlot` is that layer:

```cpp
// The layer of `sprites`, the actors of the area `shown` at its slot `slot`: at the depth that slot is drawn
// at for that area, with that area's scroll, kept inside the view. Each area has a layer of its own, keyed by
// the area, so a layer's scroll moves only with the camera, and an area scrolling in brings its actors with it.
retropp::DrawLayer AreaSlot(const WorldKit::World& world, const WorldKit::Shown& shown, std::string_view slot,
                            LayerKit::Sprites sprites) {
    const std::string key =
        "farm:" + std::to_string(shown.layout) + ":" + std::to_string(shown.area) + ":" + std::string(slot);
    const CameraKit::View& view = world.layouts[static_cast<std::size_t>(shown.layout)].view;
    return LayerKit::SpriteLayer(key, WorldKit::ActorZ(world, shown, slot), {std::move(sprites)},
                                 {.scroll = shown.scroll, .regions = WorldKit::ViewStencil(view)});
}
```

The frame calls it for every area `state.camera.shown(world)` lists, as the overview does.
`WorldKit::Camera::shown`, `WorldKit::ActorZ` and `WorldKit::ViewStencil` are WorldKit's (its
`docs/how-to.md`, "Put things in an area"), and `LayerKit::SpriteLayer` is LayerKit's.
`WorldKit::ActorZ(world, shown, slot)` is the depth the slot is drawn at for the area `shown`: with two
areas on screen, every area's `"underfoot"` stays under every area's `"actors"`. The `"farm:"`
prefix is the example's: use one of your own.

`shown` lists the camera's area, every area joined to it by a seam that is in view, and during a scroll
the area scrolling in. With a layer for each:

| Situation | What the player sees |
|---|---|
| The screen scrolls into the next area. | That area's actors scroll in with its map. |
| A neighbor across a seam is in view. | Its actors are drawn on its map, before the camera crosses. |
| The camera's area changes. | Every layer keeps its own scroll, so every actor stays where it is on screen. |

Draw them this way, with one layer per area, and keep `WorldKit::Actors` for sprites that belong to the
camera's area alone. `WorldKit::Actors` keeps one layer for a slot across every area. Its scroll jumps
when the camera's area changes, and the engine eases a layer's scroll, so actors drawn on it slide into
place after every area change, and the area scrolling in shows its actors only at the scroll's end.

Watch for:

- **An actor that changes area changes layer.** The engine matches a sprite by its key across every
  layer of the frame, and eases it from where the key was on the tick before. On the tick an actor
  moves into another area, its position is in that area's pixels and its sprite is on that area's
  layer, so call [`fresh`](#fresh-keys) for it on that tick. Your camera code does it for the player
  ([how-to.md](how-to.md#follow-the-player-with-the-camera)).
- **Sprites of your own share the layer.** Add them to the list you hand `AreaSlot`, in that area's
  pixels.
- **A slot needs a call.** An actor whose slot your frame leaves out is live and undrawn.
- **`Sprites` answers for any slot name.** `WorldKit::ActorZ` throws `std::out_of_range` for an area
  that lacks the slot, so give every area the slots you draw.

## Depth

Depth has two levels.

| Level | Decided by |
|---|---|
| Between slots, and against the map's layers | the slot's `z` in your world |
| Within one slot | each sprite's own `z`: the bottom of the actor's box |

Within a slot, an actor lower on the screen has a higher `z` and draws in front of one above it.

| Actor | Position | Sprite `z` |
|---|---|---|
| a box `{.x = 2, .y = 10, .width = 12, .height = 6}` | (40, 50) | 66: 50 plus the box's bottom edge, 16 |
| an empty box, showing a look 16 pixels tall | (60, 30) | 46: 30 plus the look's height |
| a mover with a collision box `{.x = 2, .y = 8, .width = 12, .height = 8}` | (40, 40) | 56: 40 plus 16 |

- Set the box at the actor's feet. A tall actor then sorts by where it stands, and a walker passing
  behind it is drawn behind it.
- To keep one thing always under or over another, give them different slots.

## Keys

Every sprite `Sprites` returns has a key:

```
actor:<set>:<id>
```

| Part | What it is |
|---|---|
| `<set>` | A number the `Actors` value took when it was constructed. |
| `<id>` | The actor's [`Id`](actors.md#id). |

The engine matches sprites between ticks by key and eases a sprite's position from one tick to the
next (the engine's `docs/guide/sprites.md`, "Identity — `key`").

- **A key belongs to one actor for its whole life.** Ids are handed out once.
- **Keys from different sets differ.** Two `Actors` values both have an actor 1, and their keys differ
  in `<set>`.
- **A copy of an `Actors` value keeps its number,** so a loaded state's actors have the keys they were
  saved with.
- **`Actors::key(id)`** returns the key an actor's sprite has now. Use it to find one actor's sprite in
  the list `Sprites` returned.
- Keep the `actor:` prefix for ActorKit's sprites, and key your own sprites differently.

```cpp
LayerKit::Sprites sprites = ActorKit::Sprites(state.actors, config, here, "actors", state);
for (retropp::Sprite& sprite : sprites) {
    if (sprite.key.value == state.actors.key(hero)) {
        sprite.alpha = 0.5f;   // the hero, half see-through
    }
}
```

`hero` is the hero's `ActorKit::Id`.

## Fresh keys

When an actor jumps from one place to another, the engine eases its sprite across the screen, because
the key is the same on both ticks. Call `fresh` to show the actor at its new position at once:

```cpp
ActorKit::tick(state.actors, config, state.camera, input, state);

if (state.warping) {
    state.actors.moveTo(hero, {.layout = 0, .area = 2}, {40, 96});
    state.actors.fresh(hero);
}
```

`state.warping` is a `bool` of yours. `fresh(id)` reports the actor fresh for this tick. While it is
reported and the actor has moved since the last tick began, the actor's sprite goes out under a
throwaway key, which the engine mounts as a new sprite, in place.

| Tick | Your code | The sprite's key |
|---|---|---|
| 1 | `tick` alone | `actor:3:1` |
| 2 | `tick`, then `moveTo` and `fresh` | `actor:3:1:a` |
| 3 | `tick`, then `moveTo` and `fresh` again | `actor:3:1:b` |
| 4 | `tick`, then `fresh`, the actor standing where it was | `actor:3:1:b` |
| 5 | `tick` alone | `actor:3:1` |

Here the set's number is 3 and the actor's id is 1.

- **Call `fresh` after `tick`,** on each tick you want it. The report lasts until the next `tick`
  begins.
- **Two throwaway keys alternate,** so two fresh moves in a row each mount a new sprite.
- **The actor's own key comes back** on the first tick you leave `fresh` out.
- **For an actor that should always appear at once,** call `fresh` for it on every tick.

## `Look`

```cpp
struct Look {
    retropp::AssetDimensions size{};
    retropp::AtlasId atlas{};
    std::uint16_t tile = 0;
    retropp::PaletteId palette{};
    bool flipX = false;
    bool flipY = false;

    constexpr bool operator==(const Look&) const noexcept = default;
};

Look lookOf(const retropp::AtlasManifest& sheet, std::size_t slot, retropp::PaletteId palette);
```

The art an actor shows. Its fields are the fields of the same names on a `retropp::Sprite`: which art,
through which palette, and how it is flipped.

`lookOf` makes the look of one slot of a sheet. Call it once per look, after the sheet is loaded:

```cpp
const ActorKit::Look arrowRight = ActorKit::lookOf(sheet, kArrow, arrowPalette);

ActorKit::Look arrowLeft = arrowRight;
arrowLeft.flipX = true;
```

| `lookOf` argument | What it is |
|---|---|
| `sheet` | The manifest the renderer returned when it loaded the sheet. |
| `slot` | The slot's number on the sheet, in the order the sheet was read. |
| `palette` | The palette the look is drawn through. |

- The look takes its `size` and `tile` from the slot, so sheets with slots of any size work.
- One sheet through two palettes gives two looks: call `lookOf` once for each palette.
- `lookOf` throws `std::out_of_range` for a slot past the sheet's last.
- A test can fill in a `Look` by hand, with any `atlas` and `tile`
  ([testing.md](testing.md#test-what-an-actor-shows)).

## `LookRule` and `Looks`

```cpp
struct LookRule {
    ScriptKit::Expr<bool> when = true;
    Look look{};

    LookRule(Look shown);
    LookRule(ScriptKit::Expr<bool> condition, Look shown);
};

class Looks {
public:
    Looks() = default;
    Looks(Look look);
    Looks(std::initializer_list<LookRule> rules);

    const std::vector<LookRule>& rules() const noexcept;
    bool empty() const noexcept;
};
```

`Looks` is the type of `Actor::look`: a list of rules, each a look and the condition it is shown under.
You write one look, or braces:

| Written as | The actor shows |
|---|---|
| `.look = eggLook` | `eggLook`, always |
| `.look = {{GateOpen, gateOpen}, {gateShut}}` | `gateOpen` while `GateOpen` holds, and `gateShut` the rest of the time |
| `.look = {{!Awake, dogAsleep}}` | `dogAsleep` while `Awake` is false, and its mover's frame while it is true |
| `.look = {{Health <= 1, slimeHurt}, {Angry, slimeRed}, {slimePlain}}` | the first of the three whose condition holds |
| `look` left out | its mover's frame |

- A rule written as a look alone, `{gateShut}`, always holds.
- A condition is any `ScriptKit::Expr<bool>`: a `Value<bool>`, `!Awake`, `Health <= 1`,
  `Angry && !Asleep`.
- `rules()` and `empty()` are what `Sprites` reads.

## `Sprites`

```cpp
template <class State>
LayerKit::Sprites Sprites(const Actors& actors, const Config& config, WorldKit::Place place,
                          std::string_view slot, const State& state);
```

Returns one sprite for each live actor in `place` whose `slot` is `slot` and that has something to
show, in spawn order. `LayerKit::Sprites` is a `std::vector<retropp::Sprite>`. Call it every frame, once
for each slot you draw.

```cpp
for (const WorldKit::Shown& shown : state.camera.shown(world)) {
    const WorldKit::Place place{.layout = shown.layout, .area = shown.area};
    frame.layers.push_back(
        AreaSlot(world, shown, "actors", ActorKit::Sprites(state.actors, config, place, "actors", state)));
}
```

`AreaSlot` is your component, in [A layer for each area](#a-layer-for-each-area).

| Sprite field | Value |
|---|---|
| `key` | [`Actors::key(id)`](#keys) |
| `x`, `y` | the actor's position, in its area's pixels |
| `z` | `y` plus the bottom edge of the actor's box. With an empty box, `y` plus the height of what it shows. |
| `size`, `atlas`, `tile`, `palette` | from the look shown, or from the mover's frame |
| `flipX`, `flipY` | from the look shown. `false` for a mover's frame. |
| every other field | the engine's default |

- **`state` is read through your bindings,** for the looks' conditions. `Sprites` only reads.
- **An actor spawned this tick is reported at once.**
- **A frozen actor is reported like any other,** where it stands, with the first look whose condition
  holds on that frame ([actors.md](actors.md#freeze-wake-and-frozen)).
- **The position is in the area's pixels.** Draw the sprites on a layer with that area's scroll, one
  layer for each area ([A layer for each area](#a-layer-for-each-area)).
- **A mover's frame resolves its art through the renderer** that loaded the sheet. A look carries its
  art already. So an actor shown by looks alone needs only the engine library, and one shown by its
  mover's frame needs a renderer ([testing.md](testing.md#what-each-part-needs)).
- **Each call visits every actor of the set.**

| Throws | When |
|---|---|
| `std::out_of_range` | A look's condition reads a value outside the actor's own, ActorKit's six and your bindings. |
| `std::logic_error` | `config.bindings` is written for another type than `State`. |
| what the mover's `frame` throws | A `TopDownMovement` walker throws `std::logic_error` when the walk animation for its direction is empty. |
