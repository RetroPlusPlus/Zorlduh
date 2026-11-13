# WindowKit — windows and message boxes

`WindowKit` draws a box of any size from a sheet of border pieces, puts a game's sprites inside it as
one layer, opens and closes it over time — growing, clipped, squashed, faded or slid — and runs a message
box: a passage wrapped and paged to the box, typed out a character at a time, with an arrow while another
page follows. It is for games on the
[Polyrhythm](https://github.com/RetroPlusPlus/Polyrhythm) engine, and builds on the LayerKit and TextKit
modules.

```cpp
// What the border is: a sheet carved into the nine pieces of a box (and an arrow), and its colors.
const WindowKit::Border border{.sheet = borderSheet, .palette = borderColors};

// When a message starts, paged to the room inside the box.
const retropp::IntRect room = WindowKit::inside(box, border);
dialog.open("Welcome to the meadow.", {.width = room.width, .height = room.height}, font);

// Each tick.
dialog.advance(confirmPressed);

// Each frame.
frame.layers.push_back(WindowKit::DialogBox("message", kMessageZ, box, border, font, dialog));
```

| Piece | What it is | Page |
|---|---|---|
| `Border`, `Piece`, `kArrowSlot` | a sheet of box pieces and its palette | [docs/window.md](docs/window.md#border) |
| `thickness`, `inside` | the border's thickness; the room a box leaves inside it | [docs/window.md](docs/window.md#thickness-and-inside) |
| `Box` | a rectangle of any size filled and framed by the pieces, as sprites | [docs/window.md](docs/window.md#box) |
| `Window` | a box and the sprites a game puts inside it, as one sprite layer | [docs/window.md](docs/window.md#window) |
| `Animation`, `Phase` | a window opening and closing over time: how far open it is | [docs/animation.md](docs/animation.md#animation) |
| `Grow`, `Clip`, `Squash`, `Fade`, `Slide`, `Style`, `Animate` | the styles a window opens and closes in | [docs/animation.md](docs/animation.md#the-styles) |
| `Dialog` | a message being read: its box opening and closing, its pages, the page up, how much is typed | [docs/dialog.md](docs/dialog.md#dialog) |
| `DialogText`, `DialogArrow`, `DialogBox` | the page, the arrow, and the whole message box | [docs/dialog.md](docs/dialog.md#drawing-a-message) |

The module is header-only, depends on the engine (`retropp::engine`), LayerKit (`LayerKit::LayerKit`) and
TextKit (`TextKit::TextKit`), includes no header from the game it sits in, and needs C++20. It is meant
to leave this repository as a standalone module.

## Using it

```cmake
add_subdirectory(modules/LayerKit)
add_subdirectory(modules/TextKit)
add_subdirectory(modules/WindowKit)
target_link_libraries(my-game PRIVATE WindowKit::WindowKit)
```

The directory's `CMakeLists.txt` defines the INTERFACE target `WindowKit` (alias `WindowKit::WindowKit`)
and expects the three targets above. `WINDOWKIT_BUILD_EXAMPLES` (on by default when the directory is the
top-level project) builds `WindowKit-example-getting-started`, the program
[docs/getting-started.md](docs/getting-started.md) walks through.

## Documentation

**Start with [docs/getting-started.md](docs/getting-started.md)**, then [docs/concepts.md](docs/concepts.md)
for the model and [docs/how-to.md](docs/how-to.md) for recipes. The full index is
[docs/README.md](docs/README.md).

## Layout

```
modules/WindowKit/
  CMakeLists.txt                    the WindowKit::WindowKit target and the example option
  include/WindowKit/
    border.h                        Piece, kArrowSlot, Border, thickness, inside
    box.h                           Box
    window.h                        Window
    animation.h                     Phase, Animation
    animate.h                       Grow, Clip, Squash, Fade, Slide, middle, Style, Animated, Animate
    dialog.h                        Dialog
    dialog_box.h                    DialogText, DialogArrow, DialogBox
  examples/getting_started.cpp      the getting-started program
  examples/assets/                  its border sheet, font sheet and palette images
  docs/                             the guide
```

`border.h`, `animation.h` and `dialog.h` build no draw type, so a game's simulation can hold a `Border`,
an `Animation` and a `Dialog`. `box.h`, `animate.h`, `window.h` and `dialog_box.h` return sprites, regions,
transforms and layers.
