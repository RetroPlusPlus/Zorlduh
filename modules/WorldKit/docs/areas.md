# Areas reference

What a world is made of, and the check that it holds together. The types are in
`include/WorldKit/area.h`.

```cpp
#include "WorldKit/area.h"   // Side, Transition, Span, Destination, Opening, CellValue, Area, CameraLaw, Layout, World, Place, validate
```

## Contents

- [`Side`, `Transition`, `Span`](#side-transition-span)
- [`Destination`](#destination)
- [`Opening`](#opening)
- [`Area`](#area)
- [`CameraLaw`](#cameralaw)
- [`Layout`](#layout)
- [`World`, `Place`](#world-place)
- [`validate`](#validate)

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

## `Area`

```cpp
struct Area {
    const retropp::IndexGrid* tiles = nullptr;       // one map value per cell
    retropp::TileCatalog art{};                      // which tile and palette each map value draws as
    const retropp::IndexGrid* collision = nullptr;   // the same size; one value is open
    std::vector<Opening> openings;
};
```

The maps are the game's and must outlive the world. `art` is an engine `TileCatalog`, so two areas
drawn from the same map values can look different, and one area can draw from several sheets.

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
    std::vector<Area> areas;                 // indices are area ids
    int cellSize = 8;                        // the side of one map cell, in pixels
    CellValue open{};                        // the one collision-map value that is not solid
    CameraKit::View view{};                  // the screen the camera shows, and where it sits on a layer
    CameraLaw camera = CameraKit::follow;    // where the camera is within an area
    std::uint32_t scrollTicks = 32;          // a scroll's length
    int carry = 16;                          // how far a scroll carries the thing through, in pixels
    bool scrollAligns = false;               // whether a scroll also moves the camera across the exit's axis
    std::uint32_t fadeTicks = 16;            // a fade's length each way
    retropp::Rgba8 fadeColor{};              // what the screen fades to
};
```

A set of areas that share these settings. `open` is a `CellValue`, which takes the collision map's own
enumerator (`Cell::Open`) or a number. A scroll slides the camera along the exit's axis; with
`scrollAligns` it also moves across that axis, as it slides, the least it must for the view to lie
within the next area when that area is at least as big as the view there — a smaller area lands where
the opening lines it up either way. The scroll settings are those of the layout being left; a fade
darkens in the layout being left and lightens in the one entered. A layout is a plain value: it copies,
and the camera reads it on every call rather than keeping a pointer to it.

## `World`, `Place`

```cpp
struct World {
    std::vector<Layout> layouts;   // indices are layout ids
};

struct Place {
    int layout = 0;
    int area = 0;
};
```

The world is its layouts; every camera call takes it. A place is a layout and an area in it, the pair
the module hands back when a game needs both (the place entered, the areas shown).

## `validate`

```cpp
void validate(const World& world);
```

Checks the world and throws `std::invalid_argument` for the first problem it finds. A game calls it
once after building the world, so a wrong link is found at startup and not when a player reaches it.
Each message names the layout and area by index and, where there is one, the opening by name:

| Rejected | Message ends |
|---|---|
| a cell size of zero or less | `'s cell size must be positive` |
| an area with either map missing | `has no tile map or no collision map` |
| an area whose maps differ in size | `'s tile map and collision map differ in size` |
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

Not checked: that the map's cells agree with the openings (a doorway cell at each span, open in the
collision map), or that a scroll or a fade leads back. A game that wants either tests its own data for
it.
