# CameraKit — the camera

`CameraKit` gives a game on the [Polyrhythm](https://github.com/RetroPlusPlus/Polyrhythm) engine a camera:
a value that says which world pixel the screen looks at, laws that say where it should look, and moves
that take it there, at once or over a number of ticks. A game places the camera each tick and hands its
scroll to the layers that show the world.

```cpp
CameraKit::Camera camera;

// Each tick: center the player, stopping at the map's edges.
camera.place(CameraKit::follow(mapBounds, kView.size, playerCenter));

// Each frame: the layers of world pixels take the camera's scroll.
LayerKit::TileLayer("map", 0, cells, width, height, {.scroll = camera.scroll(kView)});

// A cutscene: pan to the gate over a second, then camera.advance() each tick until !camera.panning().
camera.pan(CameraKit::follow(mapBounds, kView.size, gate), 60);
```

| Piece | What it is | Page |
|---|---|---|
| `View` | the screen the camera shows: its size, and where it sits on a layer | [docs/camera.md](docs/camera.md#view) |
| `Camera` | where the screen looks now; `place`, `pan`, `advance`, `scroll` | [docs/camera.md](docs/camera.md#camera) |
| `follow` | the law that centers a point and stops at the edges of what it may show | [docs/camera.md](docs/camera.md#follow) |

The module is header-only, depends on the engine alone (`retropp::engine`), includes no header from the
game it sits in, and needs C++20. It is meant to leave this repository as a standalone module.

## Using it

```cmake
add_subdirectory(modules/CameraKit)
target_link_libraries(my-game PRIVATE CameraKit::CameraKit)
```

The directory's `CMakeLists.txt` defines the INTERFACE target `CameraKit` (alias `CameraKit::CameraKit`)
and expects the engine as the target `retropp::engine`. `CAMERAKIT_BUILD_EXAMPLES` (on by default when
the directory is the top-level project) builds `CameraKit-example-getting-started`, the program
[docs/getting-started.md](docs/getting-started.md) walks through.

## Documentation

**Start with [docs/getting-started.md](docs/getting-started.md)**, then [docs/concepts.md](docs/concepts.md)
for the model. The full index is [docs/README.md](docs/README.md).

## Layout

```
modules/CameraKit/
  CMakeLists.txt                    the CameraKit::CameraKit target and the example option
  include/CameraKit/
    camera.h                        View, Camera
    laws.h                          follow
  examples/getting_started.cpp      the getting-started program
  docs/                             the guide
```
