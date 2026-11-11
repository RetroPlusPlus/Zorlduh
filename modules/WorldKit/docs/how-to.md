# How-to

Recipes for the questions a game asks once it has more than one area.

## Contents

- [Scroll between the screens of a side-scroller](#scroll-between-the-screens-of-a-side-scroller)
- [Make a one-way drop](#make-a-one-way-drop)
- [Shut a door](#shut-a-door)
- [Draw areas with different art](#draw-areas-with-different-art)
- [Draw a layer over the player](#draw-a-layer-over-the-player)
- [Draw things at more than one depth](#draw-things-at-more-than-one-depth)
- [Use a camera law of your own](#use-a-camera-law-of-your-own)
- [Plug in a movement module](#plug-in-a-movement-module)
- [Change the fade's look and the scroll's pace](#change-the-fades-look-and-the-scrolls-pace)
- [Lead into another layout](#lead-into-another-layout)
- [Join two maps seamlessly](#join-two-maps-seamlessly)
- [Put things in an area](#put-things-in-an-area)
- [Keep part of the screen for something else](#keep-part-of-the-screen-for-something-else)

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
answers what is solid. For screens that simply continue into each other, make the openings seamless
([below](#join-two-maps-seamlessly)).

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

Openings do not open and close. A door that is shut until a key is found is two worlds — one with the
opening leading somewhere and one without — and the game switches which it hands the camera; or the
game draws the shut door and refuses the player with its own solid query over the module's:

```cpp
const auto solids = [&](retropp::IntRect box) {
    return camera.solids(world).solidBox(box) || (doorShut && overlaps(box, doorCells));
};
```

The second keeps one world and makes the door's state the game's, like any other obstacle.

## Draw areas with different art

Each map layer carries its own `TileCatalog`, so the same map values can draw differently from area to
area, and a layer can draw from several sheets:

```cpp
WorldKit::Area{.layers = {{.name = "ground", .tiles = &maps.cave.tiles, .art = CaveArt(assets), .z = kGroundZ}},
               .collision = &maps.cave.collision, ...},
```

where `CaveArt` maps each value to a tile and palette of its own choosing. An art table is built once,
after the sheets are uploaded, as any `TileCatalog` is (the engine's `docs/guide/tilemaps.md`).

## Draw a layer over the player

A canopy the player walks under is a second map layer above the area's actor slot. Its map is the
area's size; the cells under leaves hold one value and every other cell another, drawn through a slot
that draws nothing:

```cpp
// The sheet: slot 0 is every pixel index 0, slot 1 is leaves with gaps of index 0.
// Index 0 is the sheet's transparent index, so neither slot 0 nor the gaps draw.
const retropp::AtlasManifest overhead = renderer.uploadAtlas(pixels, 16, 8, retropp::TransparentIndices::of({0}));

const retropp::TileCatalog canopyArt{.entries = {
    {.id = 0, .sheet = overhead.atlasId, .slot = 0, .palette = leaves},   // nothing
    {.id = 1, .sheet = overhead.atlasId, .slot = 1, .palette = leaves},   // leaves
}};

WorldKit::Area{
    .layers = {{.name = "ground", .tiles = &maps.glade, .art = forestArt, .z = kGroundZ},
               {.name = "canopy", .tiles = &maps.gladeCanopy, .art = canopyArt, .z = kOverheadZ}},
    .actorSlots = {{.name = "actors", .z = kActorsZ}},   // kGroundZ < kActorsZ < kOverheadZ
    .collision = &maps.gladeCollision,
    ...},
```

The player, drawn through `Actors` at the `"actors"` slot, is over the ground and under the canopy;
the ground shows through the canopy's gaps and everywhere the canopy's map draws nothing. A path under
the player's feet is the same with its depth between the ground and the slot, and the inside of a cave
mouth is a roof layer over it. The canopy is only drawn: the collision map alone says what is solid.

## Draw things at more than one depth

An area has as many actor slots as it needs. Flying things over a canopy that walking things go under:

```cpp
.actorSlots = {{.name = "actors", .z = kActorsZ},     // under the canopy
               {.name = "flyers", .z = kFlyersZ}},    // over it: kOverheadZ < kFlyersZ
```

```cpp
frame.layers.push_back(WorldKit::Actors(state.camera, world, "actors", {Player(state), Walkers(state)}));
frame.layers.push_back(WorldKit::Actors(state.camera, world, "flyers", {Birds(state)}));
```

Each slot is its own sprite layer with its own key. An area without a slot by that name makes `Actors`
throw, so either give every area the slots the game draws at or draw a slot only where the current area
has it. A thing that moves between the two — a bird landing — is drawn by one component one frame and
the other the next.

## Use a camera law of your own

A layout's `camera` takes any function of the shape `(bounds, view, focus) → position`:

```cpp
WorldKit::Layout layout{..., .camera = sideScroll};
```

The module asks it where the camera should be within every area of that layout, and takes the camera
there as the followed thing moves ([camera.md](camera.md#camera)). A law that reads the game's own
state (the direction of travel, for look-ahead) is a lambda that captures what it needs. CameraKit's
`docs/how-to.md` has a side-scrolling law to start from.

## Plug in a movement module

`camera.solids(world)` has the `solidBox(retropp::IntRect)` member a movement module's solid query is,
and `follow` takes a plain `retropp::IntRect`, so a module's walker and the camera meet through the
engine's types:

```cpp
if (!camera.transitioning()) {
    walker.walk(config, directions, camera.solids(world));
}
const WorldKit::Step step = camera.follow(world, walker.collision(config).bounds());
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
    .scrollAligns = true,                       // a scroll also lines the view up within a wide next area
    .fadeTicks = 30,                            // half a second each way
    .fadeColor = retropp::Rgba8{255, 255, 255}, // to white
};
```

Each layout has its own, so a dungeon's doors can scroll faster than the overworld's, and its fades be
another color.

## Lead into another layout

A destination names a layout when the opening leads out of its own. A dungeon's door in the
overworld, and the dungeon's way back:

```cpp
// The overworld's lake
{.name = "dungeon door", .at = {.x = 30, .y = 2, .width = 2, .height = 1}, .transition = Transition::Fade,
 .leadsTo = {.layout = Layout::DungeonOne, .area = Room::Entrance, .opening = "door"}},
// The dungeon's entrance room
{.name = "door", .at = {.x = 9, .y = 13, .width = 2, .height = 1}, .transition = Transition::Fade,
 .leadsTo = {.layout = Layout::Overworld, .area = Area::Lake, .opening = "dungeon door"}},
```

Any kind of transition crosses layouts: a scroll or a seam attaches the other layout's area beside this
one as it would an area of its own. A game learns which layout it is in from `camera.layout()` and from
`step.entered->layout`.

## Join two maps seamlessly

Two openings that are each other's far end, both seamless, in facing walls, with spans of one length:

```cpp
// The hall, 20 cells wide: its whole north wall but the corners
{.name = "north wall", .side = Side::North, .span = {1, 18}, .transition = Transition::Seamless,
 .leadsTo = {.area = Room::Treasury, .opening = "south wall"}},
// The treasury, 30 cells wide: 18 cells of its south wall, centered
{.name = "south wall", .side = Side::South, .span = {6, 23}, .transition = Transition::Seamless,
 .leadsTo = {.area = Room::Hall, .opening = "north wall"}},
```

The two maps meet along the spans: the treasury is drawn above the hall whenever it is in view, what is
solid continues from one map into the other, and crossing the wall only changes which area is current.
Where the openings line the maps up is where they meet; to shift one from there, give both destinations
an `offset`, each the opposite of the other:

```cpp
{.name = "east edge", .side = Side::East, .span = {0, 15}, .transition = Transition::Seamless,
 .leadsTo = {.area = Area::Next, .opening = "west edge", .offset = {.x = 0, .y = 2}}},   // the next map two cells lower
{.name = "west edge", .side = Side::West, .span = {0, 15}, .transition = Transition::Seamless,
 .leadsTo = {.area = Area::This, .opening = "east edge", .offset = {.x = 0, .y = -2}}},
```

The doorway cells on each map are what the spans say, so a seam along a whole edge means a whole edge of
doorway cells.

Areas joined in a chain are drawn through it: from a big room, a small room hanging off the side of the
room beside it is in view where it hangs. To draw fewer, set the layout's `seamDepth` — 1 draws only the
areas joined to the current one, 2 those and the areas joined to them:

```cpp
WorldKit::Layout layout{..., .seamDepth = 1};
```

## Put things in an area

The module draws the areas and the sprite layers a game hands it for its actor slots. A game's things —
the player, an area's inhabitants, a chest, a sign — live in the game's state, each with a position in
its own area's pixels, and are drawn by the game's components, through `Actors` for the current area.

Which things are where is the game's to keep: a table keyed by the place (`WorldKit::Place`, or the
layout and area). `step.entered` says when the place changes, so that is where an area's things come into
being, wake or are put away; `camera.layout()` and `camera.area()` say where the screen is at any
moment. What the things make solid or passable is the game's own solid query wrapping
`camera.solids(world)` ([Shut a door](#shut-a-door)).

Across a seam the next area is on screen before the thing followed crosses, so its things are too. Draw
the things of every area `shown` lists, each area's on a layer at that area's slot depth that takes that
area's scroll, not the current area's alone:

```cpp
// The game's things, by area; each position is in that area's pixels.
std::map<std::pair<int, int>, std::vector<Thing>> things;

// Each frame, after the world's layers: one sprite layer per area on screen that has things.
const CameraKit::View& view = world.layouts[static_cast<std::size_t>(camera.layout())].view;
for (const WorldKit::Shown& area : camera.shown(world)) {
    const auto found = things.find({area.layout, area.area});
    if (found == things.end()) {
        continue;
    }
    const std::string key = "things:" + std::to_string(area.layout) + ":" + std::to_string(area.area);
    const std::int32_t z = WorldKit::ActorZ(world, {.layout = area.layout, .area = area.area}, "actors");
    frame.layers.push_back(LayerKit::SpriteLayer(key, z, {Things(found->second)},
                                                 {.scroll = area.scroll, .regions = WorldKit::ViewStencil(view)}));
}
```

`Things` is the game's component for a list of them. Each layer's key names its area, so a neighbor's
layer keeps its key from the frame it comes into view to the frame it leaves, and the current area's
from before a crossing to after it. During a scroll `shown` also lists the next area, so its things slide
in with it.

## Keep part of the screen for something else

The layout's `view` is the part of the screen the world is drawn in, and where it sits. A 16 px strip
across the top for a HUD:

```cpp
constexpr CameraKit::View kView{.size = {.width = 160, .height = 128}, .origin = {.x = 0, .y = 16}};
WorldKit::Layout layout{..., .view = kView};
```

`WorldLayers` confines every map layer to it and `Actors` every sprite layer it builds, so whatever an
area reaches past the strip, the strip shows the layers below and whatever the game draws there. A
border around the screen, or a panel down one side, is the same: a smaller view, placed where the world
goes. Any other layer of the game's own in the world's pixels can take the same confinement with
`ViewStencil(kView)` in its `regions`.
