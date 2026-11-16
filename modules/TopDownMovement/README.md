# TopDownMovement

Walking characters for a top-down game on the [Polyrhythm](https://github.com/RetroPlusPlus/Polyrhythm)
engine.

A walker moves a pixel at a time, stops at walls, slides along them on a diagonal, faces the way it
goes, and plays a walk animation while it moves. You can move one three ways: by the player's input, by
a heading your code computes, or along a route.

```cpp
#include "TopDownMovement/input_walker.h"    // InputWalker, WalkerConfig, TileCollision, readDirections
#include "TopDownMovement/walker_sprite.h"   // WalkerSprite

// The config: what a hero is. Build it once, after the art is loaded.
const TopDownMovement::WalkerConfig heroConfig{
    .speed     = 1,
    .collision = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8},
    .walk      = {.up = walkUp, .down = walkDown, .left = walkLeft, .right = walkRight},
};

// The position is a value in your game state. The walker is bound to it.
retropp::Vec2i heroAt{72, 56};
TopDownMovement::InputWalker hero{heroAt};

// Every tick.
const TopDownMovement::TileCollision walls{.map = &collisionMap, .cellSize = 8, .open = Cell::Open};
hero.walk(heroConfig, TopDownMovement::readDirections(input, steering), walls);

// Every frame.
const retropp::Sprite sprite = TopDownMovement::WalkerSprite("hero", hero, heroConfig);
```

[docs/concepts.md](docs/concepts.md#overview) explains each line.

## Contents

| Type or function | Purpose | Reference |
|---|---|---|
| `WalkerConfig` | Describes a kind of walker: speed, 8-way or 4-way movement, 4 or 8 sprite directions, collision shape, walk animations. | [docs/walker.md](docs/walker.md#walkerconfig) |
| `Walker` | One walking character. Moved by a heading. | [docs/walker.md](docs/walker.md#walker) |
| `InputWalker` | A `Walker` moved by the player's input. | [docs/walker.md](docs/walker.md#inputwalker) |
| `routeFrom`, `follow` | Move a walker along the engine's `SpritePath`. | [docs/routes.md](docs/routes.md) |
| `CollisionShape` | The pixels of a walker that collide: a box, a circle, a polygon. | [docs/collision.md](docs/collision.md#collisionshape) |
| `TileCollision` | Solids read from a collision map. | [docs/collision.md](docs/collision.md#tilecollision) |
| `readDirections` | Reads the four directions from the engine's input. | [docs/walker.md](docs/walker.md#readdirections) |
| `WalkerSprite` | Returns the sprite for a walker. | [docs/walker.md](docs/walker.md#walkersprite) |

## Requirements

- C++20
- The Polyrhythm engine, as the CMake target `retropp::engine`

The module is header-only and depends on the engine alone.

## Install

```cmake
add_subdirectory(modules/TopDownMovement)
target_link_libraries(my-game PRIVATE TopDownMovement::TopDownMovement)
```

| CMake option | Default | Effect |
|---|---|---|
| `TOPDOWNMOVEMENT_BUILD_EXAMPLES` | `ON` when this directory is the top-level project, otherwise `OFF` | Builds the example program as the target `TopDownMovement-example-getting-started`. |

## Documentation

| Page | Read it for |
|---|---|
| **[Getting started](docs/getting-started.md)** | **Start here.** A complete example program, explained step by step. |
| [Concepts](docs/concepts.md) | How the module works. |
| [Collision](docs/collision.md) | Solids and collision shapes. |
| [Routes](docs/routes.md) | Patrols, waits and detours. |
| [How-to](docs/how-to.md) | Recipes for specific tasks. |
| [Troubleshooting](docs/troubleshooting.md) | Symptoms, causes and fixes. |
| [Testing](docs/testing.md) | Testing code that uses the module. |
| [Walker reference](docs/walker.md) | Every type and member. |
| [Guide index](docs/README.md) | Every page, a task index, a name index, and what the module provides and what it leaves to your game. |

## Layout

```
modules/TopDownMovement/
  CMakeLists.txt                    the TopDownMovement::TopDownMovement target and the example option
  include/TopDownMovement/
    directions.h                    Ways, Facing, Directions, DirectionalInput, DirectionalActions, readDirections
    collision.h                     CollisionShape, TileCollision, CellValue, SolidQuery
    walker.h                        DirectionalAnimations, WalkerConfig, Walker
    input_walker.h                  InputWalker
    route.h                         routeFrom, follow
    walker_sprite.h                 WalkerSprite
  examples/getting_started.cpp      the example program
  examples/assets/                  its sheets and palette images
  docs/                             the guide
```

| Header | Note |
|---|---|
| `walker_sprite.h` | The only header that creates a draw type (`retropp::Sprite`). The other headers are for simulation code. |
| `route.h` | The only header that includes the engine's `retropp/sprite_path.h`. |

## License

MIT. See [LICENSE](LICENSE).
