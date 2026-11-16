# Routes

`follow` moves a walker along a `retropp::SpritePath`, the engine's route type. Use a route for movement
that is decided in advance: a patrol, a scripted walk, a detour and return.

The engine provides the route: its geometry, pacing, waits, playback modes and interrupt stack. This
module provides the walker on it: the position, a facing chosen from the route's travel, and the walk
animation.

A walker on a route goes exactly where the route goes, through walls included.

```cpp
#include "TopDownMovement/route.h"   // routeFrom, follow; includes retropp/sprite_path.h
using namespace std::chrono_literals;
```

## Contents

- [A patrol](#a-patrol)
- [What the engine provides](#what-the-engine-provides)
- [`routeFrom`](#routefrom)
- [`follow`](#follow)
- [Facing from travel](#facing-from-travel)
- [Waits, posts and playback modes](#waits-posts-and-playback-modes)
- [Reading and controlling a route](#reading-and-controlling-a-route)
- [Detours](#detours)
- [Collision](#collision)
- [Rounding](#rounding)
- [Engine behavior to know](#engine-behavior-to-know)

## A patrol

```cpp
retropp::Vec2i guardAt{16, 120};
TopDownMovement::Walker guard{guardAt};

// Build the route when the walker sets off. It starts at the walker's position.
retropp::SpritePath patrol = TopDownMovement::routeFrom(guard, {
    {.label = "east", .move = retropp::SpritePathMove::to({136.0f, 120.0f}), .pacing = retropp::PathPacing::speed(30.0f)},
    {.label = "wait", .move = retropp::SpritePathMove::to({136.0f, 120.0f}), .pacing = retropp::PathPacing::eased(500ms)},
    {.label = "west", .move = retropp::SpritePathMove::to({16.0f, 120.0f}),  .pacing = retropp::PathPacing::speed(30.0f)},
});

// Every tick.
TopDownMovement::follow(guard, guardConfig, patrol);
```

The guard walks east at 30 pixels per second, stands for half a second, walks back, and repeats. It
faces the direction it travels and plays its walk animation while the route moves.

Keep `patrol` in your game state next to the walker. `guardConfig` is the guard's `WalkerConfig`.

## What the engine provides

These are engine types. Each is documented in the engine's guide.

| Type | What it is | Engine page |
|---|---|---|
| `SpritePath` | a route: a list of nodes played in order, plus the playback state | `docs/guide/sprite-path.md` |
| `SpritePathNode` | one leg: a `label`, a `move` and a `pacing` | `docs/guide/sprite-path.md` |
| `SpritePathMove` | where a leg goes: `to(point)`, `through(points)`, `hermite(…)`, `onCurve(curve)` | `docs/guide/sprite-path.md` |
| `PathPacing` | how fast: `speed(pxPerSecond)`, `eased(duration, easing)`, `distanceTween(tween)` | `docs/guide/path-walker.md` |
| `PlaybackMode` | what happens after the last leg: loop, play once, N laps, play for a duration | `docs/guide/animation.md` |
| `interrupt`, `popInterrupt`, `ResumePolicy` | a detour, and how the route resumes after it | `docs/guide/sprite-path.md` |
| `Curve` | the geometry behind a curved leg | `docs/guide/curve.md` |

Each leg starts where the previous leg ended. The first leg starts at the route's `start`.

## `routeFrom`

```cpp
retropp::SpritePath routeFrom(const Walker& walker, std::vector<retropp::SpritePathNode> nodes);
```

Returns a `SpritePath` over `nodes` whose `start` is the walker's current position. The returned route's
`position()` equals the walker's position before the first `follow`.

- **Coordinates.** Route points are in the same pixels as the walker's position, and they name the
  walker's top-left.
- **When to call it.** On the tick the walker sets off. `follow` writes the route's position to the
  walker, so a walker that moved after `routeFrom` jumps to the route's start on the first `follow`.
- **Timing.** The route takes the engine's active timing when it is constructed. Call `routeFrom` after
  `retropp::EngineConfig::setActive`.
- **Node fields to leave unset.** `animation`, `facing`, `rotationDegrees` and `scale` take effect
  through `SpritePath::applyTo`, which is for a sprite the path writes directly. `follow` reads the
  route's position only, and the walker plays the walk animation in its `WalkerConfig`.

The route is an ordinary value, independent of the walker. `follow` takes both each tick.

## `follow`

```cpp
void follow(Walker& walker, const WalkerConfig& config, retropp::SpritePath& route,
            retropp::PlaybackMode mode = retropp::PlaybackMode::loopIndefinitely());
```

Advances `route` by one tick, then moves `walker` to the route's position.

```cpp
TopDownMovement::follow(guard, guardConfig, patrol);                                  // loops
TopDownMovement::follow(guard, guardConfig, patrol, retropp::PlaybackMode::single()); // plays once
```

- **Call it once per tick, instead of `route.advance()`.** `follow` advances the route. A route that is
  also advanced by your code runs at double speed.
- **Call one of `follow` and `walk`** for a walker on any one tick.
- **It takes any walker.** An `InputWalker` can follow a route
  ([how-to.md](how-to.md#walk-the-player-along-a-route)).
- **The walk animation plays on every tick the route moves,** including ticks where it moves less than
  a pixel and the walker's position stays the same. A route at 30 pixels per second on a 60 Hz tick
  moves half a pixel per tick and animates normally.
- **On a tick the route stands still,** the walker keeps its facing and shows frame 0.

`mode` is the playback mode of the whole route:

| Mode | After the last leg |
|---|---|
| `PlaybackMode::loopIndefinitely()` (default) | The route starts again from its first leg. |
| `PlaybackMode::single()` | The walker stays at the end. `route.finished()` returns `true`. |
| `PlaybackMode::loopNTimes(n)` | The route runs `n` times, then the walker stays at the end. |
| `PlaybackMode::playForDuration(d)` | The route loops until `d` has passed, then the walker stays where it is. |

A looping route starts again at its `start`. If the last leg ends anywhere else, the walker jumps
back to it. End a looping route where it starts.

## Facing from travel

On each tick the route moves, `follow` sets both facings from the route's travel that tick.

- `facing()` is the axis with the larger travel. If the two are equal, the walker keeps its facing when
  that is one of the two directions, and otherwise faces the horizontal one.
- `spriteFacing(config)` is the nearest of the eight directions. It is a diagonal when the smaller
  component is at least 0.4142 of the larger (the tangent of 22.5°).

| Travel direction | `facing()` | `spriteFacing` with `sprites = Ways::Eight` |
|---|---|---|
| east | `Right` | `Right` |
| 20° below east | `Right` | `Right` |
| 30° below east | `Right` | `DownRight` |
| 45° below east | `Right` or `Down`: the current facing if it is one of these, otherwise `Right` | `DownRight` |
| 60° below east | `Down` | `DownRight` |
| south | `Down` | `Down` |

With `sprites = Ways::Four` the sprite shows `facing()`.

`config.movement` applies to `walk`. A route moves diagonally under `Ways::Four` and `Ways::Eight` alike.

## Waits, posts and playback modes

**A wait** is a leg to the point it starts from, paced `eased(duration)`. The walker stands for the
duration, keeps its facing, and shows frame 0.

```cpp
{.label = "wait", .move = retropp::SpritePathMove::to({136.0f, 120.0f}), .pacing = retropp::PathPacing::eased(500ms)},
```

**A post** is a leg to a different point, paced `speed(0.0f)`. It lasts until you interrupt the route or
replace it, and the walker stands at the leg's start for that long. The engine's guide calls this a
sentinel node.

```cpp
{.label = "post", .move = retropp::SpritePathMove::to({136.0f, 120.0f}), .pacing = retropp::PathPacing::speed(0.0f)},
```

## Reading and controlling a route

The route is a `retropp::SpritePath`. Use its own members.

| Call | Use |
|---|---|
| `route.currentNode()->label` | Which leg the walker is on. |
| `route.finished()` | Whether a route played with `single()`, `loopNTimes` or `playForDuration` has ended. |
| `route.pause()`, `route.play()` | Stop and resume. A walker following a paused route stands. |
| `route.interrupt(…)`, `route.popInterrupt()` | Start and end a detour ([Detours](#detours)). |
| `route.interrupted()` | Whether a detour is running. |

To send a walker on a new route from where it stands, build a new one:

```cpp
patrol = TopDownMovement::routeFrom(guard, {
    {.move = retropp::SpritePathMove::to({16.0f, 40.0f}), .pacing = retropp::PathPacing::speed(30.0f)},
});
```

`route.restart()` rewinds a route to its original `start`. A walker that is somewhere else jumps there
on the next `follow`.

## Detours

`route.interrupt(nodes, mode, resume)` suspends the route and starts a detour from the walker's current
position. The next `follow` walks the detour. When the detour ends, the route resumes.

```cpp
// The guard walks to a noise and then returns to where he left the patrol.
patrol.interrupt({{.move = retropp::SpritePathMove::to(noise), .pacing = retropp::PathPacing::speed(60.0f)}},
                 retropp::PlaybackMode::single(), retropp::ResumePolicy::Return);
```

| `ResumePolicy` | When the detour ends |
|---|---|
| `Continue` (the engine's default) | The route resumes from where the detour ended. The rest of the route is shifted by the detour's displacement. |
| `Return` | The walker is put back where it was when the detour began, in one tick, and the route resumes from there. On that tick the walker faces along the jump. |

A detour played with `PlaybackMode::single()` ends on its own. A detour played with
`loopIndefinitely()` ends only when you call `route.popInterrupt()`.

`Return` moves the walker back in a single tick. To have it walk back, add a leg to the detour that
returns to the starting point.

## Collision

`follow` moves the walker wherever the route goes. A route through a wall moves the walker through the
wall. Draw routes through open ground.

A character that must be stopped by walls uses a heading instead of a route:
[how-to.md](how-to.md#wander-against-walls).

A route stays within one map. To continue in another map, end the route, `place` the walker there, and
build a new route.

## Rounding

A route's position is a `retropp::Vec2` of floats. A walker's position is a `retropp::Vec2i`. `follow`
rounds with `std::lround` when it writes the walker's position. The route keeps its float position, so
each tick rounds from the exact position.

## Engine behavior to know

Two behaviors of `SpritePath` at the engine version this module builds against affect walkers.

**A `speed` leg can last one tick longer than its length divided by its speed.** The engine converts
pixels per second to a fractional number of pixels per tick. A leg of 24 pixels at 60 pixels per second
on a 60 Hz tick takes 25 ticks, and the route stands still on the last one. On that tick the walker
stands and its walk animation returns to frame 0. For a leg that ends on an exact tick, pace it by
duration:

```cpp
const std::chrono::nanoseconds tick = retropp::SpritePath::defaultTiming.tickPeriod();

{.move = retropp::SpritePathMove::to({40.0f, 16.0f}), .pacing = retropp::PathPacing::eased(24 * tick, retropp::Easing::Linear)},
```

**A `Continue` detour resumes one tick early.** The route resumes from the position the walker had one
tick before the detour's end. A detour of 8 pixels at one pixel per tick shifts the route by 7 pixels.
`Return` resumes at the exact position.
