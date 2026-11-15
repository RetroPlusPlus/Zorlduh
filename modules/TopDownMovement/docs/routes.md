# Routes

The route pathway: a walker moved along the engine's `SpritePath`. A patrol, a scripted walk, a guard
taking a detour and coming back are all routes. The engine already has routes — their geometry, their
pacing, waits, sequence modes and an interrupt stack — so the module uses them as they are and adds only
the walker on top: the position it is bound to, the facing chosen from the route's travel, and the walk
animation's stride.

```cpp
#include "TopDownMovement/route.h"   // routeFrom, follow (and, through it, retropp/sprite_path.h)
using namespace std::chrono_literals;
```

## Contents

- [The engine's pieces](#the-engines-pieces)
- [`routeFrom`](#routefrom)
- [`follow`](#follow)
- [Facing from travel](#facing-from-travel)
- [Waits, sentinels and sequence modes](#waits-sentinels-and-sequence-modes)
- [Detours](#detours)
- [Nothing collides on a route](#nothing-collides-on-a-route)
- [Where the position is rounded](#where-the-position-is-rounded)

## The engine's pieces

Every one of these is the engine's, documented in its guide; this page only says how a walker rides on
them.

| Piece | What it is | The engine's page |
|---|---|---|
| `Curve`, `ArcLengthTable` | a route leg's geometry: a line, a curve through points, a Hermite arc | `docs/guide/curve.md` |
| `PathPacing` | how fast a leg is walked: `speed(pxPerSecond)`, `eased(duration, easing)`, or a `Tween<float>` of distance | `docs/guide/path-walker.md` |
| `PathWalker` | elapsed ticks to a position and a facing along one curve | `docs/guide/path-walker.md` |
| `SpritePath`, `SpritePathNode`, `SpritePathMove` | a route of legs played back to back, each departing from where the last ended | `docs/guide/sprite-path.md` |
| the wait node, the sentinel node | standing for a time; standing until something moves the route on | `docs/guide/sprite-path.md` |
| sequence modes | loop, play once, N laps, play for a duration | `docs/guide/sprite-path.md` |
| `interrupt`, `popInterrupt`, `ResumePolicy` | a detour from where the walker stands, and how the route takes up again after it | `docs/guide/sprite-path.md` |

## `routeFrom`

```cpp
retropp::SpritePath routeFrom(const Walker& walker, std::vector<retropp::SpritePathNode> nodes);
```

A route over `nodes` that departs from where `walker` stands: a `SpritePath` whose `start` is the
walker's position. It is resolved at its first tick before it is returned, so `route.position()` is the
walker's position from the start.

```cpp
retropp::Vec2i guardAt{16, 120};
TopDownMovement::Walker guard{guardAt};

retropp::SpritePath patrol = TopDownMovement::routeFrom(guard, {
    {.label = "east", .move = retropp::SpritePathMove::to({136.0f, 120.0f}), .pacing = retropp::PathPacing::speed(30.0f)},
    {.label = "wait", .move = retropp::SpritePathMove::to({136.0f, 120.0f}), .pacing = retropp::PathPacing::eased(500ms)},
    {.label = "west", .move = retropp::SpritePathMove::to({16.0f, 120.0f}),  .pacing = retropp::PathPacing::speed(30.0f)},
});
```

The route is the game's, kept in its state beside the walker. It is an ordinary `SpritePath`: the game
reads `route.currentNode()->label` and `route.finished()`, interrupts it, pops it and restarts it through
the engine's own surface. A node's `animation` stays null and its `facing` policy stays `None`: the walk
animation is the walker's, chosen by the way its sprite faces.

The route's points are in the same pixels as the walker's position, and they name the walker's top-left,
as its position does.

## `follow`

```cpp
void follow(Walker& walker, const WalkerConfig& config, retropp::SpritePath& route,
            retropp::PlaybackMode mode = retropp::PlaybackMode::loopIndefinitely());
```

One tick on `route`. It advances the route once under `mode` — the playback mode of the route's own
nodes, as `SpritePath::advance` takes it, so the default loops — and moves the walker to where the route
is now, through `Walker::moveTo`, with how far the route moved this tick as the travel.

```cpp
TopDownMovement::follow(guard, guardConfig, patrol);   // once per tick, in place of patrol.advance()
```

`follow` is the route's advance for the tick: a route a walker follows is not also advanced by the game.
Because it measures the route's own movement before rounding, the walker walks on every tick the route
moves at all: a route of `speed(30.0f)` at 60 ticks a second moves half a pixel a tick, and the walker's
stride counts every tick of it.

## Facing from travel

On a tick the route moves, the walker faces along its travel (`Walker::moveTo`):

- its **sprite** faces the nearest of the eight directions: a diagonal when the smaller part of the
  travel is at least 0.4142 of the larger (the tangent of 22.5°, halfway between a cardinal and a
  diagonal), a cardinal otherwise;
- the walker **faces** along the axis of the larger part. When the two are equal, it keeps its facing if
  that is one of the two, and otherwise faces the horizontal one.

So a leg east faces right, a leg north faces up, and a leg at 30° below east faces right with a
down-right sprite under `sprites = Ways::Eight`. `config.movement` is not read: a route goes where it is
drawn, diagonals included, whatever the walker's own movement is.

On a tick the route does not move, the walker keeps both facings and stands, on frame 0.

## Waits, sentinels and sequence modes

All three are the engine's, and a walker following them does what they say:

- a **wait node** — a leg to the point it starts from, paced `eased(duration)` — stands the walker still
  for the duration, facing the way it came, on frame 0;
- a **sentinel node** — a leg with `speed(0.0f)` — stands it until the game moves the route on;
- the **sequence mode** passed to `follow` decides what happens after the last leg: the default loops,
  `single()` stands the walker at the route's end.

## Detours

`route.interrupt(nodes, mode, resume)` starts a detour from where the walker stands; the next `follow`
walks it. When the detour ends the route takes up again, under the `ResumePolicy` given:

- **`Continue`** (the engine's default) — the route carries on from where the detour left the walker,
  its legs moved with it;
- **`Return`** — the walker is put back where it stood when the detour began, and the route carries on
  from there. On the tick of the hand-back the walker's travel is that jump, so it faces back along it.

`route.popInterrupt()` ends a detour at once; it is the only way out of a detour played under
`loopIndefinitely()`.

```cpp
// The guard breaks off toward a noise, then goes back to his post.
patrol.interrupt({{.move = retropp::SpritePathMove::to(noise), .pacing = retropp::PathPacing::speed(60.0f)}},
                 retropp::PlaybackMode::single(), retropp::ResumePolicy::Return);
```

## Nothing collides on a route

A route goes where it is drawn: the walker's collision shape is not tested on the way, so a route through
a wall walks through it. Draw routes in open space. A walker that should be stopped by walls — one that
wanders, chases or flees — takes the free pathway instead (`Walker::walk` with a heading) and turns by
the game's own rule when something stops it ([how-to.md](how-to.md#wander-against-walls)).

A route's points are in one area's pixels. A route that should go on into another area is ended at the
edge and a new one begun in the next.

## Where the position is rounded

A route's position is in float pixels; a walker's is in whole ones. `follow` rounds the route's position
with `std::lround` when it writes the walker's, the same rounding the engine's `SpritePath::applyTo`
uses for a sprite. The route keeps its float position, so a slow route arrives exactly where it is drawn
however long it takes.
