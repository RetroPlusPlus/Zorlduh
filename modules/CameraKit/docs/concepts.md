# Concepts

The model behind the module: a value that says where the camera is, a view that says what it shows, laws
that say where it should be, and moves that take it there.

## Contents

- [The camera is a value](#the-camera-is-a-value)
- [The view and its scroll](#the-view-and-its-scroll)
- [Laws](#laws)
- [Moves](#moves)
- [Where each part lives in a game](#where-each-part-lives-in-a-game)

## The camera is a value

A `Camera` is where the screen looks now: the world pixel shown at the view's top-left, plus any pan in
progress. It is a plain value with no pointers in it, so a game state holding one copies and saves like
any other value. It decides nothing: where it should be is a law's answer, and the game puts it there.

## The view and its scroll

A `View` is the screen the camera shows: its size in pixels, and its origin, where it sits on a layer. A
game that fills the whole screen with its world has a view the size of the screen at (0, 0). A game with
a 16 px HUD strip across the top of a 160×144 screen has a view of 160×128 at (0, 16): the camera looks
through the play area, and the strip above it is something else's.

A layer of world pixels shows the camera's position at the view's origin when its scroll is
`camera.scroll(view)`: the position less the origin. Screen pixel (x, y) then shows world pixel
(x + scroll.x, y + scroll.y), so the layer's content lines up with the camera wherever the view sits.
Every layer of world pixels takes the same scroll, which is what keeps the world and the things in it
together.

## Laws

A law is a function from what the camera may show, the view's size and the point it is looking at, to
where it should be:

```cpp
retropp::Vec2i law(retropp::IntRect bounds, retropp::PixelSize view, retropp::Vec2i focus);
```

`follow` is the module's law. On each axis on its own: if the bounds are at least as long as the view,
the view is centered on the focus and stopped at the bounds' edges, so nothing outside them comes on
screen; if the bounds are shorter than the view, they cannot fill it, so the view is centered on them
and holds still. A map larger than the screen is followed across; a room smaller than the screen sits in
the middle with nothing drawn around it; a room exactly the screen's size does not move.

A game calls the law and places the camera where it says, usually once per tick with the player as the
focus. Because the law is a plain function, a game writes its own for a different feel — a band the
player moves in before the camera follows, a look-ahead in the direction of travel, a floor the camera
locks to — and uses it the same way. Anything that drives the camera for the game takes a law of this
shape, so the game's own law plugs in there too.

## Moves

The camera gets where the law says in one of two ways:

- **`place`** puts it there now. Following is placing every tick.
- **`pan`** moves it there in a straight line over a number of ticks, arriving on the last; `advance`
  takes each tick. A pan is resolved through the engine's tween (`Tween<Vec2>`, linear easing) and
  rounded to the nearest pixel at each tick, so it is the same on every run.

A pan owns the camera while it runs: a game that keeps calling the law would place the camera out from
under it, so it calls `advance` instead and goes back to the law once `panning()` is false. `place`
during a pan drops the pan. Nothing else interrupts a pan; a game that wants to cut it short places the
camera.

## Where each part lives in a game

| Part | Built | Lives in |
|---|---|---|
| `View` | once | a constant beside the screen layout |
| `Camera` | at the start | the game state |
| the law, `place` or `advance` | every tick | the tick |
| `scroll(view)` | every frame | the layers that show world pixels |

`camera.h` builds one draw type, `retropp::LayerScroll`, as the return of `scroll`; a simulation that
places the camera and never reads its scroll takes nothing to draw from the header.
