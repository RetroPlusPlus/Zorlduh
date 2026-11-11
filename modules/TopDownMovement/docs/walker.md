# Walker reference

`WalkerConfig`, `DirectionalAnimations`, `Walker`, the direction types and `readDirections`, and
`WalkerSprite`.

```cpp
#include "TopDownMovement/walker.h"         // DirectionalAnimations, WalkerConfig, Walker
#include "TopDownMovement/directions.h"     // Directions, DirectionalInput, DirectionalActions, readDirections
#include "TopDownMovement/walker_sprite.h"  // WalkerSprite
```

## Contents

- [`WalkerConfig`](#walkerconfig)
- [`Walker`](#walker)
- [Reading the directions](#reading-the-directions)
- [`WalkerSprite`](#walkersprite)

## `WalkerConfig`

```cpp
struct DirectionalAnimations {
    retropp::Animation up, down, left, right;
};

struct WalkerConfig {
    int speed = 1;                                   // pixels per tick on each axis held
    CollisionShape collision{};                      // what the walker collides with
    std::span<const retropp::Anchor> anchors{};      // points in the walker's art
    DirectionalAnimations walk{};                    // the walk animation for each facing
    retropp::TimingProfile timing = retropp::AnimationPlayer::defaultTiming;
};
```

| Field | Meaning |
|---|---|
| `speed` | The most pixels the walker moves along each held axis per tick. It stops short when its collision shape would touch something solid. |
| `collision` | The walker's collision shape, in its own pixels relative to its top-left: a box, a `ShapePoints`, or the polygon through anchors ([collision.md](collision.md#collisionshape)). An empty shape collides with nothing. |
| `anchors` | Points in the walker's art, published on its sprite and readable in world pixels with `Walker::anchor`. The table belongs to the game and must outlive the config. |
| `walk` | One animation per facing. The facing's animation plays while the walker moves; its frame 0 shows while it stands. |
| `timing` | The cadence the animations' durations resolve against. The default is the engine's active cadence, which `EngineConfig::setActive` sets, as long as the config is built after it runs. |

```cpp
const TopDownMovement::WalkerConfig hero{
    .speed = 1,
    .collision = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8},
    .anchors = kHeroAnchors,
    .walk = {.up = walkUp, .down = walkDown, .left = walkLeft, .right = walkRight},
};
```

## `Walker`

```cpp
class Walker {
public:
    Walker();                                                    // at (0, 0), facing down
    explicit Walker(retropp::Vec2i position, retropp::Dir facing = retropp::Dir::Down);

    void place(retropp::Vec2i position) noexcept;

    template <SolidQuery Q>
    void walk(const WalkerConfig& config, const DirectionalInput& input, const Q& solids);

    retropp::Vec2i position() const noexcept;
    retropp::Dir facing() const noexcept;
    bool moving() const noexcept;
    CollisionShape collision(const WalkerConfig& config) const;
    retropp::Point anchor(const WalkerConfig& config, std::string_view label) const;
    const retropp::AnimationFrame& frame(const WalkerConfig& config) const;
};
```

| Member | What it does |
|---|---|
| `Walker(position, facing)` | A walker with its top-left at `position`, in world pixels. `Dir::None` reads as `Down`. |
| `place(position)` | Puts the walker at `position` without walking there: nothing on the way collides, and it stands. For spawning, respawning and moving between maps. |
| `walk(config, input, solids)` | One tick of walking, as [concepts.md](concepts.md#a-tick-of-walking) describes. Call it once per tick, including ticks with nothing held, so the walker stands and its stride resets. |
| `position()` | The walker's top-left, in world pixels. |
| `facing()` | `Up`, `Down`, `Left` or `Right`. |
| `moving()` | Whether it moved on its last `walk`. |
| `collision(config)` | Its collision shape where it stands, in world pixels: `bounds()` for a rectangle around it, `covers(pixel)` for an exact test. |
| `anchor(config, label)` | The anchor labeled `label`, in world pixels. Throws `std::out_of_range` when the config has no such anchor. |
| `frame(config)` | The frame of its facing's animation it shows now. Throws `std::logic_error` when that animation has no frames. |

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

`walk` moves by `held` and uses `pressed` to know which direction came last. A game whose simulation
takes plain values reads the directions at its input boundary and passes the `DirectionalInput` on, so
its tick and its tests need no engine input. The engine binds the actions to keys and buttons with an
`ActionMap`; `retropp::presets::directional` binds arrows, WASD and the d-pad in one call (the engine's
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
| `size`, `atlas`, `tile`, `palette` | the current frame's |
| `anchors` | `config.anchors` |

Every other field is the engine's default. The sprite resolves the frame's art through the renderer, so a
renderer exists before it is called, as it does for any `AnimationFrame::tile()`. Throws
`std::logic_error` when the frame names no art, and as `Walker::frame` does.

The key names the walker for the engine, which eases a sprite from one tick to the next by its key:
a literal for a singleton (`"player"`), an id minted at spawn for anything that comes and goes
(the engine's `docs/guide/sprites.md`, "Identity — key").
