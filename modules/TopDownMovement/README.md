# TopDownMovement — walking a top-down world

`TopDownMovement` gives a game on the [Polyrhythm](https://github.com/RetroPlusPlus/Polyrhythm) engine things
that walk a top-down world: stopped by walls, facing the way they go, playing a walk animation for the
way their sprite faces. Walking diagonally into a wall keeps them moving along it: the direction the
wall blocks stops, and the other carries on. A walker walks a position the game keeps, and it is moved
three ways: by the pad, by a heading the game works out, or along a route of the engine's own.

```cpp
// What a walker is. Two switches: 8-way or 4-way movement, 4 or 8 sprite directions.
const TopDownMovement::WalkerConfig heroConfig{
    .speed     = 1,
    .movement  = TopDownMovement::Ways::Eight,
    .sprites   = TopDownMovement::Ways::Four,
    .collision = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8},
    .walk      = {.up = walkUp, .down = walkDown, .left = walkLeft, .right = walkRight},
};
const TopDownMovement::TileCollision walls{.map = &collisionMap, .cellSize = 8, .open = Cell::Open};

// Where it stands is the game's; the walker is bound to it once.
retropp::Vec2i heroAt{72, 56};
TopDownMovement::InputWalker hero{heroAt};

// Each tick: the pad. A plain Walker takes a heading; follow moves one along a SpritePath.
hero.walk(heroConfig, TopDownMovement::readDirections(input, steering), walls);

// Each frame.
TopDownMovement::WalkerSprite("hero", hero, heroConfig);
```

| Piece | What it is | Page |
|---|---|---|
| `WalkerConfig`, `Ways` | what a walker is: speed, how it may move, how many sprite directions it has, collision shape, anchors, a walk animation per sprite direction | [docs/walker.md](docs/walker.md#walkerconfig) |
| `Walker` | something that walks: bound to a position, walked by a heading, moved by something else, turned, drawn | [docs/walker.md](docs/walker.md#walker) |
| `InputWalker` | a walker steered by the pad, which also keeps the order directions were pressed in | [docs/walker.md](docs/walker.md#inputwalker) |
| `routeFrom`, `follow` | a walker moved along the engine's `SpritePath` | [docs/routes.md](docs/routes.md) |
| `Facing` | the eight directions a walker's sprite can face | [docs/walker.md](docs/walker.md#facing) |
| `CollisionShape`, `TileCollision` | what a walker collides with, and a collision map's solid cells | [docs/collision.md](docs/collision.md) |
| `readDirections` | the four directions, read from the engine's input through the game's own actions | [docs/walker.md](docs/walker.md#reading-the-directions) |
| `WalkerSprite` | the sprite a walker is | [docs/walker.md](docs/walker.md#walkersprite) |

The module is header-only, depends on the engine alone (`retropp::engine`), includes no header from the
game it sits in, and needs C++20. It is meant to leave this repository as a standalone module.

## Using it

```cmake
add_subdirectory(modules/TopDownMovement)
target_link_libraries(my-game PRIVATE TopDownMovement::TopDownMovement)
```

The directory's `CMakeLists.txt` defines the INTERFACE target `TopDownMovement` (alias
`TopDownMovement::TopDownMovement`) and expects the engine as the target `retropp::engine`.
`TOPDOWNMOVEMENT_BUILD_EXAMPLES` (on by default when the directory is the top-level project) builds
`TopDownMovement-example-getting-started`, the program [docs/getting-started.md](docs/getting-started.md)
walks through.

## Documentation

| Page | Read it for |
|---|---|
| **[Getting started](docs/getting-started.md)** | **Start here:** the example program, three walkers in a room, part by part |
| [Concepts](docs/concepts.md) | The model: the config, the bound position, the three pathways, facing and sprite directions |
| [Routes](docs/routes.md) | Walking the engine's `SpritePath`: waits, detours, facing from travel |
| [Collision](docs/collision.md) | What is solid, what collides, and how the two meet each tick |
| [How-to](docs/how-to.md) | Recipes: patrols, wandering, four directions only, diagonal sprites and more |
| [Walker reference](docs/walker.md) | Every type and member |
| [Guide index](docs/README.md) | Every page, and what the module covers and leaves to the game |

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
  examples/getting_started.cpp      the getting-started program
  examples/assets/                  its sheets and palette images
  docs/                             the guide
```

`walker_sprite.h` is the only header that builds a draw type (`retropp::Sprite`), so a game's
simulation can include the rest without anything to draw. `route.h` is the only header that includes the
engine's `retropp/sprite_path.h`.
