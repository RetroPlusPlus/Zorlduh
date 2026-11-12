# WindowKit — guide

The guide to the WindowKit module, for anyone drawing windows and message boxes in a game on the
Polyrhythm engine. Every page is written against the shipped headers in `include/WindowKit/`.

**New here? Start with [getting-started.md](getting-started.md)**: a complete program, a window you
resize a pixel at a time and a message box typing out a three-page message, explained part by part. Then
read [concepts.md](concepts.md) for the model, [window.md](window.md) and [dialog.md](dialog.md) for
every type and rule, and [how-to.md](how-to.md) for recipes.

## What the module is

WindowKit draws a box of any size from a sheet of border pieces, puts a game's sprites inside it as one
layer, and runs a message box: a passage wrapped and paged to the box, typed out a character at a time,
with an arrow while another page follows. Everything it uses is the engine's own or its neighbors': a
border sheet is a `retropp::AtlasManifest`, its colors a `retropp::PaletteId`, a box is
`std::vector<retropp::Sprite>`, a window is a `retropp::DrawLayer` built by LayerKit's `SpriteLayer`, and a
message's text is TextKit's.

What the module takes care of:

- a box of any width and height in pixels from pieces of whatever size the sheet is carved at: corners,
  edges repeated along each side, a fill repeated inside;
- the cut: a piece with less room than its own size is drawn only as far as its room;
- keys that name each piece's place in the box, so the engine sees the same sprites from frame to frame;
- a window as one sprite layer, the box beneath whatever the game puts in it;
- a message: wrapped to the room inside its box, cut into pages, typed out, shown whole or turned or
  closed by one confirm, with a blinking arrow while a page follows.

What stays with the game: the border sheets and their palettes, the font, where a window goes and how
big it is, the depth it is drawn at, what a message says, and which button confirms.

## Index

### Start here

| Page | Covers |
|---|---|
| [getting-started.md](getting-started.md) | A complete program, a window resized a pixel at a time with its size written inside, and a message box with a three-page message, built as `WindowKit-example-getting-started`, explained part by part |
| [concepts.md](concepts.md) | A border is a sheet of pieces; a box is a rectangle and the pieces laid into it; the cut and why any size works; keys by place; a window is one layer; a message is state the tick advances and components that read it; where each part lives in a game |
| [how-to.md](how-to.md) | Recipes: a HUD panel, a window that hugs its text, a box with no fill, two windows stacked, a message the player cannot skip, another confirm button, a border of larger pieces, several borders, closing a message from the game |

### Reference

| Page | Covers | Header |
|---|---|---|
| [window.md](window.md) | `Piece`, `kArrowSlot`, `Border`, `thickness`, `inside`, `Box`, `Window` | `border.h`, `box.h`, `window.h` |
| [dialog.md](dialog.md) | `Dialog`, `DialogText`, `DialogArrow`, `DialogBox` | `dialog.h`, `dialog_box.h` |

## Coverage

| Surface | Status | Page |
|---|---|---|
| A box of any pixel size from a border of any piece size | available | concepts.md, window.md |
| Pieces cut to the room they have | available | concepts.md, window.md |
| Keys by a piece's place in the box | available | concepts.md, window.md |
| A window: the box and a game's sprites on one sprite layer | available | window.md |
| A message wrapped and paged to its box, typed out, confirmed | available | dialog.md |
| An arrow blinking while another page follows | available | dialog.md |
| Any number of borders in one game | available | how-to.md |
| Opening and closing animation | not provided | — |
| A cursor, choices and menus | not provided | — |
| Edges drawn from several alternating pieces | not provided | — |
| A title on the border | not provided | — |
| Effects within a message (a color change, a pause) | not provided | — |
| A portrait beside a message | not provided — a sprite the game puts in the window | how-to.md |
