# Getting started

This page builds a complete program on the module: a walled room with four pillars and three walkers,
each moved a different way.

- **The hero** is steered with the arrow keys, WASD or a d-pad.
- **The wanderer** walks a heading until something stops it, then turns clockwise.
- **The guard** patrols a route along the bottom of the room, resting at its east end.

Two keys switch all three between the module's settings. **Z** (or a pad's A button) toggles 8-way and
4-way movement, and **X** (or B) toggles 4 and 8 sprite directions.

The full source is [`examples/getting_started.cpp`](../examples/getting_started.cpp), and its art is in
[`examples/assets/`](../examples/assets/). It builds as the `TopDownMovement-example-getting-started`
target whenever `TOPDOWNMOVEMENT_BUILD_EXAMPLES` is on, so what you read here is what compiles.

## Contents

- [1. Build it](#1-build-it)
- [2. Run it](#2-run-it)
- [3. The art](#3-the-art)
- [4. The room and its collision map](#4-the-room-and-its-collision-map)
- [5. The walk animations](#5-the-walk-animations)
- [6. What each walker is](#6-what-each-walker-is)
- [7. Where each walker stands](#7-where-each-walker-stands)
- [8. The guard's route](#8-the-guards-route)
- [9. The tick](#9-the-tick)
- [10. Drawing it](#10-drawing-it)
- [11. Where to go next](#11-where-to-go-next)

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

The executable lands in the build tree under `modules/TopDownMovement/`. Close the window to quit.

## 2. Run it

A dark green floor inside a lighter wall, four square pillars, and three arrowheads: gold in the
middle, green near the top, red at the bottom left.

| What you do | What happens |
|---|---|
| Walk the gold one | It turns to point the way it goes and bobs a pixel back and forth with each step. Walked diagonally into a wall or a pillar, it slides along it. |
| Watch the green one | It walks right until a wall stops it, then turns an eighth of a turn clockwise and walks on. Heading diagonally, it slides along a wall until it reaches a corner. |
| Watch the red one | It walks right along the bottom row at half a pixel a tick, stands for half a second at the end, and walks back. It walks through nothing: its route is drawn in open floor. |
| Press Z | The blue cell in the top-left corner turns from a star to a plus: all three move in four directions only. The gold one held on two axes moves along the one you pressed last. |
| Press X | The orange cell beside it turns from a plus to a star: the arrowheads point diagonally when they head diagonally. |

## 3. The art

[`examples/assets/`](../examples/assets/) holds two sheets of 8×8 cells and six palette images. Open them
in any image viewer alongside this page.

| File | Size | Cells, in read order |
|---|---|---|
| `tiles.png` | 32×8 | floor, wall, a plus, a star |
| `walker.png` | 16×64 | eight rows of two frames: an arrowhead pointing up, up-right, right, down-right, down, down-left, left, up-left |

Each sheet is an indexed PNG whose pixels are palette indices, not colors. The tiles draw all four
indices. On the walker sheet index 0 is transparent, 1 is an outline, and 2 and 3 are the arrowhead's
edge and body. Each row's second frame sits a pixel further along, so walking reads as a bob.

A palette image is 4×1 pixels, one per entry. The room's tiles draw through `palettes/room.png`, the
two indicators through `palettes/movement.png` (blue) and `palettes/sprites.png` (orange). The one walker
sheet draws all three walkers, each through its own palette: `palettes/hero.png`,
`palettes/wanderer.png`, `palettes/guard.png`.

```cpp
.walker = renderer.loadAtlasFromMemory(readAsset("walker.png"), retropp::AssetDimensions{.width = kCell, .height = kCell},
                                       retropp::ContentKind::AnimationSeries, retropp::ReadOrder::LeftRightThenDown,
                                       /*count=*/0, retropp::TransparentIndices::of({0}),
                                       /*framesPerAnimation=*/2),
```

The program reads the files' bytes from the directory the build names in `TOPDOWNMOVEMENT_EXAMPLE_ASSETS`
and hands them to the engine's `loadAtlasFromMemory` and `loadPngFromMemory`.

## 4. The room and its collision map

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

A game usually loads this from a map PNG with `retropp::loadMapPng`; it is built in code here because it
is four rectangles. The same grid is drawn: each cell becomes a floor or wall `TileCell` on the room's tile
layer, and the two corner cells show the switches.

The module reads it through a `TileCollision`, which says how big a cell is and which value is open.
Everything else, and everything off the map, is solid:

```cpp
const TopDownMovement::TileCollision walls{.map = &room, .cellSize = kCell, .open = Cell::Open};
```

## 5. The walk animations

Each row of the walker sheet is one sprite direction's walk animation, two frames of eight ticks:

```cpp
retropp::Animation WalkRow(const Art& art, TopDownMovement::Facing facing, retropp::PaletteId palette) {
    const std::size_t first = static_cast<std::size_t>(facing) * 2;
    return retropp::Animation{{
        {.sheet = art.walker.atlasId, .tileIndex = first, .palette = palette, .duration = 8 * 1s / 60},
        {.sheet = art.walker.atlasId, .tileIndex = first + 1, .palette = palette, .duration = 8 * 1s / 60},
    }};
}
```

The sheet's rows are in the order of `TopDownMovement::Facing`, so a direction is its own row number.
`WalkAnimations(art, palette)` builds all eight, one per field of `DirectionalAnimations`: `up`, `down`,
`left`, `right`, and the four diagonals. The diagonals are read only while the sprites are set to eight
directions. These are ordinary engine animations (the engine's `docs/guide/animation.md`); the module
plays them.

## 6. What each walker is

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

- **`speed`**: one pixel per tick on each axis a walker heads along.
- **`collision`**: a 6×4 box across the bottom of the 8×8 art, so the tip of an arrowhead can overlap a
  wall above it while its base cannot. It is in the walker's own pixels; the module moves it with the
  walker. A circle, a polygon or named points in the art work too ([collision.md](collision.md)).
- **`walk`**: the eight animations, in the walker's own colors.
- **`movement`** and **`sprites`** are left at their defaults, 8-way movement and 4 sprite directions.
  Z and X change them on all three configs at once.

A config says what a walker *is*. The three here differ only in color; in a game they differ in
everything.

## 7. Where each walker stands

```cpp
retropp::Vec2i heroAt{76, 68};
retropp::Vec2i wandererAt{76, 24};
retropp::Vec2i guardAt{16, 120};

TopDownMovement::InputWalker hero{heroAt};
TopDownMovement::Walker wanderer{wandererAt};
TopDownMovement::Walker guard{guardAt};
```

Where a walker stands is the program's: three positions it keeps, each walker bound to its own when it is
made. The walker reads and writes that position from then on, so `heroAt` is always where the hero is.
In a game the position is wherever the thing that stands there keeps it
([concepts.md](concepts.md#the-position-a-walker-is-bound-to)).

The hero is an `InputWalker` because the pad steers it, and only the pad needs to remember which
direction was pressed last. The other two are plain walkers.

## 8. The guard's route

```cpp
retropp::SpritePath patrol = TopDownMovement::routeFrom(guard, {
    {.label = "east", .move = retropp::SpritePathMove::to({136.0f, 120.0f}), .pacing = retropp::PathPacing::speed(30.0f)},
    {.label = "wait", .move = retropp::SpritePathMove::to({136.0f, 120.0f}), .pacing = retropp::PathPacing::eased(500ms)},
    {.label = "west", .move = retropp::SpritePathMove::to({16.0f, 120.0f}), .pacing = retropp::PathPacing::speed(30.0f)},
});
```

The route is the engine's `SpritePath`: three legs played back to back, each departing from where the
last ended. `routeFrom` starts it where the guard stands.

- **`east`** and **`west`** are straight legs at 30 pixels a second, half a pixel a tick at the program's
  60 ticks a second.
- **`wait`** is the engine's wait node: a leg to the point it starts from, paced over half a second, so
  the guard stands there for that long.

The engine's guide (`docs/guide/sprite-path.md`) covers everything else a route can do: curved legs,
other paces, playing once or N times, detours. [routes.md](routes.md) covers a walker on one.

## 9. The tick

```cpp
std::size_t heading = 0;   // the wanderer's, in kClockwise
loop.simTick([&](const retropp::InputState& input) {
    // … Z and X flip movement and sprites on all three configs …

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

One line per pathway.

- **The hero.** `readDirections` reads which of the program's four actions are held and which were just
  pressed; `presets::directional` binds them to the arrows, WASD and the d-pad. `walk` moves the hero a
  pixel along each held axis if its collision box there is clear, turns it, and advances its walk
  animation ([concepts.md](concepts.md#a-tick-of-walking)).
- **The wanderer.** It is handed a heading, one of eight in `kClockwise`, and walks it against the same
  walls. When `moving()` says it got nowhere, the program turns it to the next.
- **The guard.** `follow` advances the patrol a tick and puts the guard where the route is now, facing
  the way it went. It walks on every tick the route moves, so its arrowhead bobs at half a pixel a tick,
  and stands on frame 0 through the wait. Nothing collides on a route, which is why this one is drawn in
  open floor.

## 10. Drawing it

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

`WalkerSprite` returns a walker's current frame as a `retropp::Sprite` at its position, whichever way it
was moved: the frame of the animation for the way its sprite faces. The sprites' `z` is the bottom of
each one's collision box, so a walker lower on the screen draws over one above it. The program puts them
on a plain engine sprite layer; a game that builds its screen from components puts them on whatever
sprite layer it uses. The sprite and cell vectors live outside the render callback because a layer's
content is a span the renderer reads while it draws.

## 11. Where to go next

- The model: the config, the bound position, the three pathways, facing and sprite directions —
  [concepts.md](concepts.md).
- Routes: the engine's pathing, waits and detours, facing from travel — [routes.md](routes.md).
- How collision works and what can be solid — [collision.md](collision.md).
- Recipes: patrols, wandering, four directions only, diagonal sprites, a walker in a copyable struct —
  [how-to.md](how-to.md).
- Every type and member — [walker.md](walker.md).
