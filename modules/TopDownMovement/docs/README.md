# TopDownMovement guide

Documentation for the TopDownMovement module: walking characters for a top-down game on the Polyrhythm
engine.

New to the module? Read [getting-started.md](getting-started.md), then [concepts.md](concepts.md).

## Pages

| Page | Contents |
|---|---|
| [getting-started.md](getting-started.md) | A complete example program, explained step by step. |
| [concepts.md](concepts.md) | How the module works: the config, the walker, the position, the three ways to move a walker, facing, sprites, lifetimes. |
| [collision.md](collision.md) | Solids and collision shapes. Reference for `CollisionShape`, `TileCollision`, `CellValue` and `SolidQuery`. Limits and cost. |
| [routes.md](routes.md) | Moving a walker along the engine's `SpritePath`. Reference for `routeFrom` and `follow`. |
| [how-to.md](how-to.md) | Recipes for specific tasks. |
| [troubleshooting.md](troubleshooting.md) | Symptoms, causes and fixes. Compile errors and exceptions. |
| [testing.md](testing.md) | Testing code that uses the module. |
| [walker.md](walker.md) | Reference for `WalkerConfig`, `Walker`, `InputWalker`, `WalkerSprite`, `readDirections` and the direction types. |

## Find it by task

| I want to | Read |
|---|---|
| Make the player walk | [getting-started.md](getting-started.md), [concepts.md](concepts.md#overview) |
| Make an enemy chase or wander | [how-to.md](how-to.md#chase-a-target), [how-to.md](how-to.md#wander-against-walls) |
| Make a character patrol | [how-to.md](how-to.md#patrol-a-route), [routes.md](routes.md) |
| Move in four directions only | [how-to.md](how-to.md#move-in-four-directions-only) |
| Use eight directions of art | [how-to.md](how-to.md#give-the-diagonals-their-own-sprites) |
| Use a round or polygon collision shape | [collision.md](collision.md#collision-shapes) |
| Make doors, blocks or other walkers solid | [collision.md](collision.md#solids), [how-to.md](how-to.md#stop-walkers-walking-through-each-other) |
| Move a walker from my own code | [how-to.md](how-to.md#push-a-walker), [how-to.md](how-to.md#move-a-walker-with-your-own-mover) |
| Walk the player through a scripted scene | [how-to.md](how-to.md#walk-the-player-along-a-route) |
| Keep walkers in a `std::vector` or copy my game state | [concepts.md](concepts.md#lifetimes), [how-to.md](how-to.md#keep-a-walker-and-its-position-in-one-value) |
| Fix a walker that does not move, or an animation that does not play | [troubleshooting.md](troubleshooting.md) |
| Write tests | [testing.md](testing.md) |

## Find it by name

| Name | Header | Page |
|---|---|---|
| `Ways`, `Facing` | `directions.h` | [walker.md](walker.md#ways) |
| `Directions`, `DirectionalInput` | `directions.h` | [walker.md](walker.md#directions) |
| `ActionName`, `DirectionalActions`, `readDirections` | `directions.h` | [walker.md](walker.md#readdirections) |
| `DirectionalAnimations`, `WalkerConfig` | `walker.h` | [walker.md](walker.md#walkerconfig) |
| `Walker` | `walker.h` | [walker.md](walker.md#walker) |
| `InputWalker` | `input_walker.h` | [walker.md](walker.md#inputwalker) |
| `WalkerSprite` | `walker_sprite.h` | [walker.md](walker.md#walkersprite) |
| `routeFrom`, `follow` | `route.h` | [routes.md](routes.md#routefrom) |
| `CollisionShape` | `collision.h` | [collision.md](collision.md#collisionshape) |
| `TileCollision`, `CellValue` | `collision.h` | [collision.md](collision.md#tilecollision) |
| `SolidQuery` | `collision.h` | [collision.md](collision.md#solidquery) |

## What the module does

| The module does | Your game does |
|---|---|
| Moves a walker a pixel at a time and stops it at solids | Keeps each walker's position in its own state |
| Slides a walker along a wall on a diagonal | Says what is solid: a collision map or a callable |
| Moves in 8 directions or 4, set per config | Decides the heading of anything that is not the player |
| Sets the walker's facing from where it heads | Builds the routes |
| Shows 4 or 8 sprite directions, set per config | Provides the walk animations |
| Plays the walk animation while the walker moves | Declares the input actions and their bindings |
| Moves a walker along an engine `SpritePath` | Puts the sprite on a layer |
| Reads the four directions from the engine's input | |
| Returns the walker's sprite | |

## Feature status

| Feature | Status | Page |
|---|---|---|
| A walker bound to a position the game keeps | available | [concepts.md](concepts.md#the-position-a-walker-is-bound-to) |
| 8-way movement with sliding along walls | available | [concepts.md](concepts.md#a-tick-of-walking) |
| 4-way movement, by input or by heading | available | [concepts.md](concepts.md#four-directions-only) |
| Four facings, with the press order for input | available | [concepts.md](concepts.md#facing) |
| 4 or 8 sprite directions, a walk animation for each | available | [concepts.md](concepts.md#sprite-directions) |
| Walking by a heading from the game's own code | available | [how-to.md](how-to.md#wander-against-walls) |
| Routes, waits, playback modes and detours, through the engine's `SpritePath` | available | [routes.md](routes.md) |
| Movement slower than a pixel per tick | available on a route, or by walking on some ticks | [how-to.md](how-to.md#walk-slower-than-a-pixel-a-tick) |
| Collision shapes: box, any `ShapePoints`, polygon through anchors | available | [collision.md](collision.md#collision-shapes) |
| Solids from a collision map or any callable | available | [collision.md](collision.md#solids) |
| Reading directions from `retropp::InputState`, player slot 0 | available | [walker.md](walker.md#readdirections) |
| A pathway of your own, by deriving from `Walker` | available | [walker.md](walker.md#writing-your-own-pathway) |
| The walker's sprite, depth-sorted by the bottom of its collision shape | available | [walker.md](walker.md#walkersprite) |
| Collision on a route | not provided | [routes.md](routes.md#collision) |
| Collision between walkers | not provided; a recipe adds it | [how-to.md](how-to.md#stop-walkers-walking-through-each-other) |
| A route that crosses from one map to another | not provided | [routes.md](routes.md#collision) |
| A fractional `speed` | not provided; `speed` is whole pixels | [how-to.md](how-to.md#walk-slower-than-a-pixel-a-tick) |
| A collision map placed away from world pixel (0, 0) | not provided; a callable adds it | [collision.md](collision.md#limits) |
| More than one walkable value in a `TileCollision` | not provided; a callable adds it | [collision.md](collision.md#limits) |
| Reading directions for player slots other than 0 | not provided; build the input by hand | [how-to.md](how-to.md#build-a-directionalinput-by-hand) |
| Analog stick steering | not provided; an example shows how to add it | [walker.md](walker.md#writing-your-own-pathway) |
