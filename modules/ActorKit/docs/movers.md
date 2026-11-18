# Movers

What moves an actor: a mover of your own type, the three things that can drive it, and what a walking
actor collides with.

```cpp
#include "ActorKit/mover.h"   // movedBy, Mover, Solids
```

`actor.h` includes this header.

## Contents

- [Overview](#overview)
- [The three drives](#the-three-drives)
- [What a mover collides with](#what-a-mover-collides-with)
- [What an actor takes from its mover](#what-an-actor-takes-from-its-mover)
- [A mover of your own](#a-mover-of-your-own)
- [`movedBy`](#movedby)
- [`Mover`](#mover)
- [`Solids`](#solids)

## Overview

```cpp
// The player: an InputWalker steered by the pad.
const ActorKit::Actor hero{
    .slot  = "actors",
    .solid = true,
    .mover = ActorKit::movedBy<TopDownMovement::InputWalker>(heroWalk, &Input::move),
};

// A hen: a Walker that its own scripts steer, through a value it owns.
const ActorKit::Actor hen{
    .slot      = "actors",
    .solid     = true,
    .ownValues = {{Heading, retropp::Dir::None}},
    .scripts   = {&wander},
    .mover     = ActorKit::movedBy<TopDownMovement::Walker>(henWalk, Heading),
};
```

Names from your game:

| Name | What it is |
|---|---|
| `heroWalk`, `henWalk` | `TopDownMovement::WalkerConfig`s: speed, collision shape and walk animations |
| `Input` | your input struct, with `move`, a `TopDownMovement::DirectionalInput` |
| `Heading` | a `ScriptKit::Value<retropp::Dir>` |
| `wander` | a `ScriptKit::Script` that sets `Heading` |

The walking is your mover's. `movedBy<M>(config, drive)` names a mover type, that type's config and
what drives it. For each actor spawned, ActorKit makes one `M` bound to the actor's position. Every tick
it calls the mover's `walk` with the drive's heading and with what is solid, and reads back the mover's
facing, box and frame.

`TopDownMovement::Walker` and `TopDownMovement::InputWalker` are two mover types that fit. ActorKit
is independent of both: any type with the same members fits
([A mover of your own](#a-mover-of-your-own)).

## The three drives

The drive decides the heading a mover is walked with each tick. Choose by what steers the actor.

| Drive | Written as | Use it for |
|---|---|---|
| a member of your input value | `&Input::move` | the player |
| a `ScriptKit::Value<retropp::Dir>` | `Heading` | an actor its own scripts steer |
| a function of the actor's values | `towardPlayer` | a heading your code works out |

### A member of your input

```cpp
.mover = ActorKit::movedBy<TopDownMovement::InputWalker>(heroWalk, &Input::move),
```

Each tick the mover is walked with `input.move`, where `input` is the value you handed `tick`.

- The member's type is what the mover's `walk` takes: a `TopDownMovement::DirectionalInput` for an
  `InputWalker`, a `TopDownMovement::Directions` for a `Walker`.
- `tick` throws `std::logic_error` when its input is of another type than the member belongs to.

### A direction value

```cpp
.ownValues = {{Heading, retropp::Dir::None}},
.mover     = ActorKit::movedBy<TopDownMovement::Walker>(henWalk, Heading),
```

Each tick the mover is walked toward the direction the value holds. A script steers the actor by
setting the value:

```cpp
const ScriptKit::Script pace{
    .name = "pace",
    .on = ActorKit::Enter,
    .body = {loop({set(Heading, retropp::Dir::Left), wait(1s), set(Heading, retropp::Dir::Right), wait(1s)})},
};
```

| The value holds | The actor |
|---|---|
| `Dir::Up`, `Down`, `Left` or `Right` | walks that way, one tick at a time, for as long as the value holds it |
| `Dir::None` | stands |

- The value is the actor's own, listed in `ownValues`, or one of your game's. With a value of your
  game's, every actor driven by it walks the same way.
- The direction is handed to the mover as its own heading type. Any heading type with the `bool`
  members `up`, `down`, `left` and `right` works.
- A `retropp::Dir` is one of four directions. For a diagonal heading, use a function.

### A function of the actor's values

```cpp
// A heading toward the player.
TopDownMovement::Directions towardPlayer(const ScriptKit::Values& values) {
    const int dx = values.get(ActorKit::PlayerX) - values.get(ActorKit::X);
    const int dy = values.get(ActorKit::PlayerY) - values.get(ActorKit::Y);
    return {.up = dy < 0, .down = dy > 0, .left = dx < 0, .right = dx > 0};
}
```

```cpp
.mover = ActorKit::movedBy<TopDownMovement::Walker>(slimeWalk, towardPlayer),
```

Each tick the function is called with the actor's values, and the mover is walked with the heading it
returns. `slimeWalk` is a `TopDownMovement::WalkerConfig`.

- The function returns the heading type the mover's `walk` takes.
- It is handed the actor's own values and ActorKit's six. A value of your game's is there too when one
  of the actor's scripts, its `solid` or one of its looks reads it.
- `ScriptKit::Values::get` throws `std::out_of_range` for a value that was left out.
- With the player in another area, `PlayerX` and `PlayerY` equal the actor's own position, so this
  slime stands until the hero comes into its area.

## What a mover collides with

Each tick, the mover's `walk` is handed a [`Solids`](#solids) to test its steps against:

| What is solid | For which actors |
|---|---|
| The solid cells of the area's collision map: every cell but the layout's `open` value. | every actor |
| The box of every other actor that is solid this tick, in the same area and the same set. | every actor |
| Everything outside the area's own rectangle. | every actor but the player |

The player walks out through its area's openings, as WorldKit's `AreaSolids` lets it, and your
camera code carries it into the next area
([how-to.md](how-to.md#follow-the-player-with-the-camera)). Every other actor stays inside its area, at
an open doorway included.

| Situation | Result |
|---|---|
| A walker heads into a wall. | It stops with its box against the wall. |
| A walker heads into a solid actor. | It stops with the two boxes meeting. |
| A walker heads into an actor whose `solid` is false. | It walks through. |
| A walker whose own `solid` is false heads into a solid actor. | It stops. `solid` says whether others stop at this actor. |
| The gate's `solid` turns false. | Walkers pass from that tick on. |
| A walker other than the player reaches a doorway in the area's edge. | It stops at the edge. |
| The camera runs a scroll or a fade. | Every walker stands until it ends. |

- **Actors walk in spawn order.** An actor that walks later in the tick meets the earlier ones where
  they now stand. Take two walkers heading the same way in single file. When the front one was spawned
  first, the back one keeps up with it. When the back one was spawned first, it trails by a pixel.
- **Two solid actors whose boxes overlap hold each other in place.** Spawn solid actors apart, and
  keep a walker out of a box before its `solid` turns true.
- **A solid actor stays put when walked into.** It stays where it is until its own mover, a script or
  your code moves it.

## What an actor takes from its mover

| The actor's | Comes from |
|---|---|
| position | The actor. The mover is bound to it and writes it when it walks. |
| box | The bounds of the mover's collision shape. `Actor::box` stays empty. |
| facing | The mover's `facing()`. Interact acts on the actor it faces. |
| frame | The mover's `frame(config)`, shown while every look's condition is false ([drawing.md](drawing.md#what-an-actor-shows)). |

## A mover of your own

`movedBy` takes any type with these members:

| Member | Requirement |
|---|---|
| a constructor | Takes a `retropp::Vec2i&`: the position the mover walks. |
| copying, and `bind(retropp::Vec2i&)` | The type is copyable, and `bind` moves a copy onto another position. |
| `walk(const Config&, D, const ActorKit::Solids&)` | One tick of walking. `D` is the heading the drive hands over. |
| `facing()` | Returns a `retropp::Dir`. |
| `collision(const Config&)` | Returns something with a `bounds()` member that returns a `retropp::IntRect` in the area's pixels. |
| `frame(const Config&)` | Returns a `const retropp::AnimationFrame&`. |

`Config` is whatever config type you pass `movedBy`. A complete mover that floats a pixel at a time
and stops at anything solid:

```cpp
// A heading: four flags.
struct Way {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
};

struct FloaterConfig {
    int speed = 1;
    retropp::IntRect box{.x = 0, .y = 0, .width = 8, .height = 8};
    retropp::AnimationFrame frame{};
};

class Floater {
public:
    explicit Floater(retropp::Vec2i& at) : at_(&at) {}
    void bind(retropp::Vec2i& at) { at_ = &at; }

    // One tick toward `way`: up to `speed` pixels on each axis, stopping where the box meets a solid.
    void walk(const FloaterConfig& config, Way way, const ActorKit::Solids& solids) {
        const int dx = (way.right ? 1 : 0) - (way.left ? 1 : 0);
        const int dy = (way.down ? 1 : 0) - (way.up ? 1 : 0);
        if (dx != 0) {
            facing_ = dx > 0 ? retropp::Dir::Right : retropp::Dir::Left;
        }
        if (dy != 0) {
            facing_ = dy > 0 ? retropp::Dir::Down : retropp::Dir::Up;
        }
        step(config, {.x = dx, .y = 0}, solids);
        step(config, {.x = 0, .y = dy}, solids);
    }

    retropp::Dir facing() const { return facing_; }

    struct Shape {
        retropp::IntRect box;
        retropp::IntRect bounds() const { return box; }
    };
    Shape collision(const FloaterConfig& config) const { return {boxAt(config, *at_)}; }

    const retropp::AnimationFrame& frame(const FloaterConfig& config) const { return config.frame; }

private:
    static retropp::IntRect boxAt(const FloaterConfig& config, retropp::Vec2i at) {
        return {.x = at.x + config.box.x, .y = at.y + config.box.y, .width = config.box.width, .height = config.box.height};
    }

    void step(const FloaterConfig& config, retropp::Vec2i unit, const ActorKit::Solids& solids) {
        for (int n = 0; n < config.speed && (unit.x != 0 || unit.y != 0); ++n) {
            const retropp::Vec2i next{.x = at_->x + unit.x, .y = at_->y + unit.y};
            if (solids.solidBox(boxAt(config, next))) {
                return;
            }
            *at_ = next;
        }
    }

    retropp::Vec2i* at_;
    retropp::Dir facing_ = retropp::Dir::Down;
};
```

An actor uses it like any mover:

```cpp
const ActorKit::Actor ghost{
    .slot      = "actors",
    .ownValues = {{Drift, retropp::Dir::Left}},
    .mover     = ActorKit::movedBy<Floater>(FloaterConfig{.speed = 2, .frame = ghostFrame}, Drift),
};
```

`Drift` is a `ScriptKit::Value<retropp::Dir>` and `ghostFrame` a `retropp::AnimationFrame`.

- **The mover reads and writes the position it is bound to.** ActorKit owns the position and binds
  each actor's mover to it.
- **`walk` decides everything about the step:** speed, collision shape, sliding, facing. `Solids`
  answers one question, whether a rectangle touches something solid.
- **`Way` works with all three drives.** A direction value sets the one flag it names, a member of your
  input is a `Way` field, and a function returns a `Way`.
- **`frame`** returns the art `Sprites` shows for the actor. A frame with its `tileIndex` left unset
  is a palette-only frame, and `Sprites` then shows the actor through its looks alone.

## `movedBy`

```cpp
template <class M, class Config, class Drive>
Mover movedBy(Config config, Drive drive);
```

Returns the `Mover` for `Actor::mover`: a mover of type `M`, walked under `config` by `drive`. You name
`M` and the other two are deduced:

```cpp
.mover = ActorKit::movedBy<TopDownMovement::Walker>(henWalk, Heading),
```

| Argument | What it is |
|---|---|
| `M` | The mover type ([requirements](#a-mover-of-your-own)). |
| `config` | The config `M`'s `walk`, `collision` and `frame` take. It is copied once, kept, and shared by every actor spawned from the value. |
| `drive` | One of [the three drives](#the-three-drives). |

- Call it where you build the actor value, after the art is loaded. A
  `TopDownMovement::WalkerConfig` holds the walk animations, and takes its timing from the engine's
  active config when it is constructed.
- A drive of any other kind fails to compile, with a message that names the three.
- An `M` that a `retropp::Vec2i&` fails to construct fails to compile, with a message that says so.

## `Mover`

```cpp
class Mover {
public:
    Mover() = default;

    explicit operator bool() const noexcept;
    const detail::MoverKind* kind() const noexcept;
};
```

The type of `Actor::mover`. `movedBy` makes one. A default `Mover` is empty, which is an actor that
stays where it is put.

| Member | Returns |
|---|---|
| `operator bool` | `true` for a mover made by `movedBy`, `false` for an empty one. |
| `kind()` | What `movedBy` captured, for `spawn` and `tick`. |

Copies share what `movedBy` captured. Each actor spawned gets a mover of its own.

## `Solids`

```cpp
class Solids {
public:
    Solids(WorldKit::AreaSolids area, retropp::IntRect bounds, bool confined,
           std::span<const retropp::IntRect> others) noexcept;

    bool solidBox(retropp::IntRect box) const noexcept;
};
```

What is solid to one walking actor on one tick. `tick` builds it and hands it to the mover's `walk`.

`solidBox(box)` returns `true` when any pixel of `box`, a rectangle in the area's pixels, is solid:

| Part | Solid where |
|---|---|
| `area` | WorldKit reports the area's collision map solid (WorldKit's `docs/camera.md`, "AreaSolids"). |
| `bounds`, when `confined` is true | Any part of `box` lies outside `bounds`. `tick` passes the area's rectangle, and `true` for every actor but the player. |
| `others` | `box` overlaps one of the rectangles. `tick` passes the boxes of the other solid actors in the area. |

- A box of zero width or height is open.
- A `Solids` is valid during the `walk` call it is handed to. `others` is a span over a list `tick`
  owns.
- It has the `solidBox(retropp::IntRect)` member that TopDownMovement's walkers ask for, so they take
  it as it is.
- A mover's test builds one by hand:

```cpp
const std::vector<retropp::IntRect> crates{{.x = 64, .y = 40, .width = 16, .height = 16}};
const ActorKit::Solids solids{WorldKit::AreaSolids{.world = &world, .layout = 0, .area = 0},
                              retropp::IntRect{.x = 0, .y = 0, .width = 160, .height = 128},
                              /*confined=*/true, crates};

const bool blocked = solids.solidBox({.x = 60, .y = 44, .width = 8, .height = 8});   // true: it overlaps the crate
```

`world` is a `WorldKit::World` whose area 0 of layout 0 is 160 by 128 pixels.
