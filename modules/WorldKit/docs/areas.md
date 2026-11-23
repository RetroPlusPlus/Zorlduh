# Areas reference

What a world is made of, and the check that it holds together. The types are in
`include/WorldKit/area.h`.

```cpp
#include "WorldKit/area.h"   // Side, Transition, Span, Destination, Opening, CellValue, MapLayer, ReservedLayer,
                             // ActorSlot, Place, Reach, AtEdge, AwakeWhen, AnyActor, AreaActor, LayoutActor,
                             // WorldActor, Area, CameraLaw, Layout, World, validate, ActorZ, LayerZ, placeOf
```

## Contents

- [`Side`, `Transition`, `Span`](#side-transition-span)
- [`Destination`](#destination)
- [`Opening`](#opening)
- [`MapLayer`](#maplayer)
- [`ReservedLayer`](#reservedlayer)
- [`ActorSlot`](#actorslot)
- [`Reach`](#reach)
- [`AtEdge`](#atedge)
- [`AwakeWhen`](#awakewhen)
- [`AnyActor`](#anyactor)
- [`AreaActor`, `LayoutActor`, `WorldActor`](#areaactor-layoutactor-worldactor)
- [`Area`](#area)
- [`CameraLaw`](#cameralaw)
- [`Layout`](#layout)
- [`World`, `Place`](#world-place)
- [`validate`](#validate)
- [`ActorZ`](#actorz)
- [`LayerZ`](#layerz)
- [`placeOf`](#placeof)

## `Side`, `Transition`, `Span`

```cpp
enum class Side : std::uint8_t { None, North, South, East, West };
enum class Transition : std::uint8_t { Scroll, Fade, Seamless };
struct Span { int first = 0; int last = 0; };   // cells along a side, both ends included
```

A side is the wall an edge opening is in; `None` is the value of an opening that is not in a wall. A span
runs down the east and west walls (rows) and across the north and south walls (columns), in cells,
counted from the map's top-left. A transition is how an opening is gone through: the next area slides
in; the screen fades; or the two areas are joined for good and going through only changes which is
current.

## `Destination`

```cpp
inline constexpr int kNoArea = -1;
inline constexpr int kSameLayout = -1;

struct Destination {
    int layout = kSameLayout;   // the layout's index in the world; the opening's own by default
    int area = kNoArea;         // the area's index in that layout
    std::string opening;        // the name of an opening in it
    retropp::Vec2i offset{};    // in cells, right and down: the next area's shift from where the openings line it up
};
```

Where an opening leads. A game names its layouts and areas with enums whose values are their indices, so
`.area = Area::Lake` and `.leadsTo = {.layout = Layout::DungeonOne, .area = Room::Entrance, .opening =
"door"}` are the usual forms. `kNoArea` means nowhere: the opening is only an entrance. `offset` applies
to a scroll and a seam, which attach the next area; a fade has nothing to shift, and an offset on one
is rejected.

## `Opening`

```cpp
struct Opening {
    std::string name;
    Side side = Side::None;             // an edge opening: the wall it is in ...
    Span span{};                        // ... and the cells along it
    retropp::IntRect at{};              // a spot opening: cells inside the area (width and height > 0)
    Transition transition = Transition::Scroll;
    Destination leadsTo{};
};
```

Exactly one of `side` and `at` is set. The name is how another area's opening names this one as its
destination, and how a check's message names it; it is unique within its area.

```cpp
{.name = "east door",  .side = Side::East, .span = {6, 9}, .transition = Transition::Scroll,
 .leadsTo = {.area = Area::Lake, .opening = "west door"}},
{.name = "cave mouth", .at = {.x = 12, .y = 4, .width = 2, .height = 1}, .transition = Transition::Fade,
 .leadsTo = {.area = Area::Cave, .opening = "cave exit"}},
{.name = "north wall", .side = Side::North, .span = {1, 18}, .transition = Transition::Seamless,
 .leadsTo = {.area = Room::Treasury, .opening = "south wall"}},
{.name = "pit bottom", .at = {.x = 3, .y = 3, .width = 1, .height = 1}},   // an entrance only
```

A scroll opening is in a wall and leads to an opening in the facing wall of the next area whose span is
the same length. A seam is the same, and its far end is a seam that leads back to it with the opposite
offset. A fade opening is either kind and leads to either kind.

## `MapLayer`

```cpp
struct MapLayer {
    std::string name;                            // part of the layer's key; unique within its area
    const retropp::IndexGrid* tiles = nullptr;   // one map value per cell, the collision map's size
    retropp::TileCatalog art{};                  // which tile and palette each map value draws as
    std::int32_t z = 0;                          // the layer's depth on the engine's z axis: lower is below
};
```

One tile map of an area, drawn on a layer of its own at the depth the game declares. The ground is a
layer like any other; a path under the player's feet, a canopy over the player's head and the inside of
a cave mouth are more of them. `art` is an engine `TileCatalog`, so two areas drawn from the same map
values can look different, and one layer can draw from several sheets. A layer whose `tiles` is null is
a [reserved layer](#reservedlayer): a depth for a layer the game builds itself.

Every layer's map is the size of the area. A layer that covers only part of the area draws nothing in
its other cells: their catalog entries name a slot whose every pixel is in the sheet's
transparent-index set (`retropp::TransparentIndices`, given when the sheet is uploaded), so what is
below shows through. The same goes for gaps inside a tile, such as the space between a canopy's
leaves.

```cpp
{.name = "ground", .tiles = &maps.glade.tiles, .art = ForestArt(assets), .z = depths.ground},
{.name = "canopy", .tiles = &maps.gladeCanopy, .art = OverlayArt(assets), .z = depths.overhead},
```

## `ReservedLayer`

```cpp
struct ReservedLayer {
    std::string name;
    std::int32_t z = 0;

    operator MapLayer() const;   // this name and depth, `tiles` null
};
```

A depth an area holds, under a name, for a layer your game builds itself: rain over the actors, a
shadow under them, an effect between two map layers. Write it among the area's `layers`; it becomes a
`MapLayer` whose `tiles` is null, and `WorldLayers` skips it. Each frame, your game builds a layer of any kind
LayerKit builds, for each area on screen that has it, at the depth
[`LayerZ`](camera.md#actorz-layerz-by-shown) gives for that area.

```cpp
.layers = {{.name = "ground", .tiles = &maps.yard, .art = groundArt, .z = kGroundZ},
           WorldKit::ReservedLayer{.name = "rain", .z = kRainZ},              // over the actors, under the canopy
           {.name = "canopy", .tiles = &maps.yardCanopy, .art = canopyArt, .z = kCanopyZ}},
.actorSlots = {{.name = "actors", .z = kActorsZ}},                           // kActorsZ < kRainZ < kCanopyZ
```

[how-to.md](how-to.md#draw-a-layer-of-your-own-among-the-worlds) has the frame side whole.

| A reserved layer | What it does |
|---|---|
| its `name` | is a layer name of its area: unique among the area's layers and slots, and what `LayerZ` finds it by |
| its `z` | orders it among the area's layers and slots, and is checked by `validate` as theirs are |
| in `WorldLayers` | is skipped: your game builds the layer |
| in `TopZ` | counts like any layer |
| its `art` | stays empty: `validate` rejects a layer with art and a null `tiles` as a map layer missing its map |

Watch for:

- **Build the layer for every area on screen that reserves it,** with that area's `Shown`, as you build
  an actor layer. A layer built only for the current area is missing from a neighbor across a seam.
- **Give each area's layer a key of its own,** such as the area's numbers in it, so each keeps its scroll.

## `ActorSlot`

```cpp
struct ActorSlot {
    std::string name;   // what a game asks for it by; unique among the area's layers and slots
    std::int32_t z = 0; // its depth on the same axis as the map layers
};
```

A named depth in an area for the things that move in it: the player, the people and creatures in it.
A slot holds nothing and WorldKit draws nothing at it; a game draws its sprites there through
[`Actors`](drawing.md#actors), or on a layer of its own at the depth
[`ActorZ`](camera.md#actorz-layerz-by-shown) gives for an area on screen, and they land among the
area's map layers — over every map layer below the slot's depth and under every one above it. An area
has as many slots as it needs, or none: an `"actors"` slot over the ground and under a canopy, a
`"flyers"` slot above the canopy, a `"swimmers"` slot under a water surface.

## `Reach`

```cpp
enum class Reach : std::uint8_t { Area, Layout, World };
```

How far an actor of a [scene](scene.md) may roam. An actor written in an area's `actors` has
`Reach::Area`, in a layout's `Reach::Layout`, and in the world's `Reach::World`; `SceneActor::reach`
changes it while the game runs ([scene.md](scene.md#reach)).

| `Reach` | Inside it | Its edge |
|---|---|---|
| `Area` | its own area | every opening out of it |
| `Layout` | every area of its layout | every opening into another layout |
| `World` | every area of the world | — |

An actor goes through an opening inside its reach by the opening's transition
([scene.md](scene.md#where-an-actor-goes-through-an-opening)). An opening outside it is the edge of its
reach, where its [`AtEdge`](#atedge) applies.

## `AtEdge`

```cpp
enum class AtEdge : std::uint8_t { Stop, Despawn, Hide };
```

What an actor does at an opening that leads outside its reach. Written on its entry, `.atEdge`, and
changed with `SceneActor::atEdge`.

| `AtEdge` | At a gap in a wall | At a spot |
|---|---|---|
| `Stop` (the default) | It is held there. | It walks across and stays in its area. |
| `Despawn` | It goes out and is removed: its id is retired. | It is removed as its center steps onto the spot. |
| `Hide` | It goes out and is hidden. | It is hidden as its center steps onto the spot. |

A hidden actor is out of the world: not drawn, not ticked, not solid, not touchable, until the game puts
it somewhere again with `actors().moveTo`. It keeps its id, its values and where its scripts are
([scene.md](scene.md#the-edge-of-a-reach)).

```cpp
.actors = {{.actor = Bat(art), .at = {24, 24}, .atEdge = WorldKit::AtEdge::Despawn}},   // gone up the stairs
```

## `AwakeWhen`

```cpp
enum class AwakeWhen : std::uint8_t { Always, InLayout, OnScreen };
```

When an actor of a scene is awake, taking part in each tick. The rest of the time it is frozen: it holds
exactly as it is, still drawn and still solid, and carries on from there when it wakes.

| `AwakeWhen` | The actor is awake |
|---|---|
| `Always` (the default) | all the time |
| `InLayout` | while any area of the layout it is in is on screen |
| `OnScreen` | while the area it is in is on screen |

On screen is what the camera shows: its own area, the areas across seams in view, and the area scrolling
in ([scene.md](scene.md#awake-or-frozen)).

```cpp
.actors = {{.actor = Gardener(art), .in = GroundsArea::Garden, .at = {96, 40}, .awakeWhen = WorldKit::AwakeWhen::InLayout}},
```

## `AnyActor`

```cpp
class AnyActor {
public:
    AnyActor() = default;
    template <class T> AnyActor(T actor);         // holds a copy of `actor`
    explicit operator bool() const noexcept;      // whether it holds a value
    template <class T> const T* as() const noexcept;
};
```

An actor value held for a scene, so `area.h` includes the engine's and WorldKit's headers alone. You write it as
`.actor = Guard(art)`, with any function or value giving an `ActorKit::Actor`; the scene reads it back as
that `ActorKit::Actor`.

| Member | Returns |
|---|---|
| `AnyActor()` | an empty one |
| `AnyActor(actor)` | one that holds a copy of `actor` |
| `operator bool()` | whether it holds a value |
| `as<T>()` | the value when it is a `T`, `nullptr` otherwise |

- **Copies share** the one value they hold.
- **`validate` rejects an entry whose `actor` is empty.** The scene rejects one that holds another type
  than `ActorKit::Actor`, with `std::logic_error`.

## `AreaActor`, `LayoutActor`, `WorldActor`

```cpp
struct AreaActor {
    std::string name;
    AnyActor actor;
    retropp::Vec2i at{};
    AtEdge atEdge = AtEdge::Stop;
    AwakeWhen awakeWhen = AwakeWhen::Always;
};

struct LayoutActor {
    std::string name;
    AnyActor actor;
    int in = 0;
    retropp::Vec2i at{};
    AtEdge atEdge = AtEdge::Stop;
    AwakeWhen awakeWhen = AwakeWhen::Always;
};

struct WorldActor {
    std::string name;
    AnyActor actor;
    Place in{};
    retropp::Vec2i at{};
    AtEdge atEdge = AtEdge::Stop;
    AwakeWhen awakeWhen = AwakeWhen::Always;
};
```

An actor that starts in the world: an entry of an area's, a layout's or the world's `actors`. Where you
write it is how far it may roam, its [`Reach`](#reach):

```cpp
const WorldKit::AreaActor guard{.actor = Guard(art), .at = {40, 64}};                              // in an area's actors: stays there
const WorldKit::LayoutActor hound{.actor = Hound(art), .in = CastleArea::WideHall, .at = {120, 80}};   // in a layout's: roams it
const WorldKit::WorldActor marker{.name = "marker", .actor = Marker(art), .in = {Castle, Hall}, .at = {76, 68}};   // in the world's: anywhere
```

`Guard`, `Hound`, `Marker` and `art` are your functions returning an `ActorKit::Actor` and what they make
it from; `Castle`, `Hall` and `CastleArea::WideHall` are your layout and area numbers.
[scene.md](scene.md#overview) shows the three written in a world.

| Field | Meaning | Default |
|---|---|---|
| `name` | what `Scene::actor` and `Scene::follow` find it by; unique across the whole world | empty: found by its id |
| `actor` | the `ActorKit::Actor` it is | empty, which `validate` rejects |
| `in` | `LayoutActor`: an area index of its layout; `WorldActor`: a place of the world | area 0; layout 0 area 0 |
| `at` | its top-left in that area's pixels, put exactly there, walls included | (0, 0) |
| `atEdge` | what it does at the edge of its reach ([`AtEdge`](#atedge)) | `Stop` |
| `awakeWhen` | when it is awake ([`AwakeWhen`](#awakewhen)) | `Always` |

The scene spawns every entry when it is built, in the order the world is written: for each layout, each
area's entries and then the layout's; then the world's ([scene.md](scene.md#constructors)). The world
says who is there at the start; `Scene::spawn` takes the same entries while the game runs.

A world with a system of its own for what lives in it leaves every `actors` empty.

## `Area`

```cpp
struct Area {
    std::string name;                                // unique within its layout
    std::vector<MapLayer> layers;                    // at least one, map layers and reserved layers
    std::vector<ActorSlot> actorSlots;               // any number
    const retropp::IndexGrid* collision = nullptr;   // the area's size; one value is open
    std::vector<Opening> openings;
    std::vector<AreaActor> actors;                   // who starts here and stays here
};
```

The name is how [`placeOf`](#placeof) finds the area and how every message names it. The layers and
slots are in any order; each one's `z` places it. The maps are the game's and must outlive the
world. Every depth is the game's: it hands them in when it builds the world, from the one table it
keeps its depths in. An area alone on screen is drawn at exactly those depths; with more areas on
screen, WorldKit spreads them so every lower depth stays under every higher one
([drawing.md](drawing.md#how-the-layers-stack)). Collision, the openings and the camera read the
collision map alone; the layers are only drawn. Its `actors` start in it and stay in it
([`AreaActor`](#areaactor-layoutactor-worldactor)).

## `CameraLaw`

```cpp
using CameraLaw = std::function<retropp::Vec2i(retropp::IntRect bounds, retropp::PixelSize view, retropp::Vec2i focus)>;
```

Where the camera should be within an area: given the rectangle it may show — the area's own, in its own
pixels, from (0, 0), reaching across each seam into the area beyond — the view's size and the point the
camera looks at. `CameraKit::follow` is one; any function of this shape fits, including one that reads
the game's own state. How the camera gets to the law's place is the module's: see
[camera.md](camera.md#camera).

## `Layout`

```cpp
struct Layout {
    std::string name;                        // unique within the world
    std::vector<Area> areas;                 // indices are area ids
    std::vector<LayoutActor> actors;         // who starts in its areas and roams it
    int cellSize = 8;                        // the side of one map cell, in pixels
    CellValue open{};                        // the one collision-map value that is not solid
    CameraKit::View view{};                  // the screen the camera shows, and where it sits on a layer
    CameraLaw camera = CameraKit::follow;    // where the camera is within an area
    std::uint32_t scrollTicks = 32;          // a scroll's length
    int carry = 16;                          // how far a scroll carries the thing through, in pixels
    bool scrollAligns = false;               // whether a scroll also moves the camera across the exit's axis
    std::uint32_t fadeTicks = 16;            // a fade's length each way
    retropp::Rgba8 fadeColor{};              // what the screen fades to
    std::uint32_t seamDepth = 0;             // how many seams away an area in view is drawn; 0 for any number
};
```

A set of areas that share these settings. `open` is a `CellValue`, which takes the collision map's own
enumerator (`Cell::Open`) or a number. A scroll slides the camera along the exit's axis; with
`scrollAligns` it also moves across that axis, as it slides, the least it must for the view to lie
within the next area when that area is at least as big as the view there — a smaller area lands where
the opening lines it up either way. The scroll settings are those of the layout being left; a fade
darkens in the layout being left and lightens in the one entered. `seamDepth` is how far through seams
the camera looks for areas to show: 1 for the areas joined to the current one, 2 for those and the areas
joined to them, and so on, and 0 for every area a chain of seams reaches; the camera's law and the solid
query reach one seam whatever it is. Its `actors` start in one of its areas and roam it
([`LayoutActor`](#areaactor-layoutactor-worldactor)). A layout is a plain value: it copies,
and the camera reads it on every call rather than keeping a pointer to it.

## `World`, `Place`

```cpp
struct World {
    std::vector<Layout> layouts;      // indices are layout ids
    std::vector<WorldActor> actors;   // who starts in it and goes anywhere
};

struct Place {
    int layout = 0;
    int area = 0;
};
```

The world is its layouts, and its `actors` start in a place of it and go anywhere
([`WorldActor`](#areaactor-layoutactor-worldactor)); every camera call takes it. A place is a layout
and an area in it, the pair
the module hands back when a game needs both (the place entered, the areas shown). A game names a
place by its enums, or by the names of its layout and area through [`placeOf`](#placeof).

## `validate`

```cpp
void validate(const World& world);
```

Checks the world and throws `std::invalid_argument` for the first problem it finds. A game calls it
once after building the world, so a wrong link is found at startup and not when a player reaches it.
Each message names the layout and the area by index and by name, and the layer, slot or opening by
name: `WorldKit: layout 0 'overworld' area 1 'lake' opening 'east door' …`.

| Rejected | Message ends |
|---|---|
| a layout whose name is empty | `layout N has no name` |
| two layouts with one name | `is named twice` |
| an area whose name is empty | `area N has no name` |
| two areas of one layout with one name | `is named twice` |
| a cell size of zero or less | `'s cell size must be positive` |
| an area with no collision map | `has no collision map` |
| an area with no layers | `has no layers` |
| a layer whose name is empty | `has a layer with no name` |
| an actor slot with no name | `has an actor slot with no name` |
| two layers or slots in one area with one name | `layer '…' is named twice` |
| a layer that sets `art` and leaves `tiles` null: a map layer missing its map | `layer '…' has art but no map` |
| a layer whose map is not the collision map's size | `layer '…''s map differs in size from the collision map` |
| two layers or slots in one area at one depth | `layer '…' is at the same depth as layer '…'` |
| a layer or slot at a depth `z` for which `z × N` or `z × N + N − 1` leaves `std::int32_t`, N the number of areas in the world | `layer '…' is at a depth out of range for a world of N areas` |
| an opening with no name | `has an opening with no name` |
| two openings in one area with one name | `is named twice` |
| an opening neither in a wall nor at a spot, or both | `must be in one wall or at one spot` |
| a span that runs backward or off its wall | `'s span is off its wall` |
| a spot off the map | `is off the map` |
| two openings on one wall whose spans overlap, or two spots that overlap | `overlaps opening '…'` |
| a destination naming a layout that is not in the world | `leads to layout N, which does not exist` |
| a destination naming an area that is not in that layout | `leads to area N of layout M, which does not exist` |
| a destination naming an opening the area does not have | `leads to '…' in area N of layout M, which has no such opening` |
| a fade with an offset | `fades, so it takes no offset` |
| a seam at a spot | `joins seamlessly, so it must be in a wall` |
| a scroll from a spot, or a scroll or seam into a wall that does not face the exit's | `scrolls, so it and '…' must be in facing walls` / `joins seamlessly, so …` |
| a scroll or seam whose two spans differ in length | `scrolls into '…', whose span is a different length` / `joins seamlessly into …` |
| a seam into a layout of another cell size | `joins seamlessly into layout N, whose cell size differs` |
| a seam whose far end is not a seam | `joins seamlessly into '…', which is not seamless` |
| a seam whose far end does not lead back to it | `joins seamlessly into '…', which does not lead back to it` |
| a seam whose far end's offset is not the opposite of its own | `joins seamlessly into '…', whose offset is not the negation of its own` |
| an actor entry whose `actor` is empty | `actor N '…' has no actor` |
| an actor name used twice anywhere in the world, every level together | `actor N '…' is named twice` |
| a layout's actor whose `in` is an area the layout lacks | `actor N '…' is in area A, which the layout lacks` |
| a world's actor whose `in` is a place the world lacks | `actor N '…' is in layout L area A, which the world lacks` |

An actor entry is named by its level, its index among that level's `actors` and its name:
`WorldKit: layout 0 'castle' area 0 'hall' actor 0 'guard' …` for an area's,
`WorldKit: layout 0 'castle' actor 0 '' …` for a layout's, and `WorldKit: actor 0 'marker' …` for the
world's. The entries are checked in the order the scene spawns them, so the second use of a name is the
one named. Unnamed entries are any number.

The depth rule keeps every depth in the engine's range whatever is on screen: with N areas in the
world, a layer declared at `z` is drawn between `z × N` and `z × N + N − 1`. Left to your game's own
tests: that the map's cells agree with the openings (a doorway cell at each span, open in the collision
map), and that a scroll or a fade leads back.

## `ActorZ`

```cpp
std::int32_t ActorZ(const World& world, Place place, std::string_view slot);
```

The depth the area at `place` declares for its actor slot named `slot`: the depth the slot is drawn at
while that area is alone on screen.

```cpp
const std::int32_t declared = WorldKit::ActorZ(world, {.layout = Layout::Overworld, .area = Area::Lake}, "actors");
```

Call it to read back what the world declares. For the depth to build a layer at, call the form that
takes the area's `Shown` ([camera.md](camera.md#actorz-layerz-by-shown)): it equals this one while the
area is alone on screen and is spread among the areas on screen otherwise.

Throws `std::out_of_range`, naming the layout, the area and the slot, when `slot` is missing from the
area's slots.

## `LayerZ`

```cpp
std::int32_t LayerZ(const World& world, Place place, std::string_view name);
```

The depth the area at `place` declares for its layer named `name`, a [reserved layer](#reservedlayer)
or a map layer: the depth the layer is drawn at while that area is alone on screen.

```cpp
const std::int32_t rain = WorldKit::LayerZ(world, {.layout = Layout::Overworld, .area = Area::Meadow}, "rain");
```

As with `ActorZ`, the form that takes the area's `Shown` ([camera.md](camera.md#actorz-layerz-by-shown))
is the depth to build a layer at.

Throws `std::out_of_range`, naming the layout, the area and the layer, when `name` is missing from the
area's layers.

## `placeOf`

```cpp
Place placeOf(const World& world, std::string_view layout, std::string_view area);
```

The place of the area named `area` in the layout named `layout`. Call it where a game names its places
in text: a table of warps, a save file, a debug console.

```cpp
const WorldKit::Place treasury = WorldKit::placeOf(world, "dungeon-one", "treasury");
WorldKit::Camera camera{treasury.layout, treasury.area};
```

| Called with | Returns or throws |
|---|---|
| a layout and an area of it, by name | their place |
| a layout the world has, and an area it lacks | `std::out_of_range`: `WorldKit: layout N '…' has no area '…'` |
| a layout the world lacks | `std::out_of_range`: `WorldKit: the world has no layout '…'` |

It walks the world's layouts and areas in order. A game that looks the same place up every tick looks
it up once and keeps the `Place`.
