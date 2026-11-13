# Borders, boxes and windows

Reference for `border.h`, `box.h` and `window.h`.

```cpp
#include "WindowKit/border.h"   // Piece, kArrowSlot, Border, thickness, inside
#include "WindowKit/box.h"      // Box
#include "WindowKit/window.h"   // Window, plain and with an animation
```

## Contents

- [Border](#border)
- [thickness and inside](#thickness-and-inside)
- [Box](#box)
- [Worked boxes](#worked-boxes)
- [Window](#window)
  - [With an animation](#with-an-animation)

## Border

```cpp
enum class Piece : std::size_t {
    TopLeft, Top, TopRight,
    Left, Fill, Right,
    BottomLeft, Bottom, BottomRight,
};

inline constexpr std::size_t kArrowSlot = 9;

struct Border {
    retropp::AtlasManifest sheet;
    retropp::PaletteId palette{};
};
```

- **`Piece`** is the order of the pieces in a border sheet's slots: the box as it looks, three across and
  three down.
- **`kArrowSlot`** is the slot after the nine pieces: the arrow a message box shows while another page
  follows. A sheet without it draws no arrow.
- **`Border`** is a sheet carved into the pieces, as the renderer's `loadAtlas` or `uploadAtlas` returns
  it, and the palette they are drawn through. A piece is the size its slot is. The sheet's transparent
  index is what shows around a rounded corner. A sheet with fewer than nine slots draws nothing.

## thickness and inside

```cpp
retropp::PixelSize thickness(const Border& border) noexcept;
retropp::IntRect   inside(retropp::IntRect box, const Border& border) noexcept;
```

- **`thickness`** is the size of the border's top-left piece; `{0, 0}` for a sheet with fewer than nine
  slots.
- **`inside`** is `box` with the thickness taken off each side: where a window's contents go. Its width and
  height are never negative; a box thinner than two thicknesses leaves 0.

```cpp
WindowKit::inside({.x = 10, .y = 20, .width = 24, .height = 24}, border8);   // {18, 28, 8, 8}
WindowKit::inside({.x = 0, .y = 0, .width = 20, .height = 10}, border8);     // {8, 8, 4, 0}
WindowKit::inside({.x = 0, .y = 0, .width = 50, .height = 50}, border16);    // {16, 16, 18, 18}
```

(`border8` and `border16` are borders whose sheets are carved into 8×8 and 16×16 pieces.)

## Box

```cpp
std::vector<retropp::Sprite> Box(std::string_view key, retropp::IntRect box, const Border& border,
                                 std::int32_t z = 0);
```

A box as sprites: `box` filled and framed by the border's pieces, in the pixels of the layer the sprites
are put on.

**Layout.** With `t` the border's thickness:

- the corners sit at the box's four corners;
- the top and bottom edges repeat left to right across `[box.x + t.width, box.x + box.width − t.width)`,
  each repeat its slot's width; the bottom edge sits against the box's bottom;
- the left and right edges repeat top to bottom across `[box.y + t.height, box.y + box.height − t.height)`,
  each its slot's height; the right edge sits against the box's right;
- the fill repeats across `inside(box, border)`, left to right, then down.

**The cut.** A repeat that runs past the end of its span — an edge's last, the fill's last column and last
row — carries a region keyed `"cut"`: a rectangle in the sprite's own quad over the part past the end, with
one `ScreenSpaceEffectKind::Transparency` effect, so that part is not drawn. A fill piece cut on both axes
carries two, the right part and then the bottom part. Every piece is also cut to the box, so a box
narrower or shorter than a corner cuts its corners. A piece that fits carries no region.

**Small and empty boxes.** A box narrower than two corners draws them overlapping, the right over the
left; one shorter than two, the bottom over the top. A span of no length draws no repeat. A box with no
width or no height, or a border whose sheet has fewer than nine slots, is no sprites. `Box` never throws.

**Order and z.** The fill first, then the edges (top, bottom, left, right), then the corners (top-left,
top-right, bottom-left, bottom-right), all at `z`: equal z keeps that order, so the corners draw over the
edges and the edges over the fill.

**Keys.** Each piece is keyed by its place in the box and the box's size, `<width>` and `<height>` being
the box's:

| Piece | Key |
|---|---|
| fill, column c, row r | `"<key>:fill:<c>:<r>:<width>x<height>"` |
| top edge, repeat n | `"<key>:top:<n>:<width>x<height>"` |
| bottom edge, repeat n | `"<key>:bottom:<n>:<width>x<height>"` |
| left edge, repeat n | `"<key>:left:<n>:<width>x<height>"` |
| right edge, repeat n | `"<key>:right:<n>:<width>x<height>"` |
| top-left corner | `"<key>:top-left:<width>x<height>"` |
| top-right corner | `"<key>:top-right:<width>x<height>"` |
| bottom-left corner | `"<key>:bottom-left:<width>x<height>"` |
| bottom-right corner | `"<key>:bottom-right:<width>x<height>"` |

A box that moves at one size keeps every key, and the engine eases all of it together. A box drawn at a
new size shares no key with the box before it, so it is drawn whole on the tick it changes, every piece in
place with its cut, whichever way it grew: the engine eases a sprite's position between ticks but never
its regions ([concepts.md](concepts.md#keys-by-place)).

**Each sprite** is its slot's `tile` and `size` from the border's sheet, its `atlas`, and the border's
`palette`, with `x`/`y` the piece's top-left.

## Worked boxes

Over a border of 8×8 pieces:

**24×24 at (10, 20)** — nine sprites, none cut:

| Key | At |
|---|---|
| `w:fill:0:0:24x24` | (18, 28) |
| `w:top:0:24x24` | (18, 20) |
| `w:bottom:0:24x24` | (18, 36) |
| `w:left:0:24x24` | (10, 28) |
| `w:right:0:24x24` | (26, 28) |
| `w:top-left:24x24` | (10, 20) |
| `w:top-right:24x24` | (26, 20) |
| `w:bottom-left:24x24` | (10, 36) |
| `w:bottom-right:24x24` | (26, 36) |

**93×41 at (0, 0)** — the room is 77×25 at (8, 8):

- the top edge is ten repeats from x = 8; the tenth, at x = 80, has 5 pixels of room, and its cut is the
  rectangle (5, 0, 3, 8);
- the fill is ten columns by four rows; the last column is cut at (5, 0, 3, 8), the last row, with 1
  pixel of room, at (0, 1, 8, 7), and the last cell carries both;
- the right edge is four repeats at x = 85, the last cut at (0, 1, 8, 7); the bottom edge sits at y = 33;
- 72 sprites in all.

**12×24** — narrower than two corners: the top-left at x = 0 and the top-right at x = 4, overlapping,
the top-right drawn over; no top or bottom repeat. At **5×24**, narrower than one corner, the top-left
is cut at (5, 0, 3, 8) and the top-right, at x = −3, at (0, 0, 3, 8).

Over a border of 16×16 pieces, **50×50** has a thickness of 16 and a room of 18×18: each edge is two
repeats, the second cut at (2, 0, 14, 16) along the top, and the fill is two by two, cut the same way.

## Window

```cpp
retropp::DrawLayer Window(std::string_view key, std::int32_t z, retropp::IntRect box, const Border& border,
                          std::initializer_list<LayerKit::Sprites> children,
                          LayerKit::LayerOptions options = {});
```

A window as one sprite layer keyed `key` at `z`: the box, keyed `"<key>:box"`, then each child in order.
The box's sprites are at the lowest z there is, so the box is beneath every child whatever the child's
own z. Every option passes onto the layer untouched: scroll, alpha, blend, effects, regions, transform.

The children are placed by the game, in the layer's pixels; `inside(box, border)` is where the border
leaves room. The layer is LayerKit's `SpriteLayer`, which keeps the sprites under the layer's key until
the next call with that key, so each key is called once per frame.

```cpp
const retropp::IntRect room = WindowKit::inside(box, border);
frame.layers.push_back(WindowKit::Window("status", kHudZ, box, border,
                                         {TextKit::Text("status:name", "HERO", {room.x, room.y}, font),
                                          Hearts(state, {room.x, room.y + 8})}));
```

### With an animation

```cpp
retropp::DrawLayer Window(std::string_view key, std::int32_t z, retropp::IntRect box, const Border& border,
                          const Animation& animation, const Style& style,
                          std::initializer_list<LayerKit::Sprites> children,
                          LayerKit::LayerOptions options = {});
```

A window that opens and closes over time: while closed, an empty sprite layer keyed `key` at `z`; while
it opens or closes, the box alone drawn in `style` for where `animation` is; once open, the plain `Window`
with its children. The animation and the styles are on [animation.md](animation.md).
