# TopDownMovement — walking a top-down world

`TopDownMovement` gives a game on the [Polyrhythm](https://github.com/RetroPlusPlus/Polyrhythm) engine things
that walk a top-down world: in eight directions, stopped by walls, facing one of four ways, playing a
walk animation for the way they face. Walking diagonally into a wall keeps them moving along it: the
direction the wall blocks stops, and the other carries on. A game says what a walker is once, in a config, and then walks it
one tick at a time and draws it as a sprite.

```cpp
// What the hero is.
const TopDownMovement::WalkerConfig hero{
    .speed = 1,
    .collision = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8},
    .walk = {.up = walkUp, .down = walkDown, .left = walkLeft, .right = walkRight},
};
const TopDownMovement::TileCollision walls{.map = &collisionMap, .cellSize = 8, .open = Cell::Open};

// Each tick.
walker.walk(hero, TopDownMovement::readDirections(input, {.up = Action::Up, .down = Action::Down,
                                                   .left = Action::Left, .right = Action::Right}),
            walls);

// Each frame.
TopDownMovement::WalkerSprite("hero", walker, hero);
```

| Piece | What it is | Page |
|---|---|---|
| `WalkerConfig` | what a walker is: speed, collision shape, anchors, a walk animation per facing | [docs/walker.md](docs/walker.md) |
| `Walker` | one walker's position, facing and place in its stride, and `walk`, which moves it a tick | [docs/walker.md](docs/walker.md) |
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
`TopDownMovement::TopDownMovement`) and expects the engine as the target `retropp::engine`. `TOPDOWNMOVEMENT_BUILD_EXAMPLES` (on by default when the
directory is the top-level project) builds `TopDownMovement-example-getting-started`, the program
[docs/getting-started.md](docs/getting-started.md) walks through.

## Documentation

**Start with [docs/getting-started.md](docs/getting-started.md)**, then [docs/concepts.md](docs/concepts.md)
for the model and [docs/collision.md](docs/collision.md) for how collision works. The full index is
[docs/README.md](docs/README.md).

## Layout

```
modules/TopDownMovement/
  CMakeLists.txt                    the TopDownMovement::TopDownMovement target and the example option
  include/TopDownMovement/
    directions.h                    Directions, DirectionalInput, DirectionalActions, readDirections
    collision.h                     CollisionShape, TileCollision, CellValue, SolidQuery
    walker.h                        DirectionalAnimations, WalkerConfig, Walker
    walker_sprite.h                 WalkerSprite
  examples/getting_started.cpp      the getting-started program
  docs/                             the guide
```

`walker_sprite.h` is the only header that builds a draw type (`retropp::Sprite`), so a game's
simulation can include the rest without anything to draw.
