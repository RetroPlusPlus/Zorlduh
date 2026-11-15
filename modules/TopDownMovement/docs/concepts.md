# Concepts

How the module works: the five things you write to put a walking character on screen, what each one is
for, and how a walker behaves at walls, on diagonals and on routes.

## Contents

- [Overview](#overview)
- [The config and the walker](#the-config-and-the-walker)
- [The position a walker is bound to](#the-position-a-walker-is-bound-to)
- [Three pathways](#three-pathways)
- [A tick of walking](#a-tick-of-walking)
- [Facing](#facing)
- [Sprite directions](#sprite-directions)
- [The walk animation](#the-walk-animation)
- [Drawing a walker](#drawing-a-walker)
- [Where each part lives in a game](#where-each-part-lives-in-a-game)
- [Lifetimes](#lifetimes)

## Overview

```cpp
#include "TopDownMovement/input_walker.h"    // InputWalker, WalkerConfig, TileCollision, readDirections
#include "TopDownMovement/walker_sprite.h"   // WalkerSprite

// 1. The config: what a hero is. Build it once, after the art is loaded.
const TopDownMovement::WalkerConfig heroConfig{
    .speed     = 1,                                                           // pixels per tick
    .collision = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8},  // its feet, in a 16×16 frame
    .walk      = {.up = walkUp, .down = walkDown, .left = walkLeft, .right = walkRight},
};

// 2. The position: a value in your game state.
retropp::Vec2i heroAt{72, 56};

// 3. The walker, bound to that position.
TopDownMovement::InputWalker hero{heroAt};

// 4. Every tick: walk it by the pad, against the walls.
const TopDownMovement::TileCollision walls{.map = &collisionMap, .cellSize = 8, .open = Cell::Open};
hero.walk(heroConfig, TopDownMovement::readDirections(input, steering), walls);

// 5. Every frame: draw it.
const retropp::Sprite sprite = TopDownMovement::WalkerSprite("hero", hero, heroConfig);
```

Names from your game:

| Name | What it is |
|---|---|
| `walkUp`, `walkDown`, `walkLeft`, `walkRight` | `retropp::Animation`s built from your sheet |
| `collisionMap` | a `retropp::IndexGrid`; cells equal to `Cell::Open` can be walked on |
| `input` | the tick's `retropp::InputState` |
| `steering` | a `TopDownMovement::DirectionalActions` naming your four direction actions |

[getting-started.md](getting-started.md) builds all of these in a program you can run.

The result is a hero that walks in eight directions, stops at walls, slides along them, faces the
direction it walks in, and plays a walk animation while it moves. The sections below cover each step.

## The config and the walker

Each character has two values.

- A **`WalkerConfig`** describes a kind of character: speed, 8-way or 4-way movement, 4 or 8 sprite
  directions, collision shape, walk animations. Build it once, after the art is loaded.
- A **`Walker`** is the state of one character: which way it faces and where it is in its stride. It
  changes every tick. Keep it in your game state.

One config serves any number of walkers. Forty bats use one bat config and forty walkers. Calls that
need both take both:

```cpp
bat.walk(batConfig, heading, walls);
const retropp::Sprite sprite = TopDownMovement::WalkerSprite("bat", bat, batConfig);
```

## The position a walker is bound to

A walker reads and writes a `retropp::Vec2i` that your game owns. It stores no position of its own. You
bind the walker to yours when you construct the walker:

```cpp
retropp::Vec2i heroAt{72, 56};
TopDownMovement::InputWalker hero{heroAt};
```

After this, `heroAt` and `hero.position()` are the same value. Walking the hero changes `heroAt`.
Assigning to `heroAt` moves the hero.

The position is yours because your game already needs it in its own state: the camera, combat and save
code read it there. Binding avoids a second copy that would have to be kept in sync.

The binding is by address:

- **The position must not move in memory** while a walker is bound to it, and it must outlive the
  walker. See [Lifetimes](#lifetimes) for the `std::vector` case.
- **A copied walker is bound to the original's position.** After copying a walker, call `bind` on the
  copy to give it its own position. [how-to.md](how-to.md#keep-a-walker-and-its-position-in-one-value) has a struct that does this automatically.

## Three pathways

There are three ways to move a walker. Choose by what decides its direction.

| Use it for | Direction comes from | Call each tick |
|---|---|---|
| The player | the pad | `InputWalker::walk(config, input, solids)` |
| An enemy that chases, flees or wanders | a heading your code computes | `Walker::walk(config, heading, solids)` |
| A patrol or a scripted walk | a route | `follow(walker, config, route)` |

```cpp
hero.walk(heroConfig, TopDownMovement::readDirections(input, steering), walls);   // the pad
bat.walk(batConfig, TopDownMovement::Directions{.left = true}, walls);            // a heading
TopDownMovement::follow(guard, guardConfig, patrol);                              // a route
```

- **Input.** `InputWalker` is a `Walker` that also records the order directions were pressed in. It uses
  that order when two directions are held.
- **Free.** A plain `Walker` takes a heading: a `Directions` value with four flags. It collides with
  walls the same way the player does.
- **Route.** `follow` moves a walker along a `retropp::SpritePath`, the engine's route type, which has
  legs, waits and detours ([routes.md](routes.md)). A walker on a route does not collide with anything.
  A route can move slower than one pixel per tick; the other two pathways cannot.

If the character must be stopped by walls, use input or a heading. If its path is fixed in advance and
runs through open ground, use a route. A walker can switch pathways from one tick to the next.

## A tick of walking

```cpp
walker.walk(config, heading, solids);
```

- `heading` is a `TopDownMovement::Directions`: the flags `up`, `down`, `left` and `right`.
- `solids` answers whether a rectangle of pixels touches something solid. It is usually a
  `TileCollision` over your collision map ([collision.md](collision.md)).

Each tick the walker moves along x, then along y, up to `config.speed` pixels on each. It moves one
pixel at a time and tests its collision shape before taking each pixel.

| Heading | Result |
|---|---|
| right, open floor | Moves `speed` pixels right. |
| right, `speed` 3, wall 2 pixels ahead | Moves 2 pixels and stops against the wall. |
| down-right, wall below | Moves right along the wall. |
| down-right, into a corner | Does not move. |
| left and right together | The two cancel. No movement on x. |

Notes:

- The axes are tested separately, so a wall that blocks one axis does not block the other.
- Every pixel is tested, so a walker does not pass through a thin wall at any speed.
- `speed` is whole pixels per tick. For slower movement see
  [how-to.md](how-to.md#walk-slower-than-a-pixel-a-tick).
- A diagonal moves `speed` pixels on both axes, so it covers more distance per tick than a straight
  line.

### Four directions only

With `config.movement = TopDownMovement::Ways::Four` the walker moves along one axis per tick. When two
axes are requested, it chooses one:

| Walker | Axis it takes |
|---|---|
| `InputWalker` | The direction pressed more recently. When that one is released, the other. |
| `Walker` with a heading | The direction it faces, if that is one of the two. Otherwise the horizontal one. |

The default is `Ways::Eight`. Routes do not read `movement`; a route can move diagonally under either
setting.

## Facing

A walker faces one of four directions: `retropp::Dir::Up`, `Down`, `Left` or `Right`. Read it with
`walker.facing()`. Use it for game logic that depends on direction, such as what the character interacts
with or which way it attacks.

Rules:

- Facing follows the heading even when the walker cannot move. A walker heading into a wall turns to
  face the wall.
- With no heading, the facing does not change.
- With a heading on one axis, the walker faces that direction.
- With a heading on both axes, the walker keeps its current facing if that is one of the two
  directions. If it is neither, an `InputWalker` faces the direction pressed more recently (the
  horizontal one if both were pressed on the same tick), and a plain `Walker` faces the horizontal one.

Examples with an `InputWalker`:

| Input | Facing |
|---|---|
| Hold Right, then also hold Up | right |
| Hold Up, then also hold Right | up |
| Hold Up and Right while facing right, then release Right and press Left | left (pressed more recently than Up) |
| Facing down, press Up and Left on the same tick | left (horizontal on a tie) |

On a route, a walker faces the axis it travels further along each tick
([routes.md](routes.md#facing-from-travel)).

To turn a walker without moving it, call `walker.face(retropp::Dir::Left)`.

## Sprite directions

Movement directions and sprite directions are set separately, with `config.movement` and
`config.sprites`. Each is `Ways::Four` or `Ways::Eight`.

| | `sprites = Ways::Four` (default) | `sprites = Ways::Eight` |
|---|---|---|
| **`movement = Ways::Eight`** (default) | Moves in 8 directions. The sprite shows the walker's facing, so on a diagonal it shows one of the two cardinal directions. Needs 4 walk animations. | Moves in 8 directions. Each diagonal shows its own walk animation. Needs 8. |
| **`movement = Ways::Four`** | Moves in 4 directions and shows 4. | Moves in 4 directions. The diagonal animations show only on a route or after `face(Facing)`. |

With `sprites = Ways::Eight` a walker has two facings:

| Call | Returns | Use it for |
|---|---|---|
| `walker.facing()` | one of four `retropp::Dir` values | game logic |
| `walker.spriteFacing(config)` | one of eight `TopDownMovement::Facing` values: the last direction the walker headed in | choosing art |

A walker that stops after moving down-left keeps `Facing::DownLeft` and shows frame 0 of the down-left
animation. With `sprites = Ways::Four`, `spriteFacing` returns the same direction as `facing`.

## The walk animation

`config.walk` holds one `retropp::Animation` per sprite direction: `up`, `down`, `left`, `right`, and
with `sprites = Ways::Eight` also `upLeft`, `upRight`, `downLeft`, `downRight`. They are ordinary engine
animations built from your sheet (the engine's `docs/guide/animation.md`).

The walker selects and advances the animation. You do not start or stop it.

| Walker state | Frame shown |
|---|---|
| Moving | The animation for the sprite's direction, looping. |
| Not moving (no heading, or blocked on every axis it heads along) | Frame 0 of that animation. |
| Turns while moving | The new direction's animation, at the same point in the stride. |
| Sliding along a wall | The animation keeps playing. |

The walker keeps one tick count for its stride and uses it for whichever direction is showing. Give
every direction the same frame durations so a turn does not jump to a different point in the step.

Frame durations are real time. They are converted to ticks using `config.timing`, which defaults to
the engine's active timing at the moment the config is constructed. Construct configs after calling
`retropp::EngineConfig::setActive`.

## Drawing a walker

```cpp
const retropp::Sprite sprite = TopDownMovement::WalkerSprite("hero", hero, heroConfig);
```

`WalkerSprite` returns a `retropp::Sprite` showing the walker's current frame, with the frame's top-left
at the walker's position. It works the same for all three pathways. Add the sprite to a sprite layer.

- **Position.** The sprite is in world pixels, like the walker's position. Put it on a layer that
  scrolls with the camera.
- **Depth.** The sprite's `z` is the bottom edge of the walker's collision shape. On a layer of walkers,
  the one lower on the screen draws in front.
- **Key.** The first argument is the sprite's key, which the engine uses to match the sprite between
  ticks. Use a fixed string for a character there is one of, and an id assigned at spawn for characters
  that are created and removed (the engine's `docs/guide/sprites.md`, "Identity — `key`").

## Where each part lives in a game

| Part | When you create or call it | Where it goes |
|---|---|---|
| `WalkerConfig` | once, after loading the art | with your loaded data |
| the position | when the character spawns | game state |
| `Walker`, `InputWalker` | when the character spawns, bound to the position | game state, next to the position |
| a route (`retropp::SpritePath`) | when the walker starts following it | game state |
| `readDirections(…)` | every tick | where your game reads input |
| `walk(…)`, `follow(…)` | every tick | the tick |
| `WalkerSprite(…)` | every frame | drawing code |

`walker_sprite.h` is the only header that creates a draw type. Simulation code includes `walker.h`,
`input_walker.h` and `route.h` and needs no renderer.

## Lifetimes

What the module keeps a pointer or a view to, and what it does not:

| Object | Rule |
|---|---|
| The position | Must stay at the same address while a walker is bound to it. |
| A `Walker` | A copy is bound to the same position as the original until you call `bind`. |
| A `WalkerConfig` | Passed to each call. The walker stores nothing from it. The config holds its own copies of the animations. |
| `config.anchors` | A span over your anchor table. The table must outlive the config. |
| `TileCollision::map` | A pointer to your `IndexGrid`. The map must outlive the `TileCollision`. |
| The `solids` argument | Used during the call only. |
| A route | A value you own. `follow` stores nothing from it. |

The common mistake is binding walkers to positions in a `std::vector` that later grows. Growth moves the
elements, and the walkers are left bound to freed memory:

```cpp
std::vector<retropp::Vec2i> positions;
std::vector<TopDownMovement::Walker> bats;

positions.push_back({40, 40});
bats.emplace_back(positions.back());
positions.push_back({80, 40});          // the vector grows: the first position moves
bats.emplace_back(positions.back());    // the first bat is now bound to freed memory
```

Two ways to avoid it:

- Keep each walker and its position in one struct that calls `bind` when copied
  ([how-to.md](how-to.md#keep-a-walker-and-its-position-in-one-value)). This is safe in a vector.
- Keep the positions in a container that does not move them: a vector with `reserve` called up front,
  or a `std::deque` that is only added to at its ends.
