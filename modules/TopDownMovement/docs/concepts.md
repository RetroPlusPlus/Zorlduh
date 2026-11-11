# Concepts

The model behind the module: a config that says what a walker is, a value that says where one is now,
one call that moves it a tick, and one function that draws it.

## Contents

- [The config and the walker](#the-config-and-the-walker)
- [A tick of walking](#a-tick-of-walking)
- [Facing](#facing)
- [The walk animation](#the-walk-animation)
- [Drawing a walker](#drawing-a-walker)
- [Where each part lives in a game](#where-each-part-lives-in-a-game)

## The config and the walker

A `WalkerConfig` is what a kind of walker *is*: how fast it moves, what of it collides, the points in its
art, and its walk animation for each facing. It is built once, after the art is loaded, and never
changes. One config serves every walker of that kind: every bat in a cave walks by the same bat config.

A `Walker` is one walker *now*: its position, its facing, how far it is into its stride, and the order
its directions were last pressed in. It is a small value with no pointers in it, so a game state holding
walkers copies and saves like any other value.

Every call that needs both takes both: `walker.walk(config, …)`, `walker.frame(config)`,
`WalkerSprite(key, walker, config)`.

## A tick of walking

```cpp
walker.walk(config, directions, solids);
```

`directions` is a `DirectionalInput`: the four directions held this tick and the ones pressed since the
last. `solids` answers whether a rectangle of world pixels touches anything solid
([collision.md](collision.md)).

1. Opposite directions cancel: left and right together are no x movement; up and down together are no
   y movement.
2. The walker moves along x, a pixel at a time, up to `config.speed` pixels. It takes each pixel only if
   its collision shape there touches nothing solid, so it stops flush against a wall.
3. Then the same along y.

Because each axis is tried on its own, a diagonal into a wall moves along the wall: the blocked axis
stops and the other carries on. A diagonal moves `speed` pixels on both axes, so it covers more ground
per tick than a straight line does.

## Facing

A walker faces one of four ways, `Dir::Up`, `Down`, `Left` or `Right`, and its facing changes only on
a tick it moves. A walker pushing into a wall it cannot move away from keeps the facing it had.

- **Moving along one axis**, it faces the way it moves.
- **Holding two directions on different axes** (a diagonal, whether or not a wall blocks one of them),
  it keeps its facing if that is one of the two. Otherwise it faces the one of the two pressed more
  recently. When both were pressed on the same tick, it faces the horizontal one.

So a character walking right who adds up keeps facing right, and one walking up who adds right keeps
facing up: the facing follows the direction the player committed to first. A walker that turns from
down to an up-right diagonal faces whichever of up and right was pressed last.

## The walk animation

`config.walk` holds one `retropp::Animation` per facing. While the walker moves, the animation for its
facing plays, looping; when it stands, it shows that animation's frame 0.

The walker counts the ticks it has walked since it last stood, and the frame is that count resolved
through the animation, at the cadence in `config.timing` (by default, the engine's active cadence when
the config is built). A
turn keeps the count, so the stride carries on in the new facing's animation instead of starting over.

The animations are the game's: any frames, any durations, any number of frames per facing. Each frame
names its own sheet, slot and palette, as every engine animation does (the engine's
`docs/guide/animation.md`).

## Drawing a walker

```cpp
const retropp::Sprite sprite = TopDownMovement::WalkerSprite("hero", walker, config);
```

The sprite shows the walker's current frame with the frame's top-left at the walker's position. Its `z`
is the bottom edge of the walker's collision shape: on a sprite layer of walkers, sprites with a higher
`z` draw in front, so the walker standing lower on screen covers the one above it. The config's anchors
are published on the sprite, for anything else that pins to it.

The sprite goes on any sprite layer the game builds. Placed in world pixels, it belongs on a layer whose
scroll is the camera, the same scroll as the world's tile layer.

## Where each part lives in a game

| Part | Built | Lives in |
|---|---|---|
| `WalkerConfig` | once, after loading the art | wherever the game keeps loaded data |
| `Walker` | at spawn | the game state |
| `walker.walk(…)` | every tick | the tick |
| `readDirections(…)` | every tick | the input boundary, before the tick |
| `WalkerSprite(…)` | every frame | the component that draws the walker |

`walker_sprite.h` is the only header that builds a draw type, so the game's simulation includes
`walker.h` and the drawing code includes `walker_sprite.h`.
