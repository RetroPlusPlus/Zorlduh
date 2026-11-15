# Concepts

The model behind the module: a config that says what a walker is, a walker bound to a position the game
keeps, three ways of moving it a tick, and one function that draws it.

## Contents

- [The config and the walker](#the-config-and-the-walker)
- [The position a walker is bound to](#the-position-a-walker-is-bound-to)
- [Three pathways](#three-pathways)
- [A tick of walking](#a-tick-of-walking)
- [Facing](#facing)
- [Sprite directions](#sprite-directions)
- [The walk animation](#the-walk-animation)
- [Drawing a walker](#drawing-a-walker)
- [Where each part lives in a game](#where-each-part-lives-in-a-game)

## The config and the walker

A `WalkerConfig` is what a kind of walker *is*: how fast it moves, whether it moves in eight directions
or four, whether its sprite has four directions or eight, what of it collides, the points in its art, and
its walk animation for each sprite direction. It is built once, after the art is loaded. One config
serves every walker of that kind: every bat in a cave walks by the same bat config.

A `Walker` is how one walker *stands now*: the way it faces, the way its sprite faces, and how far it is
into its stride. Every call that needs both takes both: `walker.walk(config, …)`,
`walker.frame(config)`, `WalkerSprite(key, walker, config)`.

## The position a walker is bound to

A walker walks a position it does not own. Where something stands is the game's — it may be a field of
an actor, of an enemy, of the player — and the walker is bound to it once, when it is made:

```cpp
retropp::Vec2i heroAt{72, 56};
TopDownMovement::InputWalker hero{heroAt};
```

From then on the walker reads and writes that position: `walk` moves it, `place` and `moveTo` write it,
and `hero.position()` reads it, as `heroAt` itself does. Nothing is copied back and forth, and the
position is never in two places.

The position outlives the walker and stays at its address while the walker is bound to it. A copy of a
walker walks the same position as the original until it is bound to another with `bind`, so whatever
keeps a walker beside its position and is copied — a game state, a struct holding both — binds the copy
to the copied position ([how-to.md](how-to.md#keep-a-walker-and-its-position-in-one-value)).

## Three pathways

What says where a walker goes is one of three things, and each has its own way in. All three step,
turn and animate the same walker.

| Pathway | What moves it | Type and call |
|---|---|---|
| **Input** | the pad: the directions held, and the order they were pressed in | `InputWalker::walk(config, DirectionalInput, solids)` |
| **Free** | a heading the game works out: a rule, an enemy's chase, a wander | `Walker::walk(config, Directions, solids)` |
| **Route** | the engine's `SpritePath`: legs, waits, detours | `follow(walker, config, route)` ([routes.md](routes.md)) |

The input pathway is the only one that needs to remember anything about input — which of two held
directions was pressed last, to decide which way to face — so that memory is in `InputWalker` alone. A
plain `Walker` takes a heading and steps. On a route nothing collides: the route goes where it is drawn.

## A tick of walking

```cpp
walker.walk(config, heading, solids);   // the free pathway; the input pathway is the same with the pad
```

`heading` is a `Directions`: four flags. `solids` answers whether a rectangle of world pixels touches
anything solid ([collision.md](collision.md)).

1. Opposite directions cancel: left and right together are no x movement; up and down together are no
   y movement.
2. Under **`Ways::Four`** a heading along both axes is brought to one. A heading walker keeps the axis it
   faces along when that is one of the two, and otherwise takes the horizontal one; an input walker
   takes the one pressed more recently, and the horizontal one when both were pressed on the same tick.
3. The walker moves along x, a pixel at a time, up to `config.speed` pixels. It takes each pixel only if
   its collision shape there touches nothing solid, so it stops flush against a wall.
4. Then the same along y.

Under **`Ways::Eight`**, the default, a diagonal moves on both axes. Because each axis is tried on its
own, a diagonal into a wall moves along the wall: the blocked axis stops and the other carries on. A
diagonal moves `speed` pixels on both axes, so it covers more ground per tick than a straight line does.
Under `Ways::Four` the walker moves along one axis a tick, and a diagonal into a wall moves only if the
axis it was brought to is clear.

## Facing

A walker faces one of four ways, `Dir::Up`, `Down`, `Left` or `Right`, and its facing follows its heading
on every tick, whether or not it moves. A walker heading into a wall turns to face the wall and stands
still facing it; that is how a game knows what the walker is facing to read or use. Heading nowhere, or
only in opposite directions, leaves the facing as it was.

- **Heading along one axis**, it faces that way. Under `Ways::Four` this is the axis the heading was
  brought to, so a walker faces the way it moves.
- **Heading along both axes** (a diagonal, whether or not a wall blocks one of them), it keeps its
  facing if that is one of the two. Otherwise a heading walker faces the horizontal one, and an input
  walker the one of the two pressed more recently — the horizontal one when both were pressed on the
  same tick.

So a player walking right who adds up keeps facing right, and one walking up who adds right keeps facing
up: the facing follows the direction the player committed to first. A walker moved by `moveTo` or along
a route faces the axis of the larger part of its travel ([routes.md](routes.md#facing-from-travel)).

## Sprite directions

`config.sprites` says how many directions the walker's sprite has:

- **`Ways::Four`**, the default: the sprite shows the walker's facing. On a diagonal it keeps showing
  what it showed when the diagonal began.
- **`Ways::Eight`**: the sprite shows the last of the eight directions the walker headed in, as a
  `Facing`. A diagonal shows its diagonal's own animation, and a walker that stops after a diagonal
  stands on that diagonal's first frame.

`walker.spriteFacing(config)` is the direction the sprite shows. Movement and sprite directions are two
switches: 8-way movement with four sprite directions is the classic look, 8-way movement with eight gives
the diagonals sprites of their own, and 4-way movement shows four whatever `sprites` says.

## The walk animation

`config.walk` holds one `retropp::Animation` per sprite direction: `up`, `down`, `left`, `right`, and
`upLeft`, `upRight`, `downLeft`, `downRight`, which are read only under `sprites = Ways::Eight`. While the
walker moves, the animation for the way its sprite faces plays, looping; when it stands, it shows that
animation's frame 0.

The walker counts the ticks it has walked since it last stood, and the frame is that count resolved
through the animation, at the cadence in `config.timing` (by default, the engine's active cadence when
the config is built). A turn keeps the count, so the stride carries on in the new direction's animation
instead of starting over. On a route the count runs on every tick the route moves at all, so a walker
going half a pixel a tick still walks.

The animations are the game's: any frames, any durations, any number of frames per direction. Each frame
names its own sheet, slot and palette, as every engine animation does (the engine's
`docs/guide/animation.md`).

## Drawing a walker

```cpp
const retropp::Sprite sprite = TopDownMovement::WalkerSprite("hero", hero, heroConfig);
```

The sprite shows the walker's current frame with the frame's top-left at the walker's position. Its `z`
is the bottom edge of the walker's collision shape: on a sprite layer of walkers, sprites with a higher
`z` draw in front, so the walker standing lower on screen covers the one above it. The config's anchors
are published on the sprite, for anything else that pins to it. Every pathway draws the same way.

The sprite goes on any sprite layer the game builds. Placed in world pixels, it belongs on a layer whose
scroll is the camera, the same scroll as the world's tile layer.

## Where each part lives in a game

| Part | Built | Lives in |
|---|---|---|
| `WalkerConfig` | once, after loading the art | wherever the game keeps loaded data |
| the position | at spawn | the game state, where the thing that stands there is |
| `Walker`, `InputWalker` | at spawn, bound to the position | the game state, beside the position |
| a route (`retropp::SpritePath`) | when the walker sets off on it | the game state |
| `walk(…)`, `follow(…)` | every tick | the tick |
| `readDirections(…)` | every tick | the input boundary, before the tick |
| `WalkerSprite(…)` | every frame | the component that draws the walker |

`walker_sprite.h` is the only header that builds a draw type, so the game's simulation includes
`walker.h`, `input_walker.h` and `route.h`, and the drawing code includes `walker_sprite.h`.
