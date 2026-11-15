# Walker reference

Every type and function in `directions.h`, `walker.h`, `input_walker.h` and `walker_sprite.h`. For
routes see [routes.md](routes.md). For collision types see [collision.md](collision.md).

```cpp
#include "TopDownMovement/directions.h"     // Ways, Facing, Directions, DirectionalInput, ActionName, DirectionalActions, readDirections
#include "TopDownMovement/walker.h"         // DirectionalAnimations, WalkerConfig, Walker
#include "TopDownMovement/input_walker.h"   // InputWalker
#include "TopDownMovement/walker_sprite.h"  // WalkerSprite
```

`input_walker.h` includes `walker.h`, which includes `directions.h` and `collision.h`.

## Contents

- [`Ways`](#ways)
- [`Facing`](#facing)
- [`Directions`](#directions)
- [`DirectionalInput`](#directionalinput)
- [`ActionName` and `DirectionalActions`](#actionname-and-directionalactions)
- [`readDirections`](#readdirections)
- [`DirectionalAnimations`](#directionalanimations)
- [`WalkerConfig`](#walkerconfig)
- [`Walker`](#walker)
- [`InputWalker`](#inputwalker)
- [Writing your own pathway](#writing-your-own-pathway)
- [`WalkerSprite`](#walkersprite)

## `Ways`

```cpp
enum class Ways : std::uint8_t { Four, Eight };
```

A number of directions: the four cardinal ones, or those plus the four diagonals. `WalkerConfig` has two
fields of this type, `movement` and `sprites`.

## `Facing`

```cpp
enum class Facing : std::uint8_t { Up, UpRight, Right, DownRight, Down, DownLeft, Left, UpLeft };
```

One of eight directions, clockwise from up. `Walker::spriteFacing` returns it, and `Walker::face` takes
it.

The values are 0 to 7 in the order listed. A sheet with one row per direction in this order can use the
value as the row number:

```cpp
const std::size_t row = static_cast<std::size_t>(TopDownMovement::Facing::DownRight);   // 3
```

## `Directions`

```cpp
struct Directions {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;

    constexpr bool operator==(const Directions&) const noexcept = default;
};
```

Four flags. It is used in two places:

- As a **heading** for `Walker::walk`: the directions the walker should move in this tick.
- Inside `DirectionalInput`: the directions held, and the directions pressed.

```cpp
const TopDownMovement::Directions upRight{.up = true, .right = true};
const TopDownMovement::Directions nowhere{};
```

As a heading, opposite flags cancel. `{.left = true, .right = true}` moves nowhere on x.

## `DirectionalInput`

```cpp
struct DirectionalInput {
    Directions held{};
    Directions pressed{};

    constexpr bool operator==(const DirectionalInput&) const noexcept = default;
};
```

One tick of direction input for `InputWalker::walk`.

| Field | Meaning |
|---|---|
| `held` | The directions down on this tick. The walker moves by these. |
| `pressed` | The directions that went down since the previous tick. The walker uses these to know which of two held directions is newer. |

`readDirections` builds one from the engine's input. You can also build one yourself
([how-to.md](how-to.md#build-a-directionalinput-by-hand)).

## `ActionName` and `DirectionalActions`

```cpp
struct ActionName {
    retropp::ActionId id = 0;

    template <retropp::ActionLike A>
    constexpr ActionName(A action);
};

struct DirectionalActions {
    ActionName up;
    ActionName down;
    ActionName left;
    ActionName right;
};
```

`DirectionalActions` names the four actions in your game's action enum that mean up, down, left and
right. `ActionName` converts implicitly from any enumerator or integer the engine accepts as an action,
so you write your own enumerators:

```cpp
enum class Action : std::uint8_t { Up, Down, Left, Right, Interact };

const TopDownMovement::DirectionalActions steering{
    .up = Action::Up, .down = Action::Down, .left = Action::Left, .right = Action::Right};
```

Build it once and pass it to `readDirections` each tick.

## `readDirections`

```cpp
DirectionalInput readDirections(const retropp::InputState& input, const DirectionalActions& actions);
```

Reads the four actions from the tick's input: `held` from `InputState::isHeld`, `pressed` from
`InputState::justPressed`.

```cpp
loop.simTick([&](const retropp::InputState& input) {
    hero.walk(heroConfig, TopDownMovement::readDirections(input, steering), walls);
});
```

- **It reads player slot 0.** For another slot, build the `DirectionalInput` from
  `input.player(n)` ([how-to.md](how-to.md#build-a-directionalinput-by-hand)).
- **The actions must be bound.** The engine reports nothing for an action that has no binding in the
  `ActionMap` you gave the platform. `retropp::presets::directional(up, down, left, right)` binds the
  arrow keys, WASD and the d-pad (the engine's `docs/guide/input.md`).
- **Simulation code that takes plain values** can call `readDirections` where it reads input and pass the
  `DirectionalInput` on. The simulation and its tests then need no `InputState`.

## `DirectionalAnimations`

```cpp
struct DirectionalAnimations {
    retropp::Animation up;
    retropp::Animation down;
    retropp::Animation left;
    retropp::Animation right;
    retropp::Animation upLeft;
    retropp::Animation upRight;
    retropp::Animation downLeft;
    retropp::Animation downRight;
};
```

One walk animation per sprite direction. It is the type of `WalkerConfig::walk`.

| `config.sprites` | Fields read |
|---|---|
| `Ways::Four` | `up`, `down`, `left`, `right` |
| `Ways::Eight` | all eight |

Frame 0 of each animation is the standing pose for that direction. An animation that is read and has no
frames makes `Walker::frame` throw.

## `WalkerConfig`

```cpp
struct WalkerConfig {
    int speed = 1;
    Ways movement = Ways::Eight;
    Ways sprites = Ways::Four;
    CollisionShape collision{};
    std::span<const retropp::Anchor> anchors{};
    DirectionalAnimations walk{};
    retropp::TimingProfile timing = retropp::AnimationPlayer::defaultTiming;
};
```

Describes a kind of walker. Build one per kind of character, after the art is loaded, and pass it to
every call that takes a config.

| Field | Meaning | Default |
|---|---|---|
| `speed` | Pixels moved per tick on each axis the walker heads along. The walker stops short at a wall. | `1` |
| `movement` | `Ways::Eight` moves on both axes in one tick. `Ways::Four` moves on one ([concepts.md](concepts.md#four-directions-only)). Not read by `moveTo` or `follow`. | `Ways::Eight` |
| `sprites` | `Ways::Four`: the sprite shows `facing()`. `Ways::Eight`: the sprite shows one of eight directions, each with its own animation ([concepts.md](concepts.md#sprite-directions)). | `Ways::Four` |
| `collision` | The walker's collision shape, in the walker's own pixels relative to its top-left ([collision.md](collision.md#collisionshape)). | empty |
| `anchors` | Named points in the walker's art. Read them with `Walker::anchor`; `WalkerSprite` copies the span onto the sprite. | empty |
| `walk` | The walk animations. | empty |
| `timing` | The tick rate used to convert frame durations to ticks. | the engine's active timing when the config is constructed |

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

Watch for:

- **Initializer order.** C++ requires designated initializers in declaration order: `speed`, `movement`,
  `sprites`, `collision`, `anchors`, `walk`, `timing`.
- **An empty `collision`.** A walker with no collision shape collides with nothing and walks through
  walls.
- **`speed` of 0 or less.** The walker turns to face its heading and does not move.
- **`timing`.** The default is read when the config is constructed. Construct configs after
  `retropp::EngineConfig::setActive`, or set `timing` yourself.
- **`anchors` is a span.** The table it views must outlive the config.
- **Copying.** A config owns its animations, so copying a config copies them. Pass configs by reference.

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

The state of one walking character: the position it is bound to, its facing, its sprite's facing, and
its place in the walk animation. A `Walker` has no default constructor; it is always bound to a
position.

### Constructor

```cpp
explicit Walker(retropp::Vec2i& position, retropp::Dir facing = retropp::Dir::Down) noexcept;
```

Binds the walker to `position`, the walker's top-left in world pixels, and sets its starting facing.
`retropp::Dir::None` is treated as `Down`.

```cpp
retropp::Vec2i batAt{40, 40};
TopDownMovement::Walker bat{batAt, retropp::Dir::Left};
```

`position` must outlive the walker and stay at the same address
([concepts.md](concepts.md#lifetimes)).

### `bind`

```cpp
void bind(retropp::Vec2i& position) noexcept;
```

Binds the walker to a different position. Facing and stride are unchanged.

Call it on a copied walker. A copy is bound to the same position as the original:

```cpp
TopDownMovement::Walker ghost = bat;   // ghost is bound to batAt
retropp::Vec2i ghostAt = batAt;
ghost.bind(ghostAt);                   // ghost now has its own position
```

### `place`

```cpp
void place(retropp::Vec2i position) noexcept;
```

Writes `position` to the bound position and resets the stride, so the walker shows frame 0. Collision is
not tested. Facing is unchanged.

Use it to spawn, respawn, or move a walker to another map.

Do not call it every tick to keep a walker in place. It resets the stride each time, so the walk
animation stays on frame 0. To change a walker's position without resetting the stride, assign to the
position itself.

### `walk`

```cpp
template <SolidQuery Q>
void walk(const WalkerConfig& config, Directions heading, const Q& solids);
```

One tick of walking toward `heading`, colliding with `solids`. It moves the walker, sets its facing and
advances or resets its stride. [concepts.md](concepts.md#a-tick-of-walking) describes the movement and
[concepts.md](concepts.md#facing) the facing.

```cpp
bat.walk(batConfig, TopDownMovement::Directions{.left = true}, walls);
```

- Call it once per tick, including ticks with an empty heading. An empty heading is what makes the
  walker stand and show frame 0.
- Calling it twice in one tick moves and animates the walker at double speed.
- `solids` is not stored. You can pass a different one on each call.

### `moveTo`

```cpp
void moveTo(const WalkerConfig& config, retropp::Vec2i to, retropp::Vec2 travel);
```

One tick of being moved by something other than `walk`. It writes `to` to the position without testing
collision, and uses `travel` — the distance moved this tick, as a float vector — to set the facing and
the stride.

| `travel` | Facing | Stride |
|---|---|---|
| non-zero | set from the direction of `travel` ([routes.md](routes.md#facing-from-travel)) | advances |
| zero | unchanged | reset; the walker shows frame 0 |

`follow` calls it for routes. Call it yourself to drive a walker from your own mover
([how-to.md](how-to.md#move-a-walker-with-your-own-mover)):

```cpp
ghost.moveTo(ghostConfig, {40, 41}, {0.0f, 0.5f});   // at (40, 41), facing down, walking
```

`travel` can be smaller than a pixel and `to` can equal the current position. The stride still
advances, so a slow mover animates.

### `face`

```cpp
void face(retropp::Dir facing) noexcept;
void face(Facing facing) noexcept;
```

Turns the walker without moving it. The stride is unchanged.

| Overload | `facing()` becomes | `spriteFacing` becomes |
|---|---|---|
| `face(retropp::Dir)` | the direction given | the same direction |
| `face(Facing)`, cardinal | that direction | that direction |
| `face(Facing)`, diagonal | unchanged if it is one of the diagonal's two directions, otherwise the horizontal one | the diagonal |

```cpp
guard.face(retropp::Dir::Down);
knight.face(TopDownMovement::Facing::UpLeft);
```

`face(retropp::Dir::None)` does nothing. A walker that is walked turns again on its next tick with a
heading.

### `position`

```cpp
retropp::Vec2i position() const noexcept;
```

The bound position: the walker's top-left in world pixels. It is the same value as the `Vec2i` you
bound.

### `facing`

```cpp
retropp::Dir facing() const noexcept;
```

`Up`, `Down`, `Left` or `Right`. Use it for game logic.

### `spriteFacing`

```cpp
Facing spriteFacing(const WalkerConfig& config) const noexcept;
```

The direction the sprite shows.

| `config.sprites` | Returns |
|---|---|
| `Ways::Four` | `facing()`, as a `Facing` |
| `Ways::Eight` | the last of the eight directions the walker headed in, was moved in, or was turned to |

### `moving`

```cpp
bool moving() const noexcept;
```

Whether the walk animation is running.

| After | `moving()` |
|---|---|
| a `walk` that moved the walker on at least one axis | `true` |
| a `walk` with an empty heading, or blocked on every axis | `false` |
| a `moveTo` with non-zero `travel` | `true`, even if the position did not change |
| a `moveTo` with zero `travel` | `false` |
| `place` | `false` |

Use it to detect a blocked walker:

```cpp
wanderer.walk(wandererConfig, heading, walls);
if (!wanderer.moving()) {
    // blocked: pick a new heading
}
```

### `collision`

```cpp
CollisionShape collision(const WalkerConfig& config) const;
```

The walker's collision shape moved to its position, in world pixels. Use `bounds()` for its bounding
rectangle and `covers(pixel)` for an exact test.

It returns a copy of the shape, which allocates. For the bounding rectangle alone, offset
`config.collision.bounds()` by `position()` instead.

### `anchor`

```cpp
retropp::Point anchor(const WalkerConfig& config, std::string_view label) const;
```

The anchor named `label` from `config.anchors`, in world pixels: the anchor's position in the art plus
the walker's position.

```cpp
const retropp::Point hand = hero.anchor(heroConfig, "hand");
```

Throws `std::out_of_range` if `config.anchors` has no anchor with that label.

### `frame`

```cpp
const retropp::AnimationFrame& frame(const WalkerConfig& config) const;
```

The frame the walker shows now: the animation for `spriteFacing(config)`, at the walker's place in its
stride, looping. A walker that is not moving returns frame 0.

The reference points into `config.walk`. It is valid while the config is.

Throws `std::logic_error` if that animation has no frames. With `sprites = Ways::Eight` this includes a
diagonal animation left empty.

`WalkerSprite` calls it. Call it yourself to read the frame's `label` or `tileIndex`, which needs no
renderer.

## `InputWalker`

```cpp
class InputWalker : public Walker {
public:
    explicit InputWalker(retropp::Vec2i& position, retropp::Dir facing = retropp::Dir::Down) noexcept;

    template <SolidQuery Q>
    void walk(const WalkerConfig& config, const DirectionalInput& input, const Q& solids);
};
```

A `Walker` steered by a pad or keyboard. It also records the tick each direction was last pressed on.
Use it for the player.

```cpp
retropp::Vec2i heroAt{72, 56};
TopDownMovement::InputWalker hero{heroAt};

hero.walk(heroConfig, TopDownMovement::readDirections(input, steering), walls);   // once per tick
```

`walk` moves by `input.held`, as `Walker::walk` moves by a heading. The press order decides the cases a
heading cannot:

| `config.movement` | Two axes held |
|---|---|
| `Ways::Eight` | Moves on both. Keeps its facing if that is one of the two directions. Otherwise faces the direction pressed more recently, or the horizontal one if both were pressed on the same tick. |
| `Ways::Four` | Moves and faces along the direction pressed more recently, or the horizontal one if both were pressed on the same tick. When that direction is released, moves along the other. |

- `InputWalker::walk` hides `Walker::walk`. An `InputWalker` does not take a `Directions` heading.
- Every other member is inherited from `Walker`.
- An `InputWalker` can follow a route ([how-to.md](how-to.md#walk-the-player-along-a-route)).
- The press order is recorded in `walk`. Presses on ticks where `walk` is not called are not recorded.

## Writing your own pathway

`Walker` has three protected members for a derived class that decides direction its own way, as
`InputWalker` does.

```cpp
protected:
    template <SolidQuery Q>
    void stepToward(const WalkerConfig& config, int dx, int dy, retropp::Dir diagonal, const Q& solids);

    static retropp::Dir horizontalOf(int dx) noexcept;   // Right if dx > 0, otherwise Left
    static retropp::Dir verticalOf(int dy) noexcept;     // Down if dy > 0, otherwise Up
```

`stepToward` is one tick of walking toward `(dx, dy)`, where each is -1, 0 or 1. It moves the walker
against `solids`, sets the sprite's facing, and advances or resets the stride. It sets `facing()` this
way:

| `dx`, `dy` | `facing()` |
|---|---|
| both 0 | unchanged |
| one non-zero | that direction |
| both non-zero | unchanged if it is one of the two directions, otherwise `diagonal` |

`stepToward` does not read `config.movement`. A derived class that supports `Ways::Four` sets one of
`dx` and `dy` to 0 before calling it.

A walker steered by an analog stick:

```cpp
class StickWalker : public TopDownMovement::Walker {
public:
    using TopDownMovement::Walker::Walker;

    // One tick of walking by `stick`, each axis in [-1, 1]. An axis past 0.5 heads that way. When both
    // are, the walker faces the axis pushed further.
    template <TopDownMovement::SolidQuery Q>
    void walk(const TopDownMovement::WalkerConfig& config, retropp::Vec2 stick, const Q& solids) {
        int dx = stick.x > 0.5f ? 1 : stick.x < -0.5f ? -1 : 0;
        int dy = stick.y > 0.5f ? 1 : stick.y < -0.5f ? -1 : 0;
        const bool vertical = std::abs(stick.y) > std::abs(stick.x);
        if (config.movement == TopDownMovement::Ways::Four && dx != 0 && dy != 0) {
            (vertical ? dx : dy) = 0;
        }
        stepToward(config, dx, dy, vertical ? verticalOf(dy) : horizontalOf(dx), solids);
    }
};
```

## `WalkerSprite`

```cpp
retropp::Sprite WalkerSprite(retropp::ObjectKey key, const Walker& walker, const WalkerConfig& config);
```

Returns the sprite for a walker. Call it every frame and add the result to a sprite layer.

```cpp
sprites = {
    TopDownMovement::WalkerSprite("hero", hero, heroConfig),
    TopDownMovement::WalkerSprite("guard", guard, guardConfig),
};
```

| Sprite field | Value |
|---|---|
| `key` | `key` |
| `x`, `y` | the walker's position |
| `z` | the walker's `y` plus the bottom edge of `config.collision`. With an empty collision shape, `y` plus the frame's height. |
| `size`, `atlas`, `tile`, `palette` | from `walker.frame(config)` |
| `anchors` | `config.anchors` |
| every other field | the engine's default |

- **It needs a renderer.** The frame's `atlas`, `tile` and `size` are resolved through the renderer that
  loaded the sheet. Everything else in the module works without one.
- **The key must be unique in the frame and stable for the character.** The engine matches sprites
  between ticks by key (the engine's `docs/guide/sprites.md`, "Identity — `key`").
- **`z` sorts walkers on one layer.** A walker lower on the screen has a higher `z` and draws in front.

Throws `std::logic_error` if the frame names no art (a palette-only frame), and when `Walker::frame`
throws.
