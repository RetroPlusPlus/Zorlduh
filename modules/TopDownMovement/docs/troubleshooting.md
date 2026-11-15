# Troubleshooting

Symptoms, their causes and the fix for each. The last two sections list the module's compile errors and
exceptions.

## Contents

- [The walker does not move](#the-walker-does-not-move)
- [The walker walks through walls](#the-walker-walks-through-walls)
- [The walker cannot fit through a gap](#the-walker-cannot-fit-through-a-gap)
- [The walk animation stays on one frame](#the-walk-animation-stays-on-one-frame)
- [The walk animation plays at the wrong speed](#the-walk-animation-plays-at-the-wrong-speed)
- [The sprite never shows a diagonal](#the-sprite-never-shows-a-diagonal)
- [The sprite is in the wrong place](#the-sprite-is-in-the-wrong-place)
- [The walker slides across the screen when placed](#the-walker-slides-across-the-screen-when-placed)
- [A walker on a route jumps](#a-walker-on-a-route-jumps)
- [A route runs at the wrong speed](#a-route-runs-at-the-wrong-speed)
- [A copy moves the original](#a-copy-moves-the-original)
- [Positions turn to garbage, or the game crashes](#positions-turn-to-garbage-or-the-game-crashes)
- [Compile errors](#compile-errors)
- [Exceptions](#exceptions)

## The walker does not move

| Cause | Fix |
|---|---|
| The heading or the held directions are empty. The engine reports nothing for an action with no binding. | Bind the four direction actions in the `ActionMap` you give the platform, and check that `DirectionalActions` names the same four. |
| `TileCollision::map` is `nullptr`. With no map, everything is solid. | Set `.map` to your `IndexGrid`. |
| `TileCollision::open` is not the value your map uses for walkable cells. | Set `.open` to that value. The default is 0. |
| `TileCollision::cellSize` does not match the map. The walker's pixels land in the wrong cells or outside the map, which is solid. | Set `.cellSize` to the size of one map cell in pixels. |
| The walker's collision shape overlaps something solid where it stands. | Place it in open ground ([how-to.md](how-to.md#spawn-clear-of-walls)). |
| `config.speed` is 0 or less. The walker turns and does not move. | Set `speed` to 1 or more. |
| `place` is called every tick with a stored position. | Call `place` once, when the walker spawns. |

To find which, call `config.collision.hits(solids, walker.position())`. If it returns `true`, the walker
is standing in something solid, or the solids are wrong.

## The walker walks through walls

| Cause | Fix |
|---|---|
| `config.collision` was not set. An empty shape collides with nothing. | Set `.collision` in the config. |
| The walker is on a route. Routes do not collide. | Draw the route through open ground, or use a heading ([routes.md](routes.md#collision)). |
| The walker was moved by `moveTo`, by `place`, or by writing to its position. None of these test collision. | Test with `config.collision.hits` first ([how-to.md](how-to.md#push-a-walker)). |
| The collision map is not at world pixel (0, 0). | Subtract the map's origin in a callable ([collision.md](collision.md#limits)). |
| `open` equals the value of the wall cells. | Set `.open` to the walkable value. |

## The walker cannot fit through a gap

Collision is tested per pixel. A shape 16 pixels wide fits a 16-pixel gap only when it is lined up with
it exactly.

| Fix | |
|---|---|
| Make the collision shape narrower than the gap. | A box 10 pixels wide in a 16-pixel frame leaves 3 pixels of room on each side. |
| Cover only the feet. | The top of the art can then overlap walls above it. |

## The walk animation stays on one frame

| Cause | Fix |
|---|---|
| `place` is called every tick. It resets the stride. | Call `place` only to spawn or teleport. To change the position on other ticks, assign to the position itself. |
| `walk` is called a second time in the tick with an empty heading. | Call `walk` once per tick for each walker. |
| The walker is blocked on every axis it heads along. It is not moving, so it shows frame 0. | None needed. Check `moving()`. |
| The route is paused, on a wait, or on a post. | None needed. `follow` shows frame 0 while the route does not move. |
| The animation has one frame, or every frame names the same slot. | Check the animation. |

## The walk animation plays at the wrong speed

| Cause | Fix |
|---|---|
| The config was constructed before `retropp::EngineConfig::setActive`. Its `timing` is the engine's default tick rate, not yours. | Construct configs after `setActive`, or set `.timing`. |
| `walk` or `follow` is called more than once per tick for the walker. The walker also moves too fast. | Call one of them, once per tick. |
| The directions' animations have different frame durations. The speed changes when the walker turns. | Give every direction the same durations. |

## The sprite never shows a diagonal

| Cause | Fix |
|---|---|
| `config.sprites` is `Ways::Four`, the default. | Set `.sprites = TopDownMovement::Ways::Eight` and provide the four diagonal animations. |
| `config.movement` is `Ways::Four`. The walker never heads diagonally. | Set `.movement = TopDownMovement::Ways::Eight`. |

## The sprite is in the wrong place

| Cause | Fix |
|---|---|
| The sprite layer does not scroll with the camera. The sprite is in world pixels. | Give the layer the same scroll as the world's tile layer. |
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
| At the end of every lap | The route loops, and its last leg does not end at its start. | End the route where it starts, or follow it with `PlaybackMode::single()`. |
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
| The struct holding the walker and its position was moved or copied without `bind`. | [how-to.md](how-to.md#keep-a-walker-and-its-position-in-one-value) |

## Compile errors

| Error | Cause | Fix |
|---|---|---|
| No default constructor for `Walker` or `InputWalker` | A walker is always bound to a position. | Construct it with a `retropp::Vec2i&`. |
| No matching `walk` for an `InputWalker` given a `Directions` | `InputWalker::walk` takes a `DirectionalInput` and hides the heading overload. | Pass a `DirectionalInput`, or use a plain `Walker`. |
| Designated initializers out of order in a `WalkerConfig` | C++ requires declaration order. | Use the order `speed`, `movement`, `sprites`, `collision`, `anchors`, `walk`, `timing`. |
| Constraint `SolidQuery` not satisfied | The solids argument has no `solidBox(retropp::IntRect)` member and cannot be called with a `retropp::IntRect`. | Pass a `TileCollision` or a callable taking a `retropp::IntRect` and returning `bool`. |
| Cannot bind a temporary to `retropp::Vec2i&` | The constructor needs a position that outlives the walker. | Pass a named `retropp::Vec2i` from your game state. |

## Exceptions

| Call | Throws | When |
|---|---|---|
| `Walker::frame`, `WalkerSprite` | `std::logic_error` | The animation for the sprite's direction has no frames. Under `sprites = Ways::Eight` that includes a diagonal. |
| `WalkerSprite` | `std::logic_error` | The frame names no art (a palette-only frame). |
| `Walker::anchor` | `std::out_of_range` | `config.anchors` has no anchor with that label. |
| `CollisionShape(ShapePoints)` | `std::invalid_argument` | The shape has no points and no curve, is inverted, or has a transform. |
| `CollisionShape::fromAnchors` | `std::invalid_argument` | Fewer than three labels. |
| `CollisionShape::fromAnchors` | `std::out_of_range` | A label is not in the anchor table. |

`walk`, `moveTo`, `face`, `place`, `bind` and the `TileCollision` members do not throw. A solids callable
of your own that throws propagates out of `walk`.
