# Zorlduh

A top-down action-adventure built on the [Polyrhythm engine](https://github.com/RetroPlusPlus/Polyrhythm),
and the start of a library of reusable components for it.

Polyrhythm gives a game low-level primitives — tiles, palettes, sprites, layers at depths — and a frame
that is described whole, as data, every tick. Zorlduh builds the layer above those primitives: the
pieces nearly every game of its kind needs, each written as a self-contained component that can leave
this repository and be used by another game on the engine. The game is where those components are
proven.

It runs at Game Boy Color resolution (160×144) on a 60 Hz tick, with rooms of any size and a camera that
follows the player through them.

**Status: early.** The build stands up and opens the game's window, and the layer kit that every
on-screen component goes through is in place and tested. The window is still empty.

## Progress

- [x] **Build** — the engine as a submodule, a lean Release executable, a GoogleTest suite
- [x] **Layer kit** — sprite, tile, raster and image layers that gather components onto the screen
  ([`modules/layers/`](modules/layers/README.md))
- [ ] **Assets** — generated placeholder art and the struct that loads it
- [ ] **World** — a map of free-form rooms with collision, on screen
- [ ] **Player** — walking, collision, facing, animation
- [ ] **Rooms and camera** — following the player within a room and scrolling to the next
- [ ] **Actors** — spawning, stable identity, a wandering enemy
- [ ] **Combat** — the sword, damage, invulnerability, death and restart
- [ ] **HUD and pickups** — hearts and the heart pickup
- [ ] **Sound** — a cue for every game event

## Building

Requirements are the engine's: CMake 3.28+, a C++20 compiler (GCC 13+, Clang 16+, AppleClang 15+ or
MSVC 19.38+), and the engine's build-time shader toolchain — see the engine's
[build guide](engine/docs/guide/build-and-consume.md).

```sh
git clone --recurse-submodules <repo-url>
cd <repo>
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/zorlduh
```

The default build type is Release.

## Documentation

- [`docs/DESIGN.md`](docs/DESIGN.md) — what the project is, the screen and room model, the constraints
  every system keeps.
- [`docs/FEATURES.md`](docs/FEATURES.md) — every feature and its status.
