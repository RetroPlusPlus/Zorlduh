# How-to

Recipes for the questions a game asks once it has a camera.

## Contents

- [Leave room for a HUD strip](#leave-room-for-a-hud-strip)
- [Pan for a cutscene](#pan-for-a-cutscene)
- [Hold the camera still](#hold-the-camera-still)
- [Write a law of your own](#write-a-law-of-your-own)
- [Follow something other than the player](#follow-something-other-than-the-player)

## Leave room for a HUD strip

Give the view the play area's size and an origin below the strip, and let every layer of world pixels
take the camera's scroll for that view. The HUD's own layer takes no scroll:

```cpp
constexpr CameraKit::View kView{.size = {.width = 160, .height = 128}, .origin = {.x = 0, .y = 16}};

frame.layers = {
    LayerKit::TileLayer("map", kWorldZ, cells, width, height, {.scroll = camera.scroll(kView)}),
    LayerKit::SpriteLayer("actors", kActorsZ, {Player(state)}, {.scroll = camera.scroll(kView)}),
    LayerKit::SpriteLayer("hud", kHudZ, {Hearts(state)}),
};
```

The world draws under the strip too; the HUD layer, above it, covers that part wherever it is opaque.

## Pan for a cutscene

Pan to where the law would put the camera for the thing to show, so the pan ends exactly where following
it would; take the pan's ticks until it arrives, then go back to the law:

```cpp
// When the cutscene starts:
camera.pan(CameraKit::follow(mapBounds, kView.size, gate), 60);

// Each tick:
if (camera.panning()) {
    camera.advance();
} else {
    camera.place(CameraKit::follow(mapBounds, kView.size, playerCenter));
}
```

A pan back is the same call with the player as the focus. To cut a pan short, `place` the camera.

## Hold the camera still

Stop calling the law. A room exactly the screen's size holds still on its own, because `follow` has
nowhere to go; for anything else, place the camera once and leave it:

```cpp
camera.place(CameraKit::follow(roomBounds, kView.size, roomCenter));   // once, on entering the room
```

## Write a law of your own

A law is any function of the shape `(bounds, view, focus) → position`. A side-scroller's camera that
follows horizontally, keeps the player in a vertical band and never shows above the ceiling:

```cpp
retropp::Vec2i sideScroll(retropp::IntRect bounds, retropp::PixelSize view, retropp::Vec2i focus) {
    const retropp::Vec2i centered = CameraKit::follow(bounds, view, focus);
    // Keep the camera's y where it is until the focus leaves the middle band of the view.
    const int top = cameraY + view.height / 3;
    const int bottom = cameraY + 2 * view.height / 3;
    const int y = focus.y < top ? focus.y - view.height / 3
                : focus.y > bottom ? focus.y - 2 * view.height / 3
                : cameraY;
    return {.x = centered.x, .y = std::clamp(y, bounds.y, bounds.y + bounds.height - view.height)};
}
```

`cameraY` is the camera's current position, read from wherever the game keeps it; a law that depends on
the camera's history is a plain function that reads it. Use it as `follow` is used: place the camera
where it says, or hand it to whatever drives the camera for the game.

## Follow something other than the player

The focus is any point. A camera that looks between the player and a boss:

```cpp
const retropp::Vec2i between{.x = (player.x + boss.x) / 2, .y = (player.y + boss.y) / 2};
camera.place(CameraKit::follow(arenaBounds, kView.size, between));
```
