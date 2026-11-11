# Zorlduh

A top-down action-adventure built on the [Polyrhythm engine](https://github.com/RetroPlusPlus/Polyrhythm),
and the start of a library of reusable components for it.

Polyrhythm gives a game low-level primitives — tiles, palettes, sprites, layers at depths — and a frame
that is described whole, as data, every tick. Zorlduh builds the layer above those primitives: the
pieces nearly every game of its kind needs, each written as a self-contained component that can leave
this repository and be used by another game on the engine. The game is where those components are
proven.

It runs at Game Boy Color resolution (160×144) on a 60 Hz tick, in a world of areas of any size, each its
own map, joined by the doors and stairs between them, with a camera that follows the player through them.

**Status: early.** The build stands up and opens the game's window, the layer kit that every on-screen
component goes through is in place and tested, the placeholder art and maps load at startup, and the
player walks a world of five areas: through the doors between them, which scroll the next area in, and
down the stairs to a cave, which fade to it.

## Progress

- [x] **Build** — the engine as a submodule, a lean Release executable, a GoogleTest suite
- [x] **Layer kit** — sprite, tile, raster and image layers that gather components onto the screen
  ([`modules/LayerKit/`](modules/LayerKit/README.md))
- [x] **Top-down movement** — walking in eight directions, collision shapes and maps, facing, walk
  animations ([`modules/TopDownMovement/`](modules/TopDownMovement/README.md))
- [x] **Camera** — a camera value, the follow law, pans
  ([`modules/CameraKit/`](modules/CameraKit/README.md))
- [x] **World** — areas with their own maps and art, openings that scroll or fade to the next area, the
  camera within an area ([`modules/WorldKit/`](modules/WorldKit/README.md))
- [x] **Assets** — generated placeholder art and the struct that loads it
- [x] **Player** — walking, collision, facing, animation
- [x] **Areas** — five areas joined by doors and a cave mouth; the player goes between them
- [ ] **Actors** — spawning, stable identity, a wandering enemy
- [ ] **Combat** — the sword, damage, invulnerability, death and restart
- [ ] **HUD and pickups** — hearts and the heart pickup
- [ ] **Sound** — a cue for every game event

## Building

Requirements are the engine's: CMake 3.28+, a C++20 compiler (GCC 13+, Clang 16+, AppleClang 15+ or
MSVC 19.38+), and the engine's build-time shader toolchain — see the engine's
[build guide](engine/docs/guide/build-and-consume.md). Python 3 is needed too: the test suite runs
`tools/placeholder_art.py --check` to confirm the committed art is what the script writes.

```sh
git clone --recurse-submodules https://github.com/RetroPlusPlus/Zorlduh.git
cd Zorlduh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/zorlduh
```

The default build type is Release.

## License

Zorlduh and every module under `modules/` are licensed under the [MIT License](LICENSE); each module
carries its own copy so it can be taken on its own. The Polyrhythm engine they depend on is licensed
separately — PolyForm Noncommercial, with a commercial license available — under its own terms in
[`engine/LICENSE`](engine/LICENSE) and [`engine/LICENSING.md`](engine/LICENSING.md). Nothing here runs
without the engine, so using the modules means using the engine under its license.

## Documentation

- [`docs/DESIGN.md`](docs/DESIGN.md) — what the project is, the screen and world model, the constraints
  every system keeps.
- [`docs/FEATURES.md`](docs/FEATURES.md) — every feature and its status.
