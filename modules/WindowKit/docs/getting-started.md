# Getting started

This page builds a complete program on the module: a window near the top of the screen with its own
size written inside it, which the arrow keys or a pad's directions grow and shrink a pixel at a time, and
a message box along the bottom with a three-page message typed out a character at a time. Space, Return
or a pad's A button shows the page at once, turns it, and closes the message after the last; pressed
again, it opens the message again.

The full source is [`examples/getting_started.cpp`](../examples/getting_started.cpp). It builds as the
`WindowKit-example-getting-started` target whenever `WINDOWKIT_BUILD_EXAMPLES` is on, so what you read here
is what compiles. Its art is in [`examples/assets/`](../examples/assets/): open the PNGs in any image
viewer alongside this page.

## Contents

- [1. Build it](#1-build-it)
- [2. The border sheet](#2-the-border-sheet)
- [3. The font and the palettes](#3-the-font-and-the-palettes)
- [4. Loading them](#4-loading-them)
- [5. The message's room](#5-the-messages-room)
- [6. The tick](#6-the-tick)
- [7. Drawing it](#7-drawing-it)
- [8. Where to go next](#8-where-to-go-next)

## 1. Build it

The module needs the engine as the CMake target `retropp::engine`, and the LayerKit and TextKit modules;
the engine's `docs/guide/build-and-consume.md` covers building it. Add the modules and turn the examples
on:

```cmake
add_subdirectory(engine)
add_subdirectory(modules/LayerKit)
add_subdirectory(modules/TextKit)
set(WINDOWKIT_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(modules/WindowKit)
```

```sh
cmake -S . -B build
cmake --build build --target WindowKit-example-getting-started
```

The executable lands in the build tree under `modules/WindowKit/`. Run it: a window reading 93 X 41 at the
top, and along the bottom a message box typing A MESSAGE BOX WRAPS ITS TEXT TO THE ROOM. Close the
window to quit.

## 2. The border sheet

[`examples/assets/border.png`](../examples/assets/border.png) is 24×32 pixels: four rows of three 8×8
cells, read left to right and then down.

```
top-left     top     top-right
left         fill    right
bottom-left  bottom  bottom-right
arrow        (blank) (blank)
```

The first nine cells are the pieces of a box, laid out as the box looks: the order of `WindowKit::Piece`.
The tenth, `WindowKit::kArrowSlot`, is the arrow a message box shows while another page follows. The last
two cells pad the row and are not loaded.

This sheet draws a double line: each edge is a light line, a dark gap and another light line, around a
dark fill. Its pieces are 8×8 because that is how this art is drawn; a border's pieces are whatever size
its sheet is carved at ([how-to.md](how-to.md#a-border-of-larger-pieces)).

The sheet's pixels are palette indices: index 0 is transparent, 1 the dark fill and gap, and 3 the lines
and the arrow.

## 3. The font and the palettes

[`examples/assets/font.png`](../examples/assets/font.png) is a sheet of 8×8 glyphs, 16 to a row: the
uppercase letters, the digits and `.,!?'-:`, drawn in index 3 with index 0 transparent.

[`examples/assets/palettes/`](../examples/assets/palettes/) holds two palette images, each 4×1 pixels, one
pixel per entry:

| File | Colors |
|---|---|
| `window.png` | entry 0 transparent, 1 a dark navy, 2 a mid blue, 3 a pale blue-white |
| `text.png` | entry 0 transparent, 1 black, 2 gray, 3 white |

## 4. Loading them

```cpp
const WindowKit::Border border{
    .sheet = renderer.loadAtlasFromMemory(readAsset("border.png"), retropp::AssetDimensions{.width = 8, .height = 8},
                                          retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
                                          /*count=*/static_cast<int>(WindowKit::kArrowSlot + 1),
                                          retropp::TransparentIndices::of({0})),
    .palette = loadPalette(renderer, "palettes/window.png"),
};
const TextKit::Font font{
    .sheet = renderer.loadAtlasFromMemory(readAsset("font.png"), retropp::AssetDimensions{.width = 8, .height = 8},
                                          retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
                                          /*count=*/static_cast<int>(kGlyphs.size()),
                                          retropp::TransparentIndices::of({0})),
    .glyphs = kGlyphs,
    .palette = loadPalette(renderer, "palettes/text.png"),
};
```

- **The border** is the sheet carved into its cells and the palette its pieces are drawn through. `count`
  stops the carve after the arrow, so the two padding cells are not slots.
- **`TransparentIndices::of({0})`** makes index 0 draw nothing, so whatever is under a window shows around
  a corner drawn with transparent pixels.
- **The font** is TextKit's: the glyph sheet, `kGlyphs` naming the character of each cell, and its colors.

`readAsset` reads a file from the example's own `examples/assets/` directory and `loadPalette` uploads a
palette image's colors, as in TextKit's example. A game loads its own sheets with
`renderer.loadAtlas("assets/…", …)` and its palettes with `renderer.loadPaletteImage("assets/…")`; the
result is the same `AtlasManifest` and `PaletteId`.

## 5. The message's room

```cpp
constexpr retropp::IntRect kMessageBox{.x = 0, .y = 96, .width = 160, .height = 48};

const retropp::IntRect messageRoom = WindowKit::inside(kMessageBox, border);
WindowKit::Dialog dialog;
dialog.open(kMessage, {.width = messageRoom.width, .height = messageRoom.height}, font);
```

`inside` takes the border's thickness, the size of its top-left piece, off each side of the box: 160×48
less 8 on every side is 144×32 at (8, 104). That is the room the text has, so `open` wraps the message to
18 characters a line (144 pixels at the font's 8-pixel step) and cuts it into pages of 4 lines (32
pixels). TextKit's `wrap` breaks each line at the last space that fits, and the message is three pages:

```
A MESSAGE BOX           PAGES IT. EACH          FOLLOWS. PRESS
WRAPS ITS TEXT TO       PAGE TYPES OUT,         AGAIN TO TURN THE
THE ROOM INSIDE         AND AN ARROW            PAGE, OR TO CLOSE
ITS BORDER AND          BLINKS WHILE MORE       IT AFTER THE LAST.
```

## 6. The tick

```cpp
retropp::PixelSize size{.width = 93, .height = 41};
loop.simTick([&](const retropp::InputState& input) {
    if (input.justPressed(Action::Wider)) {
        size.width = std::min(size.width + 1, 160 - 2 * kResizableAt.x);
    }
    // … Narrower, Taller, Shorter the same way …
    const bool next = input.justPressed(Action::Next);
    if (!dialog.showing()) {
        if (next) {
            dialog.open(kMessage, {.width = messageRoom.width, .height = messageRoom.height}, font);
        }
        return;
    }
    dialog.advance(next);
});
```

The program's state is the resizable window's size and the `Dialog`. Each press of a direction changes
the size by one pixel.

`advance` is called once per tick. Without a confirm, the page types on: one character every
`ticksPerGlyph` ticks, two by default. With a confirm, a page still typing is shown whole, a whole page
gives way to the next, and the last page closes the message. Once it is closed, the next press opens it
again.

## 7. Drawing it

```cpp
const retropp::IntRect box{.x = kResizableAt.x, .y = kResizableAt.y, .width = size.width, .height = size.height};
const retropp::IntRect room = WindowKit::inside(box, border);
const std::string label = std::to_string(size.width) + " X " + std::to_string(size.height);
frame.layers = {
    WindowKit::Window("resizable", 0, box, border,
                      {TextKit::Text("resizable:size", label, {.x = room.x, .y = room.y}, font)}),
    WindowKit::DialogBox("message", 1, kMessageBox, border, font, dialog),
};
```

- **`Window`** returns one sprite layer: the box, then the label the program puts at the top-left of the
  room. The box is drawn beneath everything put in the window.
- **The box** at 93×41 is corners at its four corners, edges repeated along each side, and the fill
  repeated inside. The room is 77×25, which no whole number of 8-pixel pieces fills: the last column and
  row of the fill, and the last repeat of each edge, are cut where the room ends. Grow or shrink the
  window a pixel at a time to see the pieces cut cleanly at every size.
- **`DialogBox`** is the message box: a window over `kMessageBox` with the page typed so far inside it and
  the arrow at the room's bottom-right while another page follows. While no message is showing it is an
  empty layer, so the program adds it every frame.

## 8. Where to go next

- The model: pieces, the cut, keys, a message as state — [concepts.md](concepts.md).
- Every type and rule — [window.md](window.md) and [dialog.md](dialog.md).
- Recipes: a HUD panel, a window that hugs its text, several borders, a message the player cannot skip —
  [how-to.md](how-to.md).
