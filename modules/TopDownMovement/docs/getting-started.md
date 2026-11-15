# Getting started

This page walks through a complete program that uses the module: a walled room with four pillars and
three walkers, one for each way of moving a walker.

| Walker | Color | Moved by |
|---|---|---|
| The hero | gold | the arrow keys, WASD or a d-pad |
| The wanderer | green | a heading that the program changes when the walker is blocked |
| The guard | red | a route along the bottom of the room |

The source is [`examples/getting_started.cpp`](../examples/getting_started.cpp) and its art is in
[`examples/assets/`](../examples/assets/). The code on this page is taken from that file.

## Contents

- [1. Build it](#1-build-it)
- [2. Run it](#2-run-it)
- [3. The art](#3-the-art)
- [4. The room and its collision map](#4-the-room-and-its-collision-map)
- [5. The walk animations](#5-the-walk-animations)
- [6. The configs](#6-the-configs)
- [7. The positions and the walkers](#7-the-positions-and-the-walkers)
- [8. The guard's route](#8-the-guards-route)
- [9. The tick](#9-the-tick)
- [10. Drawing](#10-drawing)
- [11. Next](#11-next)

## 1. Build it

The module needs the engine as the CMake target `retropp::engine`. The engine's
`docs/guide/build-and-consume.md` covers building it. Add the module and turn its example on:

```cmake
add_subdirectory(engine)
set(TOPDOWNMOVEMENT_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(modules/TopDownMovement)
```

```sh
cmake -S . -B build
cmake --build build --target TopDownMovement-example-getting-started
```

The executable is written to `build/modules/TopDownMovement/`. Close the window to quit.

## 2. Run it

The window shows a dark green floor inside a lighter wall, four square pillars, and three arrowheads.

| Do this | Result |
|---|---|
| Hold a direction | The gold arrowhead moves, points the way it goes, and bobs one pixel with each step. |
| Hold two directions into a wall or a pillar | It slides along the wall. |
| Watch the green arrowhead | It walks until it is blocked, then turns 45° clockwise and walks again. |
| Watch the red arrowhead | It walks right along the bottom row at half a pixel per tick, stands for half a second, and walks back. |
| Press Z, or A on a pad | The blue cell in the top-left corner changes from a star to a plus. All three walkers now move in four directions only. |
| Press X, or B on a pad | The orange cell beside it changes from a plus to a star. All three walkers now show diagonal sprites when they move diagonally. |

A plus means four and a star means eight. Press the key again to switch back.

## 3. The art

`examples/assets/` holds two sheets of 8×8 cells and six palette images.

| File | Size | Contents |
|---|---|---|
| `tiles.png` | 32×8 | four cells: floor, wall, a plus, a star |
| `walker.png` | 16×64 | eight rows of two frames. Each row is an arrowhead pointing one way: up, up-right, right, down-right, down, down-left, left, up-left |
| `palettes/room.png` | 4×1 | the colors of the floor and wall |
| `palettes/movement.png`, `palettes/sprites.png` | 4×1 | the colors of the two indicator cells: blue and orange |
| `palettes/hero.png`, `palettes/wanderer.png`, `palettes/guard.png` | 4×1 | the colors of each walker |

The sheets are indexed PNGs: each pixel is a palette index, and a palette image supplies the colors. On
the walker sheet, index 0 is transparent. The three walkers share the one walker sheet and differ only in
palette.

The program reads the files from the directory named by `TOPDOWNMOVEMENT_EXAMPLE_ASSETS`, which the
module's `CMakeLists.txt` defines, and loads them with the engine's `loadAtlasFromMemory`:

```cpp
.walker = renderer.loadAtlasFromMemory(readAsset("walker.png"), retropp::AssetDimensions{.width = kCell, .height = kCell},
                                       retropp::ContentKind::AnimationSeries, retropp::ReadOrder::LeftRightThenDown,
                                       /*count=*/0, retropp::TransparentIndices::of({0}),
                                       /*framesPerAnimation=*/2),
```

The engine's `docs/guide/images-and-transparency.md` covers sheets and
`docs/guide/tiles-and-color.md` covers palettes.

## 4. The room and its collision map

The room is a `retropp::IndexGrid` of 20×18 cells. Each cell holds `Cell::Open` or `Cell::Wall`. The
edge and four 2×2 pillars are wall.

```cpp
enum class Cell : std::uint16_t { Open = 0, Wall = 1 };

retropp::IndexGrid Room() {
    retropp::IndexGrid room{.width = kRoomWidth, .height = kRoomHeight};
    room.values.resize(static_cast<std::size_t>(kRoomWidth) * kRoomHeight);
    // ... Cell::Wall on the edge and on the pillars, Cell::Open elsewhere ...
    return room;
}
```

The program builds the grid in code. A game usually loads one from a PNG with `retropp::loadMapPng`.

A `TileCollision` turns the grid into solids. It needs the size of a cell in pixels and the value that
can be walked on:

```cpp
const TopDownMovement::TileCollision walls{.map = &room, .cellSize = kCell, .open = Cell::Open};
```

Every cell except `Cell::Open` is solid, and so is everything outside the grid. The same grid is
also drawn: each cell becomes a floor or wall tile.

## 5. The walk animations

Each row of the walker sheet is the walk animation for one direction: two frames, eight ticks each.

```cpp
retropp::Animation WalkRow(const Art& art, TopDownMovement::Facing facing, retropp::PaletteId palette) {
    const std::size_t first = static_cast<std::size_t>(facing) * 2;
    return retropp::Animation{{
        {.sheet = art.walker.atlasId, .tileIndex = first, .palette = palette, .duration = 8 * 1s / 60},
        {.sheet = art.walker.atlasId, .tileIndex = first + 1, .palette = palette, .duration = 8 * 1s / 60},
    }};
}
```

The sheet's rows are in the order of the `TopDownMovement::Facing` enum, so the row number is the enum
value.

`WalkAnimations(art, palette)` calls `WalkRow` eight times and returns a `DirectionalAnimations`: one
animation for each of `up`, `down`, `left`, `right`, `upLeft`, `upRight`, `downLeft` and `downRight`.

These are ordinary engine animations (the engine's `docs/guide/animation.md`). The module chooses which
one to show and which frame.

## 6. The configs

A `WalkerConfig` describes a kind of walker. The three walkers have the same speed and shape and
different palettes, so one function builds all three configs:

```cpp
TopDownMovement::WalkerConfig WalkerOf(const Art& art, retropp::PaletteId palette) {
    return TopDownMovement::WalkerConfig{
        .speed = 1,
        .collision = retropp::IntRect{.x = 1, .y = 4, .width = 6, .height = 4},
        .walk = WalkAnimations(art, palette),
    };
}

TopDownMovement::WalkerConfig heroConfig = WalkerOf(art, art.hero);
TopDownMovement::WalkerConfig wandererConfig = WalkerOf(art, art.wanderer);
TopDownMovement::WalkerConfig guardConfig = WalkerOf(art, art.guard);
```

| Field | Value | Meaning |
|---|---|---|
| `speed` | `1` | one pixel per tick on each axis the walker heads along |
| `collision` | a 6×4 box at the bottom of the 8×8 frame | the part of the walker that collides. The tip of the arrowhead can overlap a wall above it. |
| `walk` | the eight animations | what the walker shows |
| `movement` | the default, `Ways::Eight` | moves in eight directions |
| `sprites` | the default, `Ways::Four` | shows four sprite directions |

Z and X change `movement` and `sprites` on all three configs while the program runs. The configs are
mutable for that reason; in a game they are usually `const`.

## 7. The positions and the walkers

```cpp
retropp::Vec2i heroAt{76, 68};
retropp::Vec2i wandererAt{76, 24};
retropp::Vec2i guardAt{16, 120};

TopDownMovement::InputWalker hero{heroAt};
TopDownMovement::Walker wanderer{wandererAt};
TopDownMovement::Walker guard{guardAt};
```

The program keeps the three positions. Each walker is bound to one of them and reads and writes it from
then on, so `heroAt` is always where the hero is. A position is the top-left of the walker's frame, in
pixels.

The hero is an `InputWalker` because the pad steers it. The other two are plain `Walker`s.

In a game the positions are fields of your game state
([concepts.md](concepts.md#the-position-a-walker-is-bound-to)).

## 8. The guard's route

```cpp
retropp::SpritePath patrol = TopDownMovement::routeFrom(guard, {
    {.label = "east", .move = retropp::SpritePathMove::to({136.0f, 120.0f}), .pacing = retropp::PathPacing::speed(30.0f)},
    {.label = "wait", .move = retropp::SpritePathMove::to({136.0f, 120.0f}), .pacing = retropp::PathPacing::eased(500ms)},
    {.label = "west", .move = retropp::SpritePathMove::to({16.0f, 120.0f}), .pacing = retropp::PathPacing::speed(30.0f)},
});
```

`routeFrom` returns a `retropp::SpritePath`, the engine's route type, that starts where the guard
stands. The route has three legs, played in order:

| Leg | Goes to | Pace | Result |
|---|---|---|---|
| `east` | (136, 120) | 30 pixels per second | walks right |
| `wait` | (136, 120), where it already is | half a second | stands |
| `west` | (16, 120), where the route started | 30 pixels per second | walks back |

At the program's 60 ticks per second, 30 pixels per second is half a pixel per tick. The route ends
where it starts, so it loops smoothly.

[routes.md](routes.md) covers routes.

## 9. The tick

```cpp
std::size_t heading = 0;   // the wanderer's, in kClockwise
loop.simTick([&](const retropp::InputState& input) {
    // ... Z and X flip movement and sprites on all three configs ...

    // The input pathway: the pad.
    hero.walk(heroConfig, TopDownMovement::readDirections(input, steering), walls);

    // The free pathway: a heading from this program's own rule.
    wanderer.walk(wandererConfig, kClockwise[heading], walls);
    if (!wanderer.moving()) {
        heading = (heading + 1) % kClockwise.size();
    }

    // The route pathway: one tick along the patrol.
    TopDownMovement::follow(guard, guardConfig, patrol);
});
```

Each walker gets one call per tick.

**The hero.** `readDirections` reads the four direction actions from the tick's input and returns the
directions held and pressed. `steering` names the program's four actions; `presets::directional` binds
them to the arrow keys, WASD and the d-pad. `walk` moves the hero against `walls`, sets its facing and
advances its walk animation.

**The wanderer.** `kClockwise` is an array of eight headings, clockwise from right. The wanderer walks
the current one. `moving()` returns `false` after a tick in which it was blocked, and the program
then selects the next heading.

**The guard.** `follow` advances the patrol by one tick and moves the guard to the route's position. The
guard faces the way the route travels, plays its walk animation while the route moves, and shows frame 0
during the wait. A walker on a route goes exactly where the route goes, so this route is drawn through
open floor.

## 10. Drawing

```cpp
sprites = {
    TopDownMovement::WalkerSprite("hero", hero, heroConfig),
    TopDownMovement::WalkerSprite("wanderer", wanderer, wandererConfig),
    TopDownMovement::WalkerSprite("guard", guard, guardConfig),
};
frame.layers = {
    retropp::DrawLayer{.key = "room", .z = 0, ..., .content = retropp::TileContent{...}},
    retropp::DrawLayer{.key = "walkers", .z = 10, ..., .content = retropp::SpriteContent{.sprites = sprites}},
};
```

`WalkerSprite` returns a `retropp::Sprite` for a walker: its current frame at its position. The three
sprites go on one sprite layer above the room's tile layer.

Each sprite's `z` is the bottom of its walker's collision box, so a walker lower on the screen draws in
front of one above it.

The `sprites` vector is declared outside the render callback. A layer's content is a span, and the
vector must stay alive while the renderer reads it.

## 11. Next

| To learn | Read |
|---|---|
| How the parts fit together and how a walker behaves | [concepts.md](concepts.md) |
| How collision works, and what can be solid | [collision.md](collision.md) |
| Routes: waits, detours, playback modes | [routes.md](routes.md) |
| How to do a specific task | [how-to.md](how-to.md) |
| How to fix a problem | [troubleshooting.md](troubleshooting.md) |
| How to test code that uses the module | [testing.md](testing.md) |
| Every type and member | [walker.md](walker.md) |
