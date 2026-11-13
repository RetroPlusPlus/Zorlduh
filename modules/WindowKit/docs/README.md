# WindowKit — guide

The guide to the WindowKit module, for anyone drawing windows and message boxes in a game on the
Polyrhythm engine. Every page is written against the shipped headers in `include/WindowKit/`.

**New here? Start with [getting-started.md](getting-started.md)**: a complete program, a window you
resize a pixel at a time and a message box opening in a style you pick and typing out a three-page
message, explained part by part. Then read [concepts.md](concepts.md) for the model,
[window.md](window.md), [animation.md](animation.md) and [dialog.md](dialog.md) for every type and rule,
and [how-to.md](how-to.md) for recipes.

## What the module is

WindowKit draws a box of any size from a sheet of border pieces, puts a game's sprites inside it as one
layer, opens and closes it over time in a style the game picks, and runs a message box: a passage wrapped
and paged to the box, typed out a character at a time, with an arrow while another page follows.
Everything it uses is the engine's own or its neighbors': a
border sheet is a `retropp::AtlasManifest`, its colors a `retropp::PaletteId`, a box is
`std::vector<retropp::Sprite>`, a window is a `retropp::DrawLayer` built by LayerKit's `SpriteLayer`, and a
message's text is TextKit's.

What the module takes care of:

- a box of any width and height in pixels from pieces of whatever size the sheet is carved at: corners,
  edges repeated along each side, a fill repeated inside;
- the cut: a piece with less room than its own size is drawn only as far as its room;
- keys that name each piece's place in the box and the box's size, so the engine sees the same sprites
  from frame to frame and a box at a new size is drawn whole;
- a window as one sprite layer, the box beneath whatever the game puts in it;
- opening and closing: an animation's progress over a time or a count of ticks along an easing curve,
  and five styles drawn from it — grow, clip, squash, fade, slide — with the window's contents shown only
  once it is open;
- a message: its box opened and closed, its text wrapped to the room inside the box, cut into pages,
  typed out, with a blinking arrow while a page follows, and one confirm finishing whatever is in
  progress.

What stays with the game: the border sheets and their palettes, the font, where a window goes and how
big it is, the depth it is drawn at, how long a window takes to open and close and the style it does it
in, what a message says, and which button confirms.

## Index

### Start here

| Page | Covers |
|---|---|
| [getting-started.md](getting-started.md) | A complete program, a window resized a pixel at a time with its size and the message box's style written inside, and a message box opening and closing in six styles with a three-page message, built as `WindowKit-example-getting-started`, explained part by part |
| [concepts.md](concepts.md) | A border is a sheet of pieces; a box is a rectangle and the pieces laid into it; the cut and why any size works; keys by place and size; a window is one layer; an animation is state and a style is a function; a message is state the tick advances and components that read it; where each part lives in a game |
| [how-to.md](how-to.md) | Recipes: a HUD panel, a window that hugs its text, a box with no fill, two windows stacked, a message the player cannot skip, another confirm button, a border of larger pieces, several borders, a portrait beside a message, closing a message from the game, a window that grows from a corner, a window that slides in from the bottom of the screen, a window that pops, the same style on every window, skipping an opening |

### Reference

| Page | Covers | Header |
|---|---|---|
| [window.md](window.md) | `Piece`, `kArrowSlot`, `Border`, `thickness`, `inside`, `Box`, `Window` | `border.h`, `box.h`, `window.h` |
| [animation.md](animation.md) | `Phase`, `Animation`, `Grow`, `Clip`, `Squash`, `Fade`, `Slide`, `middle`, `Style`, `Animated`, `Animate`, `Window` with an animation | `animation.h`, `animate.h`, `window.h` |
| [dialog.md](dialog.md) | `Dialog`, `DialogText`, `DialogArrow`, `DialogBox` | `dialog.h`, `dialog_box.h` |

## Coverage

| Surface | Status | Page |
|---|---|---|
| A box of any pixel size from a border of any piece size | available | concepts.md, window.md |
| Pieces cut to the room they have | available | concepts.md, window.md |
| Keys by a piece's place in the box and the box's size | available | concepts.md, window.md |
| A window: the box and a game's sprites on one sprite layer | available | window.md |
| Opening and closing animation, over a time or a count of ticks, along any easing | available | animation.md |
| — grow from a point (the box's middle, a corner, any point) | available | animation.md, how-to.md |
| — clip: the whole box revealed through a growing rectangle | available | animation.md |
| — squash along either axis | available | animation.md |
| — fade | available | animation.md |
| — slide from a point | available | animation.md, how-to.md |
| A message wrapped and paged to its box, typed out, confirmed | available | dialog.md |
| A message's box opening and closing, a confirm finishing either | available | dialog.md |
| An arrow blinking while another page follows | available | dialog.md |
| Any number of borders in one game | available | how-to.md |
| Sound on opening or closing | not provided | — |
| A cursor, choices and menus | not provided | — |
| Edges drawn from several alternating pieces | not provided | — |
| A title on the border | not provided | — |
| Effects within a message (a color change, a pause) | not provided | — |
| A portrait beside a message | not provided — a sprite the game puts in the window | how-to.md |
