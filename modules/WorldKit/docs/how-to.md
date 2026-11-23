# How-to

Recipes for the questions a game asks once it has more than one area.

## Contents

- [Scroll between the screens of a side-scroller](#scroll-between-the-screens-of-a-side-scroller)
- [Make a one-way drop](#make-a-one-way-drop)
- [Shut a door](#shut-a-door)
- [Draw areas with different art](#draw-areas-with-different-art)
- [Draw a layer over the player](#draw-a-layer-over-the-player)
- [Draw things at more than one depth](#draw-things-at-more-than-one-depth)
- [Draw a layer of your own among the world's](#draw-a-layer-of-your-own-among-the-worlds)
- [Use a camera law of your own](#use-a-camera-law-of-your-own)
- [Plug in a movement module](#plug-in-a-movement-module)
- [Change the fade's look and the scroll's pace](#change-the-fades-look-and-the-scrolls-pace)
- [Lead into another layout](#lead-into-another-layout)
- [Join two maps seamlessly](#join-two-maps-seamlessly)
- [Put things in an area](#put-things-in-an-area)
- [Run the world with the scene](#run-the-world-with-the-scene)
- [Put an actor in the world](#put-an-actor-in-the-world)
- [Let an actor roam beyond its area](#let-an-actor-roam-beyond-its-area)
- [Despawn or hide an actor at the edge of its reach](#despawn-or-hide-an-actor-at-the-edge-of-its-reach)
- [Freeze actors that are off screen](#freeze-actors-that-are-off-screen)
- [Change a slot's sprites on their way to the screen](#change-a-slots-sprites-on-their-way-to-the-screen)
- [Drive the camera by hand beside the scene](#drive-the-camera-by-hand-beside-the-scene)
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

## Draw a layer of your own among the world's

Rain that falls over the actors and under a canopy, a shadow between the ground and a path, an effect
of any kind LayerKit builds: reserve its depth in the world once, under a name, and build the layer at
that name each frame.

```cpp
// In the world, once: rain over the actors and under the canopy.
.layers = {{.name = "ground", .tiles = &maps.yard, .art = groundArt, .z = kGroundZ},
           WorldKit::ReservedLayer{.name = "rain", .z = kRainZ},
           {.name = "canopy", .tiles = &maps.yardCanopy, .art = canopyArt, .z = kCanopyZ}},
.actorSlots = {{.name = "actors", .z = kActorsZ}},   // kActorsZ < kRainZ < kCanopyZ

// Each frame, after the world's layers: the rain of every area on screen.
const CameraKit::View& view = world.layouts[static_cast<std::size_t>(camera.layout())].view;
for (const WorldKit::Shown& shown : camera.shown(world)) {
    const std::string key = "rain:" + std::to_string(shown.layout) + ":" + std::to_string(shown.area);
    frame.layers.push_back(LayerKit::SpriteLayer(key, WorldKit::LayerZ(world, shown, "rain"), {Rain(state)},
                                                 {.scroll = shown.scroll, .regions = WorldKit::ViewStencil(view)}));
}
```

`Rain` is your component for the drops, placed in the area's pixels. `LayerZ(world, shown, "rain")` is
the depth the reserved layer is drawn at for that area: the declared `kRainZ` while the area is alone
on screen, and spread among the areas on screen otherwise, so the rain stays over every area's actors
and under every area's canopy ([drawing.md](drawing.md#how-the-layers-stack)).

Watch for:

- **Every area you draw it for reserves it.** `LayerZ` throws `std::out_of_range` for an area missing
  that layer; reserve it in every area, or build the layer only for the areas that have it.
- **The key names the area,** so each area's layer keeps its own scroll as the camera moves.
- **Any layer kind fits.** A `TileLayer`, `RasterLayer` or `ImageLayer` takes the same depth, scroll and
  regions.

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
the things of every area `shown` lists, each area's on a layer at the depth `ActorZ` gives for that
area's slot, taking that area's scroll:

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
    const std::int32_t z = WorldKit::ActorZ(world, area, "actors");   // the slot's depth for this area on screen
    frame.layers.push_back(LayerKit::SpriteLayer(key, z, {Things(found->second)},
                                                 {.scroll = area.scroll, .regions = WorldKit::ViewStencil(view)}));
}
```

`Things` is the game's component for a list of them. Each layer's key names its area, so a neighbor's
layer keeps its key from the frame it comes into view to the frame it leaves, and the current area's
from before a crossing to after it. During a scroll `shown` also lists the next area, so its things slide
in with it.

For things that are ActorKit actors, a [`Scene`](scene.md) builds these layers itself, one for each area
on screen and each of its slots ([Run the world with the scene](#run-the-world-with-the-scene)).

## Run the world with the scene

Hand the camera to a `WorldKit::Scene`, name the actor it follows, and make the scene's three calls:

```cpp
struct State {
    WorldKit::Scene scene;
};
State state{.scene = WorldKit::Scene{world, WorldKit::Camera{Castle, Hall}, config}};
state.scene.follow("marker");

loop.simTick([&](const retropp::InputState& in) { state.scene.tick(readInput(in), state); });
loop.renderLoop([&] {
    frame.layers = state.scene.layers(state);
    frame.regions = state.scene.fade();
    renderer.renderFrame(frame);
});
```

`world` is your validated world with its actors written in it, `config` your `ActorKit::Config` with its
`world` left out, and `readInput`, `loop`, `renderer` and `frame` your input function, run loop, renderer
and frame.

- **`config` names your bindings and handlers** for `State`, the type you hand `tick`. A game whose actors
  use the world alone writes `WorldKit::Scene{world, camera}`.
- **The scene sets the config's `world`** to the world it runs.
- **To hold everything still** under a menu or a message, leave `scene.tick` out for those ticks.
- **`tick` returns the camera's `Step`:** `step.entered` is where your game reacts to a new area.

## Put an actor in the world

Write it in the `actors` of the level it belongs to: an area's for one that stays there, a layout's for
one that roams the layout, the world's for one that goes anywhere.

```cpp
WorldKit::Area hall{.name = "hall",
                    .layers = {{.name = "ground", .tiles = &maps.hall, .art = stone}},
                    .actorSlots = {{.name = "actors", .z = 10}},
                    .collision = &maps.hall,
                    .actors = {{.actor = Guard(art), .at = {40, 64}}}};   // the guard paces the hall
```

`maps`, `stone`, `Guard` and `art` are your maps, ground art, and function returning an
`ActorKit::Actor` with what it makes it from. A layout's entry names its area with `.in`, a world's its
place: `{.actor = Hound(art), .in = WideHall, .at = {120, 80}}`,
`{.name = "marker", .actor = Marker(art), .in = {Castle, Hall}, .at = {76, 68}}`.

To spawn one while the game runs, hand the scene the same entry:

```cpp
state.scene.spawn(WorldKit::AreaActor{.actor = Bat(art), .at = {24, 24}}, {Castle, Cellar});
```

- **`at` is the actor's top-left in its area's pixels,** put exactly there, walls included.
- **A name is for an actor you need to find,** such as the one the camera follows. Names are unique across
  the whole world; `validate` rejects one used twice.
- **A script spawns too,** with `ActorKit::spawn`; its actor stays in its area until you give it a reach.

## Let an actor roam beyond its area

Set its reach. In a handler, so one of its scripts decides when:

```cpp
const ActorKit::Handlers<State> handlers{
    {GoAnywhere, [](State& state, ActorKit::Id asker, const ScriptKit::Arguments&) {
         state.scene.actor(asker).reach(WorldKit::Reach::World);   // this is now a global actor
     }},
};
```

`GoAnywhere` is a `ScriptKit::Command` of yours that the hound's touch script asks for. From its next walk
the hound goes through every opening that leads somewhere, carried into the next area by the opening's
transition ([scene.md](scene.md#where-an-actor-goes-through-an-opening)).

| Set to | It goes through |
|---|---|
| `Reach::Area` | its area's openings all hold it: it stays in its area |
| `Reach::Layout` | the openings into its own layout's areas |
| `Reach::World` | every opening that leads somewhere |

- **The script asks; the handler sets it.** A script reaches the scene through a command, as it reaches
  anything of your game's.
- **A smaller reach governs the next crossing.** An actor already beyond it stays where it is.

## Despawn or hide an actor at the edge of its reach

Set its `atEdge`. A bat that flies up the cellar's stairs and is gone:

```cpp
.actors = {{.actor = Bat(art), .at = {24, 24}, .atEdge = WorldKit::AtEdge::Despawn}},
```

`Despawn` removes it as its center steps onto the stairs, or crosses a door's gap, and retires its id. To
keep it for later, `Hide` puts it out of the world instead: a hidden actor is out of the world, not drawn,
not ticked, not solid, not touchable, until your game puts it somewhere again, its id, values and scripts
kept. Put it back when your game wants it:

```cpp
if (state.scene.actor(bat).hidden() && state.batReturns) {
    state.scene.actors().moveTo(bat, {Castle, Cellar}, {24, 24});
    state.batReturns = false;
}
```

`bat` is the bat's `ActorKit::Id` and `batReturns` a member of your state. On the next tick it is in the
cellar again, awake by its rule, its waits carrying on from where they stood.

- **`Stop`, the default, holds it at the opening.** At a spot it walks across and stays in its area.
- **The followed actor goes through every opening that leads somewhere.** While the camera follows it,
  its `atEdge` waits.
- **Spawn a despawned actor again** from its entry with `state.scene.spawn`.

## Freeze actors that are off screen

Set `awakeWhen` on the entries of a world big enough to care:

```cpp
.actors = {{.actor = Gardener(art), .in = Garden, .at = {96, 40}, .awakeWhen = WorldKit::AwakeWhen::InLayout}},
```

`Gardener` is your function and `Garden` your area number. The gardener walks while any area of its
layout is on screen and holds exactly as it is otherwise, still drawn when its area comes into view.

| `awakeWhen` | Awake while |
|---|---|
| `Always` (the default) | all the time |
| `InLayout` | any area of its layout is on screen |
| `OnScreen` | its own area is on screen |

- **On screen** is the camera's area, the areas across seams in view and the area scrolling in.
- **A frozen actor carries on from exactly where it stopped:** its waits keep the ticks they had left.
- **To hold one still by hand,** `state.scene.actor(x).freeze()`; `wake()` hands it back to its rule.

## Change a slot's sprites on their way to the screen

Hand `layers` a function. It is called with each area on screen, each of its slots and that slot's
sprites, and what it returns is drawn:

```cpp
frame.layers = state.scene.layers(state, [&](WorldKit::Place place, std::string_view slot, LayerKit::Sprites sprites) {
    if (slot == "actors" && place == garden) {
        for (retropp::Sprite& sprite : sprites) {
            sprite.alpha = 0.75f;   // the garden's actors in its shade
        }
    }
    return sprites;
});
```

`garden` is a `WorldKit::Place` of yours. Add sprites of your own by pushing them onto `sprites`; find one
actor's sprite by its key, `state.scene.actors().key(id)`.

## Drive the camera by hand beside the scene

The scene's camera is yours to move whenever you like, and the scene carries on from where you put it:

```cpp
state.scene.actors().moveTo(marker, {Castle, Hall}, {8, 56});                  // a warp within the area
state.scene.camera().place(world, state.scene.actors().box(marker));           // the camera on it at once
```

`marker` is the followed actor's `ActorKit::Id`. Every `Camera` call works on `state.scene.camera()`, and
a game that follows by hand calls `follow(world, box)` itself while `state.scene.following()` is
`ActorKit::kNoId`.

- **Into another area,** `moveTo` alone does it: on the next tick the scene puts the camera in the followed
  actor's new place, on its box.
- **While `following()` is `kNoId`,** the camera stays where you put it, and a scroll or fade your own
  `follow` starts runs at its own pace.

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
