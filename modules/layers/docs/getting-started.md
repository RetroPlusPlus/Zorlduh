# Getting started

This page builds a complete program on the kit: a walled floor of tiles, a player you steer with the arrow
keys or a d-pad, a full-colour badge in the corner, and a meter painted on the CPU that fills as the player
crosses the floor. All four kinds of layer are in it, and the whole frame is one function's return value.

The full source is [`examples/getting_started.cpp`](../examples/getting_started.cpp). It builds as the
`layers-example-getting-started` target whenever `LAYERS_BUILD_EXAMPLES` is on, so what you read here is
what compiles.

## Contents

- [1. Build it](#1-build-it)
- [2. The shape of the program](#2-the-shape-of-the-program)
- [3. State and the tick](#3-state-and-the-tick)
- [4. The components](#4-the-components)
- [5. The screen](#5-the-screen)
- [6. Wiring](#6-wiring)
- [7. Where to go next](#7-where-to-go-next)

## 1. Build it

The kit needs the engine as the CMake target `retropp::engine`; the engine's `docs/guide/build-and-consume.md`
covers building it. Add the kit and turn its examples on:

```cmake
add_subdirectory(engine)
set(LAYERS_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(modules/layers)
```

```sh
cmake -S . -B build
cmake --build build --target layers-example-getting-started
```

The executable lands in the build tree under `modules/layers/`. Run it: the window shows a dark green
floor inside a lighter wall, a pale diamond in the middle, a red-to-blue badge top right and a bar top
left, yellow as far as the player is across the floor. Walk right and the yellow grows; walk left and it
shrinks. Close the window to quit.

## 2. The shape of the program

The program is in four parts, and every component-style program on the kit has the same four:

| Part | What it is | Draw types? |
|---|---|---|
| `State` and `step` | the game's state and the tick that advances it | none |
| `Floor`, `Player`, `Meter` | components: each returns the primitives it is | reads state, returns primitives |
| `Scene` | the screen: returns its layers, built by the aggregates | reads state, returns `Layers` |
| `main` | wiring: config, engine objects, art upload, the loop | — |

State changes only in the tick. The render callback calls `Scene` and hands the result to the renderer;
nothing is advanced, remembered or drawn into anywhere else.

## 3. State and the tick

```cpp
struct State {
    int playerX = 76;
    int playerY = 68;
    std::uint64_t meterGeneration = 1;  // a new number for each new meter picture
};
```

`step` runs once per tick with the held actions. It moves the player inside the walls and, when the
player's x changes, gives the meter a new generation — the meter's picture depends on x, so a new x is a
new picture:

```cpp
void step(State& state, const retropp::InputState& input) {
    const int dx = (input.isHeld(Action::Right) ? 1 : 0) - (input.isHeld(Action::Left) ? 1 : 0);
    const int dy = (input.isHeld(Action::Down) ? 1 : 0) - (input.isHeld(Action::Up) ? 1 : 0);
    const int minX = kCell;
    const int maxX = (kFloorWidth - 2) * kCell;
    const int minY = kCell;
    const int maxY = (kFloorHeight - 2) * kCell;

    const int x = std::clamp(state.playerX + dx, minX, maxX);
    if (x != state.playerX) {
        ++state.meterGeneration;
    }
    state.playerX = x;
    state.playerY = std::clamp(state.playerY + dy, minY, maxY);
}
```

The generation starts at 1, not 0: the renderer uploads a raster under generation 0 on every frame, and
under any other number only when the number is new ([raster-layer.md](raster-layer.md#generation)).

## 4. The components

Each component takes what it shows as parameters and returns the engine primitives it is.

**The floor** returns its cells — wall round the edge, floor inside:

```cpp
layers::Cells Floor(const Art& art) {
    layers::Cells cells;
    cells.reserve(static_cast<std::size_t>(kFloorWidth) * kFloorHeight);
    for (int y = 0; y < kFloorHeight; ++y) {
        for (int x = 0; x < kFloorWidth; ++x) {
            const bool wall = x == 0 || y == 0 || x == kFloorWidth - 1 || y == kFloorHeight - 1;
            cells.push_back(retropp::TileCell{.atlas = art.tiles,
                                              .tile = static_cast<std::uint16_t>(wall ? 1 : 0),
                                              .palette = art.palette});
        }
    }
    return cells;
}
```

**The player** returns its sprites — one, keyed by what it is, so the engine eases it from tick to tick:

```cpp
layers::Sprites Player(const State& state, const Art& art) {
    return {retropp::Sprite{.key = "player",
                            .x = state.playerX,
                            .y = state.playerY,
                            .atlas = art.sprites,
                            .tile = 0,
                            .palette = art.palette}};
}
```

**The meter** returns a `Raster` it paints pixel by pixel, carrying the generation from the state:

```cpp
layers::Raster Meter(const State& state) {
    layers::Raster meter{.width = kMeterWidth,
                         .height = kMeterHeight,
                         .generation = state.meterGeneration};
    meter.pixels.resize(static_cast<std::size_t>(kMeterWidth) * kMeterHeight * 4);
    const int filled = state.playerX * kMeterWidth / (kFloorWidth * kCell);
    // ... each pixel yellow left of `filled`, dark right of it ...
    return meter;
}
```

None of the three knows what layer it ends up on, at what depth, or how its content is kept.

## 5. The screen

```cpp
layers::Layers Scene(const State& state, const Art& art) {
    return {
        layers::TileLayer("floor", kFloorZ, Floor(art), kFloorWidth, kFloorHeight),
        layers::SpriteLayer("actors", kActorsZ, {Player(state, art)}),
        layers::ImageLayer("badge", kBadgeZ, art.badge, {.scroll = {-136, -8}}),
        layers::RasterLayer("meter", kMeterZ, Meter(state),
                            {.scroll = {-8, -10}, .fit = {.width = kMeterWidth, .height = 8}}),
    };
}
```

Four aggregates, one per layer:

- **`TileLayer`** puts the 20×18 floor grid at the screen's top-left (no scroll). The grid is finite:
  outside it, the layer shows nothing.
- **`SpriteLayer`** gathers the player's sprites onto the actors layer. More components would go in the
  same braces, drawn in the order listed.
- **`ImageLayer`** names the badge image uploaded at startup. A scroll of `{-136, -8}` puts its top-left
  corner at screen pixel (136, 8).
- **`RasterLayer`** shows the meter's 64×4 picture at (8, 10), fitted to 64×8 — each texel shown twice
  as tall, with no filtering.

The depths come from one table at the top of the file (`kFloorZ = 0`, `kActorsZ = 10`, `kBadgeZ = 20`,
`kMeterZ = 30`), and every layer key names what the layer shows. Every layer is the size of the active
viewport, which the kit reads from the engine on each call.

## 6. Wiring

```cpp
int main() {
    retropp::EngineConfig::setActive(retropp::EngineConfig{
        .identity = {.organization = "Retro++", .application = "Layer Kit Getting Started"},
        .window = {.title = "layers — getting started"},
        .enhancements = {.windowScale = 4},
    });

    retropp::SteadyClock clock;
    retropp::SdlPlatform platform;
    retropp::Renderer renderer{platform.device(), platform.sdlWindow()};
    retropp::RunLoop loop{clock};
    platform.actions(
        retropp::presets::directional(Action::Up, Action::Down, Action::Left, Action::Right));

    const Art art = uploadArt(renderer);
    State state;

    loop.simTick([&](const retropp::InputState& input) { step(state, input); });

    retropp::FrameDrawState frame;
    loop.renderLoop([&] {
        frame.layers = Scene(state, art);
        renderer.renderFrame(frame);
    });

    retropp::WindowedHost{loop, platform}.run();
    return 0;
}
```

- **`setActive` comes first.** It sets the engine's active viewport (Game Boy Color here, the default),
  and the kit sizes every layer from it.
- **`uploadArt`** uploads two indexed atlases (the floor tiles, and the sprite with index 0 declared a
  hole through `TransparentIndices::of({0})`), one palette and the badge image (`uploadImage`), once. The
  components only ever see the handles, in the `Art` struct.
- **The tick** is `step`. **The render callback** assigns `Scene`'s layers to the frame and submits it.
  Rebuilding the frame every frame costs no GPU traffic: the renderer uploads only what changed — cells
  by hash, the meter by generation, the image never.

## 7. Where to go next

- The model behind the four parts — [concepts.md](concepts.md).
- Choosing keys and depths for a real game — [keys-and-depth.md](keys-and-depth.md).
- Recipes: a HUD strip, a camera, clipping, fades — [how-to.md](how-to.md).
- Each aggregate in full — [sprite-layer.md](sprite-layer.md), [tile-layer.md](tile-layer.md),
  [raster-layer.md](raster-layer.md), [image-layer.md](image-layer.md), and their options in
  [layer-options.md](layer-options.md).
