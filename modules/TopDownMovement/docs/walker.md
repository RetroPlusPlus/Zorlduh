# Walker reference

`Ways`, `Facing`, `WalkerConfig`, `DirectionalAnimations`, `Walker`, `InputWalker`, the direction types
and `readDirections`, and `WalkerSprite`. Routes are in [routes.md](routes.md).

```cpp
#include "TopDownMovement/directions.h"     // Ways, Facing, Directions, DirectionalInput, DirectionalActions, readDirections
#include "TopDownMovement/walker.h"         // DirectionalAnimations, WalkerConfig, Walker
#include "TopDownMovement/input_walker.h"   // InputWalker
#include "TopDownMovement/walker_sprite.h"  // WalkerSprite
```

## Contents

- [`Ways`](#ways)
- [`Facing`](#facing)
- [`WalkerConfig`](#walkerconfig)
- [`Walker`](#walker)
- [`InputWalker`](#inputwalker)
- [Reading the directions](#reading-the-directions)
- [`WalkerSprite`](#walkersprite)

## `Ways`

```cpp
enum class Ways : std::uint8_t { Four, Eight };
```

How many directions: the four cardinal ones, or those and the four diagonals. A `WalkerConfig` has two:
`movement`, the directions it moves in, and `sprites`, the directions its sprite shows.

## `Facing`

```cpp
enum class Facing : std::uint8_t { Up, UpRight, Right, DownRight, Down, DownLeft, Left, UpLeft };
```

One of eight directions, clockwise from up: the way a walker's sprite faces
(`Walker::spriteFacing`). A sheet with a row per sprite direction in this order reads its rows as
`static_cast<std::size_t>(facing)`.

## `WalkerConfig`

```cpp
struct DirectionalAnimations {
    retropp::Animation up, down, left, right;
    retropp::Animation upLeft, upRight, downLeft, downRight;
};

struct WalkerConfig {
    int speed = 1;                                   // pixels per tick on each axis headed along
    Ways movement = Ways::Eight;                     // 8-way or 4-way movement
    Ways sprites = Ways::Four;                       // 4 or 8 sprite directions
    CollisionShape collision{};                      // what the walker collides with
    std::span<const retropp::Anchor> anchors{};      // points in the walker's art
    DirectionalAnimations walk{};                    // the walk animation for each sprite direction
    retropp::TimingProfile timing = retropp::AnimationPlayer::defaultTiming;
};
```

| Field | Meaning |
|---|---|
| `speed` | The most pixels the walker moves along each axis it heads along per tick. It stops short when its collision shape would touch something solid. |
| `movement` | `Ways::Eight` moves along both axes at once; `Ways::Four` brings a heading along both axes to one first ([concepts.md](concepts.md#a-tick-of-walking)). Not read on a route. |
| `sprites` | `Ways::Four` shows the walker's facing; `Ways::Eight` shows the last of the eight directions it headed in, diagonals with their own animations ([concepts.md](concepts.md#sprite-directions)). |
| `collision` | The walker's collision shape, in its own pixels relative to its top-left: a box, a `ShapePoints`, or the polygon through anchors ([collision.md](collision.md#collisionshape)). An empty shape collides with nothing. |
| `anchors` | Points in the walker's art, published on its sprite and readable in world pixels with `Walker::anchor`. The table belongs to the game and must outlive the config. |
| `walk` | One animation per sprite direction. The diagonals are read only under `sprites = Ways::Eight`. The animation for the sprite's direction plays while the walker moves; its frame 0 shows while it stands. |
| `timing` | The cadence the animations' durations resolve against. The default is the engine's active cadence, which `EngineConfig::setActive` sets, as long as the config is built after it runs. |

```cpp
const TopDownMovement::WalkerConfig knightConfig{
    .speed = 1,
    .sprites = TopDownMovement::Ways::Eight,
    .collision = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8},
    .anchors = kKnightAnchors,
    .walk = {.up = walkUp, .down = walkDown, .left = walkLeft, .right = walkRight,
             .upLeft = walkUpLeft, .upRight = walkUpRight, .downLeft = walkDownLeft, .downRight = walkDownRight},
};
```

## `Walker`

```cpp
class Walker {
public:
    explicit Walker(retropp::Vec2i& position, retropp::Dir facing = retropp::Dir::Down) noexcept;

    void bind(retropp::Vec2i& position) noexcept;
    void place(retropp::Vec2i position) noexcept;

    template <SolidQuery Q>
    void walk(const WalkerConfig& config, Directions heading, const Q& solids);
    void moveTo(const WalkerConfig& config, retropp::Vec2i to, retropp::Vec2 travel);

    void face(retropp::Dir facing) noexcept;
    void face(Facing facing) noexcept;

    retropp::Vec2i position() const noexcept;
    retropp::Dir facing() const noexcept;
    Facing spriteFacing(const WalkerConfig& config) const noexcept;
    bool moving() const noexcept;
    CollisionShape collision(const WalkerConfig& config) const;
    retropp::Point anchor(const WalkerConfig& config, std::string_view label) const;
    const retropp::AnimationFrame& frame(const WalkerConfig& config) const;
};
```

| Member | What it does |
|---|---|
| `Walker(position, facing)` | A walker bound to `position`, its top-left in world pixels, which it reads and writes from then on. The position outlives the walker and stays at its address while it is bound. `Dir::None` reads as `Down`. |
| `bind(position)` | Binds the walker to another position. A copied walker walks the same position as the original until it is bound to its own. |
| `place(position)` | Writes `position` without walking there: nothing on the way collides, and the walker stands. For spawning, respawning and moving between maps. |
| `walk(config, heading, solids)` | One tick toward `heading`, as [concepts.md](concepts.md#a-tick-of-walking) describes. Call it once per tick, including ticks it heads nowhere, so the walker stands and its stride resets. |
| `moveTo(config, to, travel)` | One tick of being moved by something else: the walker is put at `to`, nothing colliding, and faces along `travel`, how far it moved this tick. The stride advances while `travel` is not zero and resets when it is; a zero `travel` leaves both facings. What `follow` calls ([routes.md](routes.md#facing-from-travel)). |
| `face(Dir)` | Turns the walker and its sprite to that direction without moving it. The stride keeps its place. `Dir::None` changes nothing. |
| `face(Facing)` | Turns the sprite to that direction. A diagonal leaves the walker's facing if it is one of the diagonal's two directions, and otherwise faces it the horizontal one. |
| `position()` | The position it is bound to. |
| `facing()` | `Up`, `Down`, `Left` or `Right`. |
| `spriteFacing(config)` | The direction its sprite shows under `config`: the facing under `sprites = Ways::Four`, the last of the eight directions it headed in under `Ways::Eight`. |
| `moving()` | Whether it moved on its last tick. |
| `collision(config)` | Its collision shape where it stands, in world pixels: `bounds()` for a rectangle around it, `covers(pixel)` for an exact test. |
| `anchor(config, label)` | The anchor labeled `label`, in world pixels. Throws `std::out_of_range` when the config has no such anchor. |
| `frame(config)` | The frame of the animation for its sprite's direction it shows now. Throws `std::logic_error` when that animation has no frames — under `sprites = Ways::Eight`, a diagonal left empty included. |

## `InputWalker`

```cpp
class InputWalker : public Walker {
public:
    explicit InputWalker(retropp::Vec2i& position, retropp::Dir facing = retropp::Dir::Down) noexcept;

    template <SolidQuery Q>
    void walk(const WalkerConfig& config, const DirectionalInput& input, const Q& solids);
};
```

A walker steered by the pad. It is a `Walker` that also keeps the order in which directions were pressed,
and it is walked by input alone: its `walk` takes a `DirectionalInput` and hides `Walker::walk`.

`walk` moves by the directions `input` holds, as a heading walker moves by its heading. With directions
held on both axes:

- under `Ways::Eight` it moves along both and keeps its facing if that is one of the two; otherwise it
  faces the one pressed more recently, and the horizontal one when both were pressed on the same tick;
- under `Ways::Four` it moves, and faces, along the one pressed more recently, and the horizontal one when
  both were pressed on the same tick. Letting go of it moves the walker along the other.

Everything else — `place`, `moveTo`, `face`, the readers — is `Walker`'s.

## Reading the directions

```cpp
struct Directions { bool up, down, left, right; };
struct DirectionalInput { Directions held; Directions pressed; };
struct DirectionalActions { ActionName up, down, left, right; };

DirectionalInput readDirections(const retropp::InputState& input, const DirectionalActions& actions);
```

`readDirections` reads which of the four actions `input` holds this tick (`isHeld`) and which were
pressed since the last (`justPressed`), for player slot 0. `ActionName` takes the game's own
enumerators, so the actions are written as they are named:

```cpp
const TopDownMovement::DirectionalInput move = TopDownMovement::readDirections(
    input, {.up = Action::Up, .down = Action::Down, .left = Action::Left, .right = Action::Right});
```

An `InputWalker` moves by `held` and uses `pressed` to know which direction came last. A `Directions` on
its own is a heading, for a plain `Walker`. A game whose simulation takes plain values reads the
directions at its input boundary and passes the `DirectionalInput` on, so its tick and its tests need no
engine input. The engine binds the actions to keys and buttons with an `ActionMap`;
`retropp::presets::directional` binds arrows, WASD and the d-pad in one call (the engine's
`docs/guide/input.md`).

## `WalkerSprite`

```cpp
retropp::Sprite WalkerSprite(retropp::ObjectKey key, const Walker& walker, const WalkerConfig& config);
```

| Sprite field | Value |
|---|---|
| `key` | `key` |
| `x`, `y` | the walker's position |
| `z` | the walker's y plus the bottom edge of its collision shape (plus the frame's height, for a walker with no collision shape) |
| `size`, `atlas`, `tile`, `palette` | the current frame's: the animation for the sprite's direction |
| `anchors` | `config.anchors` |

Every other field is the engine's default. It takes any walker — an `InputWalker` and a walker on a route
draw the same way. The sprite resolves the frame's art through the renderer, so a renderer exists before
it is called, as it does for any `AnimationFrame::tile()`. Throws `std::logic_error` when the frame names
no art, and as `Walker::frame` does.

The key names the walker for the engine, which eases a sprite from one tick to the next by its key:
a literal for a singleton (`"player"`), an id minted at spawn for anything that comes and goes
(the engine's `docs/guide/sprites.md`, "Identity — key").
