# How-to

Recipes for the questions a game asks once it has more than one area.

## Contents

- [Scroll between the screens of a side-scroller](#scroll-between-the-screens-of-a-side-scroller)
- [Make a one-way drop](#make-a-one-way-drop)
- [Shut a door](#shut-a-door)
- [Draw areas with different art](#draw-areas-with-different-art)
- [Use a camera law of your own](#use-a-camera-law-of-your-own)
- [Plug in a movement module](#plug-in-a-movement-module)
- [Change the fade's look and the scroll's pace](#change-the-fades-look-and-the-scrolls-pace)

## Scroll between the screens of a side-scroller

An edge opening may span a whole wall. Two side-scrolling screens whose entire east and west edges join:

```cpp
// Screen 1, 20 cells tall
{.name = "east", .side = Side::East, .span = {0, 19}, .transition = Transition::Scroll,
 .leadsTo = {.area = Area::Screen2, .opening = "west"}},
// Screen 2
{.name = "west", .side = Side::West, .span = {0, 19}, .transition = Transition::Scroll,
 .leadsTo = {.area = Area::Screen1, .opening = "east"}},
```

Walking off the edge anywhere along it scrolls the next screen in, level with this one. What stops the
player going up is the movement's own business (gravity is the movement module's); the module only
answers what is solid.

## Make a one-way drop

An opening leads one way. A hole whose bottom leads back nowhere:

```cpp
// The upper floor
{.name = "hole", .at = {.x = 7, .y = 3, .width = 1, .height = 1}, .transition = Transition::Fade,
 .leadsTo = {.area = Area::LowerFloor, .opening = "landing"}},
// The lower floor: an entrance only
{.name = "landing", .at = {.x = 7, .y = 3, .width = 1, .height = 1}},
```

A wall opening that leads nowhere is solid past its gap, so a door that can only be entered from the
other side is an edge opening with no `leadsTo`.

## Shut a door

Openings do not open and close. A door that is shut until a key is found is two layouts — one with the
opening leading somewhere and one without — and the game switches which it hands the camera; or the
game draws the shut door and refuses the player with its own solid query over the module's:

```cpp
const auto solids = [&](retropp::IntRect box) {
    return camera.solids(layout).solidBox(box) || (doorShut && overlaps(box, doorCells));
};
```

The second keeps one layout and makes the door's state the game's, like any other obstacle.

## Draw areas with different art

Each area carries its own `TileCatalog`, so the same map values can draw differently from area to area,
and an area can draw from several sheets:

```cpp
WorldKit::Area{.tiles = &maps.cave, .art = CaveArt(assets), .collision = &maps.cave, ...},
```

where `CaveArt` maps each value to a tile and palette of its own choosing. An art table is built once,
after the sheets are uploaded, as any `TileCatalog` is (the engine's `docs/guide/tilemaps.md`).

## Use a camera law of your own

The layout's `camera` takes any function of the shape `(bounds, view, focus) → position`:

```cpp
WorldKit::Layout layout{..., .camera = sideScroll};
```

The module places the camera by it within every area and pans by it between areas. A law that reads
the game's own state (the direction of travel, for look-ahead) is a lambda that captures what it needs.
CameraKit's `docs/how-to.md` has a side-scrolling law to start from.

## Plug in a movement module

`camera.solids(layout)` has the `solidBox(retropp::IntRect)` member a movement module's solid query is,
and `follow` takes a plain `retropp::IntRect`, so a module's walker and the camera meet through the
engine's types:

```cpp
if (!camera.transitioning()) {
    walker.walk(config, directions, camera.solids(layout));
}
const WorldKit::Step step = camera.follow(layout, walker.collision(config).bounds());
if (step.move != retropp::Vec2i{}) {
    walker.place(walker.position() + step.move);
}
```

The walker's own position is moved only when the camera asks, so its stride and facing are its own
otherwise.

## Change the fade's look and the scroll's pace

They are fields of the layout:

```cpp
WorldKit::Layout layout{
    ...,
    .scrollTicks = 48,                          // a slower scroll
    .carry = 24,                                // carried further in
    .fadeTicks = 30,                            // half a second each way
    .fadeColor = retropp::Rgba8{255, 255, 255}, // to white
};
```
