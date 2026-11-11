# Getting started

This page builds a complete program on the module: a map twice the Game Boy Color screen in both
directions, a marker you steer with the arrow keys, WASD or a d-pad, and a camera that follows it and
stops at the map's edges. X (or the pad's B) pans the camera to the map's center over a second; Return
(or Start) pans it back to the marker.

The full source is [`examples/getting_started.cpp`](../examples/getting_started.cpp). It builds as the
`CameraKit-example-getting-started` target whenever `CAMERAKIT_BUILD_EXAMPLES` is on, so what you read
here is what compiles.

## Contents

- [1. Build it](#1-build-it)
- [2. The map and the view](#2-the-map-and-the-view)
- [3. The camera follows the marker](#3-the-camera-follows-the-marker)
- [4. A pan](#4-a-pan)
- [5. Drawing with the camera's scroll](#5-drawing-with-the-cameras-scroll)
- [6. Where to go next](#6-where-to-go-next)

## 1. Build it

The module needs the engine as the CMake target `retropp::engine`; the engine's
`docs/guide/build-and-consume.md` covers building it. Add the module and turn its examples on:

```cmake
add_subdirectory(engine)
set(CAMERAKIT_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(modules/CameraKit)
```

```sh
cmake -S . -B build
cmake --build build --target CameraKit-example-getting-started
```

The executable lands in the build tree under `modules/CameraKit/`. Run it: a floor with a grid of
pillars, a wall round the edge, and a square marker. Walk, and the screen follows the marker until the
map's edge comes into view, where it stops while the marker goes on. Press X and the screen glides to the
map's center; press Return and it glides back. Close the window to quit.

## 2. The map and the view

The map is 40×36 cells of 8 px: 320×288 px, twice the 160×144 screen each way. What the camera may show
is the map's rectangle, in world pixels:

```cpp
constexpr retropp::IntRect kMapBounds{.x = 0, .y = 0, .width = 40 * 8, .height = 36 * 8};
```

The **view** is the screen the camera shows: its size, and where it sits on a layer. This program fills
the whole screen with the world, so the view is the screen and sits at its top-left:

```cpp
constexpr CameraKit::View kView{.size = {.width = 160, .height = 144}};
```

A game with a HUD strip across the top gives its view the play area's size and an origin below the
strip ([how-to.md](how-to.md#leave-room-for-a-hud-strip)).

## 3. The camera follows the marker

```cpp
CameraKit::Camera camera;
camera.place(CameraKit::follow(kMapBounds, kView.size, markerCenter()));
```

`follow` is a **law**: given what the camera may show, the view's size and the point to look at, it says
where the camera should be. It centers the point and stops at the rectangle's edges, so nothing outside
the map comes on screen. `place` puts the camera there. The tick does the same every tick, so the camera
tracks the marker:

```cpp
loop.simTick([&](const retropp::InputState& input) {
    // ... move the marker by the directions held ...
    camera.place(CameraKit::follow(kMapBounds, kView.size, markerCenter()));
});
```

## 4. A pan

A **pan** moves the camera in a straight line over a number of ticks. It starts from wherever the camera
is and arrives on the last tick:

```cpp
if (input.justPressed(retropp::actionId(Action::PanToCenter))) {
    camera.pan(CameraKit::follow(kMapBounds, kView.size, {.x = kMapBounds.width / 2, .y = kMapBounds.height / 2}), 60);
}
```

The target is the law's answer for the map's center, so the pan ends exactly where following the center
would put the camera. While a pan runs it owns the camera: the tick calls `advance` instead of `place`,
and goes back to following once `panning()` is false:

```cpp
if (camera.panning()) {
    camera.advance();
} else {
    camera.place(CameraKit::follow(kMapBounds, kView.size, markerCenter()));
}
```

## 5. Drawing with the camera's scroll

A layer of world pixels shows the camera's position at the view's origin when its scroll is the camera's
**scroll** for that view:

```cpp
retropp::DrawLayer{.key = "map", .z = 0, .size = retropp::Renderer::defaultViewport.size(),
                   .scroll = camera.scroll(kView),
                   .content = retropp::TileContent{...}},
retropp::DrawLayer{.key = "marker", .z = 10, .size = retropp::Renderer::defaultViewport.size(),
                   .scroll = camera.scroll(kView),
                   .content = retropp::SpriteContent{.sprites = sprites}},
```

Both layers take the same scroll, so the marker, placed in world pixels, sits on the map pixel it stands
on. The program builds plain engine layers; a game that builds its screen from components hands the same
scroll to its layer aggregates.

## 6. Where to go next

- The model: the camera value, the view, laws and moves — [concepts.md](concepts.md).
- Recipes: a HUD strip, a cutscene, holding still, a law of your own — [how-to.md](how-to.md).
- Every type and member — [camera.md](camera.md).
