# Troubleshooting

Symptoms, their causes and the fix for each. The last two sections list the module's compile errors and
exceptions.

## Contents

- [The walker stands still](#the-walker-stands-still)
- [The walker walks through walls](#the-walker-walks-through-walls)
- [The walker is stuck at a gap](#the-walker-is-stuck-at-a-gap)
- [The walk animation stays on one frame](#the-walk-animation-stays-on-one-frame)
- [The walk animation plays at the wrong speed](#the-walk-animation-plays-at-the-wrong-speed)
- [The sprite shows only four directions](#the-sprite-shows-only-four-directions)
- [The sprite is in the wrong place](#the-sprite-is-in-the-wrong-place)
- [The walker slides across the screen when placed](#the-walker-slides-across-the-screen-when-placed)
- [A walker on a route jumps](#a-walker-on-a-route-jumps)
- [A route runs at the wrong speed](#a-route-runs-at-the-wrong-speed)
- [A copy moves the original](#a-copy-moves-the-original)
- [Positions turn to garbage, or the game crashes](#positions-turn-to-garbage-or-the-game-crashes)
- [Compile errors](#compile-errors)
- [Exceptions](#exceptions)

## The walker stands still

| Cause | Fix |
|---|---|
| The heading or the held directions are empty. The engine reports an action only when it has a binding. | Bind the four direction actions in the `ActionMap` you give the platform, and check that `DirectionalActions` names the same four. |
| `TileCollision::map` is `nullptr`. A `TileCollision` with a null map treats everything as solid. | Set `.map` to your `IndexGrid`. |
| `TileCollision::open` differs from the value your map uses for walkable cells. | Set `.open` to that value. The default is 0. |
| `TileCollision::cellSize` differs from the map's cell size. The walker's pixels land in the wrong cells or outside the map, which is solid. | Set `.cellSize` to the size of one map cell in pixels. |
| The walker's collision shape overlaps something solid where it stands. | Place it in open ground ([how-to.md](how-to.md#spawn-clear-of-walls)). |
| `config.speed` is 0 or less. The walker turns and stays where it is. | Set `speed` to 1 or more. |
| `place` is called every tick with a stored position. | Call `place` once, when the walker spawns. |

To find which, call `config.collision.hits(solids, walker.position())`. If it returns `true`, the walker
is standing in something solid, or the solids are wrong.

## The walker walks through walls

| Cause | Fix |
|---|---|
| `config.collision` was left empty. An empty shape passes through everything. | Set `.collision` in the config. |
| The walker is on a route. A route goes through walls. | Draw the route through open ground, or use a heading ([routes.md](routes.md#collision)). |
| The walker was moved by `moveTo`, by `place`, or by writing to its position. All three put the walker exactly where they are told. | Test with `config.collision.hits` first ([how-to.md](how-to.md#push-a-walker)). |
| The collision map is placed away from world pixel (0, 0). | Subtract the map's origin in a callable ([collision.md](collision.md#limits)). |
| `open` equals the value of the wall cells. | Set `.open` to the walkable value. |

## The walker is stuck at a gap

Collision is tested per pixel. A shape 16 pixels wide fits a 16-pixel gap only when it is lined up with
it exactly.

| Fix | |
|---|---|
| Make the collision shape narrower than the gap. | A box 10 pixels wide in a 16-pixel gap leaves 3 pixels of room on each side. |
| Cover only the feet. | The top of the art can then overlap walls above it. |

## The walk animation stays on one frame

| Cause | Fix |
|---|---|
| `place` is called every tick. It resets the stride. | Call `place` only to spawn or teleport. To change the position on other ticks, assign to the position itself. |
| `walk` is called a second time in the tick with an empty heading. | Call `walk` once per tick for each walker. |
| The walker is blocked on every axis it heads along. It is standing, so it shows frame 0. | This is correct behavior. Check `moving()`. |
| The route is paused, on a wait, or on a post. | This is correct behavior. `follow` shows frame 0 while the route stands still. |
| The animation has one frame, or every frame names the same slot. | Check the animation. |

## The walk animation plays at the wrong speed

| Cause | Fix |
|---|---|
| The config was constructed before `retropp::EngineConfig::setActive`. Its `timing` is the engine's default tick rate instead of yours. | Construct configs after `setActive`, or set `.timing`. |
| `walk` or `follow` is called more than once per tick for the walker. The walker also moves too fast. | Call one of them, once per tick. |
| The directions' animations have different frame durations. The speed changes when the walker turns. | Give every direction the same durations. |

## The sprite shows only four directions

| Cause | Fix |
|---|---|
| `config.sprites` is `Ways::Four`, the default. | Set `.sprites = TopDownMovement::Ways::Eight` and provide the four diagonal animations. |
| `config.movement` is `Ways::Four`. The walker heads along one axis at a time. | Set `.movement = TopDownMovement::Ways::Eight`. |

## The sprite is in the wrong place

| Cause | Fix |
|---|---|
| The sprite layer has a different scroll from the camera. The sprite is in world pixels. | Give the layer the same scroll as the world's tile layer. |
| The position is treated as the character's center or feet. It is the top-left of the frame. | Offset by the frame size where you set the position. |

## The walker slides across the screen when placed

The engine eases each sprite from its previous position by matching sprite keys between ticks. After
`place`, the walker's sprite is eased from the old position to the new one.

Fix: draw the walker with a different sprite key on the tick it is placed (the engine's
`docs/guide/sprites.md`, "Identity — `key`").

## A walker on a route jumps

| When | Cause | Fix |
|---|---|---|
| On the first `follow` | The walker moved after `routeFrom`. The route starts where the walker stood then. | Call `routeFrom` on the tick the walker sets off. |
| At the end of every lap | The route loops, and its last leg ends away from its start. | End the route where it starts, or follow it with `PlaybackMode::single()`. |
| After `route.restart()` | `restart` rewinds to the route's original start. | Build a new route with `routeFrom` instead. |
| When a detour ends | The detour used `ResumePolicy::Return`, which puts the walker back in one tick. | Add a leg to the detour that walks back, or use `Continue`. |

## A route runs at the wrong speed

| Cause | Fix |
|---|---|
| Your code calls `route.advance()` as well as `follow`. The route runs at double speed. | Call only `follow`. |
| `follow` is called twice per tick. | Call it once. |
| The route was constructed before `retropp::EngineConfig::setActive`. | Call `routeFrom` after `setActive`. |

## A copy moves the original

A copied walker is bound to the same position as the original. Copying a struct that holds a walker and
its position copies the binding too.

Fix: call `bind` on the copy with the copy's position, or use a struct that does it in its copy
constructor ([how-to.md](how-to.md#keep-a-walker-and-its-position-in-one-value)).

## Positions turn to garbage, or the game crashes

A walker is bound to a position that has moved or been destroyed.

| Cause | Fix |
|---|---|
| The positions are in a `std::vector` that grew. | [concepts.md](concepts.md#lifetimes) |
| The position was a local variable that went out of scope. | Keep the position in game state, for as long as the walker. |
| The struct holding the walker and its position was moved or copied, and the copy was left bound to the original. | [how-to.md](how-to.md#keep-a-walker-and-its-position-in-one-value) |

## Compile errors

| Code that fails to compile | Cause | Fix |
|---|---|---|
| Default-constructing a `Walker` or `InputWalker` | A walker is always bound to a position. | Construct it with a `retropp::Vec2i&`. |
| Calling `InputWalker::walk` with a `Directions` | `InputWalker::walk` takes a `DirectionalInput` and hides the heading overload. | Pass a `DirectionalInput`, or use a plain `Walker`. |
| A `WalkerConfig` with designated initializers out of order | C++ requires declaration order. | Use the order `speed`, `movement`, `sprites`, `collision`, `anchors`, `walk`, `timing`. |
| Passing solids that fail the `SolidQuery` constraint | `SolidQuery` requires a `solidBox(retropp::IntRect)` member, or a call operator taking a `retropp::IntRect`. | Pass a `TileCollision` or a callable taking a `retropp::IntRect` and returning `bool`. |
| Passing a temporary position to a walker's constructor | The constructor needs a position that outlives the walker. | Pass a named `retropp::Vec2i` from your game state. |

## Exceptions

| Call | Throws | When |
|---|---|---|
| `Walker::frame`, `WalkerSprite` | `std::logic_error` | The animation for the sprite's direction is empty. Under `sprites = Ways::Eight` that includes a diagonal. |
| `WalkerSprite` | `std::logic_error` | The frame is a palette-only frame. |
| `Walker::anchor` | `std::out_of_range` | The label is missing from `config.anchors`. |
| `CollisionShape(ShapePoints)` | `std::invalid_argument` | The shape is empty, is inverted, or has a transform. |
| `CollisionShape::fromAnchors` | `std::invalid_argument` | Fewer than three labels. |
| `CollisionShape::fromAnchors` | `std::out_of_range` | A label is missing from the anchor table. |

`walk`, `moveTo`, `face`, `place`, `bind` and the `TileCollision` members are exception-free. An
exception thrown by your own solids callable propagates out of `walk`.
