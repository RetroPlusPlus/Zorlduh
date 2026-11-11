# Areas reference

What a world is made of, and the check that it holds together. The types are in
`include/WorldKit/area.h`.

```cpp
#include "WorldKit/area.h"   // Side, Transition, Span, Destination, Opening, CellValue, Area, CameraLaw, Layout, validate
```

## Contents

- [`Side`, `Transition`, `Span`](#side-transition-span)
- [`Destination`](#destination)
- [`Opening`](#opening)
- [`Area`](#area)
- [`CameraLaw`](#cameralaw)
- [`Layout`](#layout)
- [`validate`](#validate)

## `Side`, `Transition`, `Span`

```cpp
enum class Side : std::uint8_t { None, North, South, East, West };
enum class Transition : std::uint8_t { Scroll, Fade };
struct Span { int first = 0; int last = 0; };   // cells along a side, both ends included
```

A side is the wall an edge opening is in; `None` is the value of an opening that is not in a wall. A span
runs down the east and west walls (rows) and across the north and south walls (columns), in cells,
counted from the map's top-left.

## `Destination`

```cpp
inline constexpr int kNoArea = -1;

struct Destination {
    int area = kNoArea;     // the area's index in the layout
    std::string opening;    // the name of an opening in it
};
```

Where an opening leads. A game names its areas with an enum whose values are their indices, so
`.area = Area::Lake` is the usual form. `kNoArea` means nowhere: the opening is only an entrance.

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
{.name = "pit bottom", .at = {.x = 3, .y = 3, .width = 1, .height = 1}},   // an entrance only
```

A scroll opening is in a wall and leads to an opening in the facing wall of the next area whose span is
the same length. A fade opening is either kind and leads to either kind.

## `Area`

```cpp
struct Area {
    const retropp::IndexGrid* tiles = nullptr;       // one map value per cell
    retropp::TileCatalog art{};                      // which tile and palette each map value draws as
    const retropp::IndexGrid* collision = nullptr;   // the same size; one value is open
    std::vector<Opening> openings;
};
```

The maps are the game's and must outlive the layout. `art` is an engine `TileCatalog`, so two areas
drawn from the same map values can look different, and one area can draw from several sheets.

## `CameraLaw`

```cpp
using CameraLaw = std::function<retropp::Vec2i(retropp::IntRect bounds, retropp::PixelSize view, retropp::Vec2i focus)>;
```

Where the camera should be within an area: given the area's rectangle (in its own pixels, so from
(0, 0)), the view's size and the point the camera looks at. `CameraKit::follow` is one; any function of
this shape fits, including one that reads the game's own state.

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
    std::uint32_t fadeTicks = 16;            // a fade's length each way
    retropp::Rgba8 fadeColor{};              // what the screen fades to
};
```

`open` is a `CellValue`, which takes the collision map's own enumerator (`Cell::Open`) or a number. A
layout is a plain value: it copies, and the camera reads it on every call rather than keeping a pointer
to it.

## `validate`

```cpp
void validate(const Layout& layout);
```

Checks the layout and throws `std::invalid_argument` for the first problem it finds. A game calls it
once after building the layout, so a wrong link is found at startup and not when a player reaches it.
Each message names the area by index and, where there is one, the opening by name:

| Rejected | Message ends |
|---|---|
| a cell size of zero or less | `the cell size must be positive` |
| an area with either map missing | `has no tile map or no collision map` |
| an area whose maps differ in size | `'s tile map and collision map differ in size` |
| an opening with no name | `has an opening with no name` |
| two openings in one area with one name | `is named twice` |
| an opening neither in a wall nor at a spot, or both | `must be in one wall or at one spot` |
| a span that runs backward or off its wall | `'s span is off its wall` |
| a spot off the map | `is off the map` |
| two openings on one wall whose spans overlap, or two spots that overlap | `overlaps opening '…'` |
| a destination naming an area that is not in the layout | `leads to area N, which does not exist` |
| a destination naming an opening the area does not have | `leads to '…' in area N, which has no such opening` |
| a scroll from a spot, or into a wall that does not face the exit's | `scrolls, so it and '…' must be in facing walls` |
| a scroll whose two spans differ in length | `scrolls into '…', whose span is a different length` |

Not checked: that the map's cells agree with the openings (a doorway cell at each span, open in the
collision map), or that a link leads back. A game that wants either tests its own data for it.
