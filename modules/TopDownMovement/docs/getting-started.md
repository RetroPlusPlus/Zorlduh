# Getting started

This page builds a complete program on the module: a walled room with four pillars, and a walker you
steer with the arrow keys, WASD or a d-pad. It walks in eight directions, stops at the walls and pillars,
keeps moving along a wall it walks into diagonally, faces where it walks and steps through its walk
animation.

The full source is [`examples/getting_started.cpp`](../examples/getting_started.cpp). It builds as the
`TopDownMovement-example-getting-started` target whenever `TOPDOWNMOVEMENT_BUILD_EXAMPLES` is on, so
what you read here is what compiles.

## Contents

- [1. Build it](#1-build-it)
- [2. The room and its collision map](#2-the-room-and-its-collision-map)
- [3. The walk animations](#3-the-walk-animations)
- [4. What the walker is](#4-what-the-walker-is)
- [5. The tick](#5-the-tick)
- [6. Drawing it](#6-drawing-it)
- [7. Where to go next](#7-where-to-go-next)

## 1. Build it

The module needs the engine as the CMake target `retropp::engine`; the engine's
`docs/guide/build-and-consume.md` covers building it. Add the module and turn its examples on:

```cmake
add_subdirectory(engine)
set(TOPDOWNMOVEMENT_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(modules/TopDownMovement)
```

```sh
cmake -S . -B build
cmake --build build --target TopDownMovement-example-getting-started
```

The executable lands in the build tree under `modules/TopDownMovement/`. Run it: a dark floor inside a
lighter wall, four square pillars, and a pale arrowhead in the middle pointing down. Walk, and it turns
to point the way it walks and bobs a pixel back and forth with each step. Close the window to quit.

## 2. The room and its collision map

The room is a collision map: an `IndexGrid` of 20×18 cells, one value per cell, wall round the edge and
four 2×2 pillars.

```cpp
enum class Cell : std::uint16_t { Open = 0, Wall = 1 };

retropp::IndexGrid Room() {
    retropp::IndexGrid room{.width = kRoomWidth, .height = kRoomHeight};
    room.values.resize(static_cast<std::size_t>(kRoomWidth) * kRoomHeight);
    // ... Cell::Wall on the edge and on the pillars, Cell::Open elsewhere ...
    return room;
}
```

A game usually loads this from a map PNG with `retropp::loadMapPng`; it is built in code here so the
example has no files. The same grid is drawn: each cell becomes a floor or wall `TileCell` on the room's
tile layer.

The module reads it through a `TileCollision`, which says how big a cell is and which value is open.
Everything else, and everything off the map, is solid:

```cpp
const TopDownMovement::TileCollision walls{.map = &room, .cellSize = kCell, .open = Cell::Open};
```

## 3. The walk animations

The walker sheet holds four rows of two 8×8 frames, one row per facing (Up, Down, Left, Right). It is
uploaded as an `AnimationSeries` so each frame is a slot:

```cpp
art.walker = renderer.uploadAtlas(walker.data(), 16, 32, retropp::AssetDimensions{.width = 8, .height = 8},
                                  retropp::ContentKind::AnimationSeries, retropp::ReadOrder::LeftRightThenDown,
                                  /*count=*/0, retropp::TransparentIndices::of({0}), /*framesPerAnimation=*/2);
```

Each facing's walk animation is that row's two frames, eight ticks each:

```cpp
retropp::Animation WalkRow(const Art& art, std::size_t row) {
    constexpr auto kFrame = 8 * std::chrono::nanoseconds{
                                    static_cast<std::chrono::nanoseconds::rep>(retropp::TickPeriodNs::Hz60)};
    return retropp::Animation{{
        {.sheet = art.walker.atlasId, .tileIndex = row * 2, .palette = art.palette, .duration = kFrame},
        {.sheet = art.walker.atlasId, .tileIndex = row * 2 + 1, .palette = art.palette, .duration = kFrame},
    }};
}
```

These are ordinary engine animations (the engine's `docs/guide/animation.md`); the module plays them.

## 4. What the walker is

```cpp
const TopDownMovement::WalkerConfig hero{
    .speed = 1,
    .collision = retropp::IntRect{.x = 1, .y = 4, .width = 6, .height = 4},
    .walk = {.up = WalkRow(art, 0), .down = WalkRow(art, 1),
             .left = WalkRow(art, 2), .right = WalkRow(art, 3)},
};
```

- **`speed`**: one pixel per tick on each axis held.
- **`collision`**: a 6×4 box across the bottom of the 8×8 art, so the top of the arrowhead can overlap a
  wall above it while its base cannot. It is in the walker's own pixels; the module moves it with the
  walker. A circle, a polygon or named points in the art work too ([collision.md](collision.md)).
- **`walk`**: the four animations, one per facing.

## 5. The tick

```cpp
const TopDownMovement::DirectionalActions steering{
    .up = Action::Up, .down = Action::Down, .left = Action::Left, .right = Action::Right};

TopDownMovement::Walker walker{{76, 68}};
loop.simTick([&](const retropp::InputState& input) {
    walker.walk(hero, TopDownMovement::readDirections(input, steering), walls);
});
```

`readDirections` reads which of the program's four actions are held and which were just pressed.
`presets::directional` binds them to the arrows, WASD and the d-pad. `walk` does the rest: it moves the
walker a pixel along each held axis if its collision box there is clear, turns it, and advances its
walk animation ([concepts.md](concepts.md#a-tick-of-walking)).

The walker is a plain value. The program keeps it in a local; a game keeps it in its state.

## 6. Drawing it

```cpp
sprites = {TopDownMovement::WalkerSprite("hero", walker, hero)};
frame.layers = {
    retropp::DrawLayer{.key = "room", .z = 0, ..., .content = retropp::TileContent{...}},
    retropp::DrawLayer{.key = "walkers", .z = 10, ..., .content = retropp::SpriteContent{.sprites = sprites}},
};
```

`WalkerSprite` returns the walker's current frame as a `retropp::Sprite` at its position. The program
puts it on a plain engine sprite layer; a game that builds its screen from components puts it on
whatever sprite layer it uses. The sprite vector lives outside the render callback because the layer's
content is a span the renderer reads while it draws.

## 7. Where to go next

- The model: the config and the walker, a tick of walking, the facing law — [concepts.md](concepts.md).
- How collision works and what can be solid — [collision.md](collision.md).
- Recipes: switching maps, combining solids, different rules per walker — [how-to.md](how-to.md).
- Every type and member — [walker.md](walker.md).
