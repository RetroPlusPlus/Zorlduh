# TextKit — text from a glyph sheet

`TextKit` puts text on screen from a glyph sheet: a font says which character each cell of the sheet
is, and a passage becomes one sprite per glyph, laid out in lines, wrapped to a width, aligned, with a
drop shadow if the font has one, and typed out over ticks if the game wants it. It is for games on the
[Polyrhythm](https://github.com/RetroPlusPlus/Polyrhythm) engine.

```cpp
// What the font is: the sheet as loaded, the character of each of its cells, and its colors.
const TextKit::Font font{.sheet = glyphSheet, .glyphs = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?",
                         .palette = white, .shadow = black};

// Each tick.
reveal.advance();

// Each frame.
TextKit::Text("message", TextKit::wrap("Hello there, traveler!", 18), {.x = 8, .y = 96}, font,
              {.shown = reveal.shown()});
```

| Piece | What it is | Page |
|---|---|---|
| `Font` | a glyph sheet, the character each slot is, the palette, an optional shadow palette, the pen's step | [docs/text.md](docs/text.md#font) |
| `glyphOf` | the slot a character draws, by its own case or else its other case | [docs/text.md](docs/text.md#glyphof) |
| `Text` | a passage as sprites, one per glyph, keyed by character position | [docs/text.md](docs/text.md#text) |
| `wrap`, `measure` | a passage broken into lines of a width; the pixels a passage takes | [docs/text.md](docs/text.md#wrap) |
| `Reveal` | how much of a passage has been typed out, advanced once per tick | [docs/text.md](docs/text.md#reveal) |

The module is header-only, depends on the engine alone (`retropp::engine`), includes no header from the
game it sits in, and needs C++20. It is meant to leave this repository as a standalone module.

## Using it

```cmake
add_subdirectory(modules/TextKit)
target_link_libraries(my-game PRIVATE TextKit::TextKit)
```

The directory's `CMakeLists.txt` defines the INTERFACE target `TextKit` (alias `TextKit::TextKit`) and
expects the engine as the target `retropp::engine`. `TEXTKIT_BUILD_EXAMPLES` (on by default when the
directory is the top-level project) builds `TextKit-example-getting-started`, the program
[docs/getting-started.md](docs/getting-started.md) walks through.

## Documentation

**Start with [docs/getting-started.md](docs/getting-started.md)**, then [docs/concepts.md](docs/concepts.md)
for the model and [docs/how-to.md](docs/how-to.md) for recipes. The full index is
[docs/README.md](docs/README.md).

## Layout

```
modules/TextKit/
  CMakeLists.txt                    the TextKit::TextKit target and the example option
  include/TextKit/
    font.h                          Font, glyphOf
    text.h                          Align, TextOptions, Text, wrap, measure
    reveal.h                        Reveal
  examples/getting_started.cpp      the getting-started program
  examples/assets/                  its font sheet and palette images
  docs/                             the guide
```

`text.h` is the only header that builds a draw type (`retropp::Sprite`). `reveal.h` includes no engine
header at all, so a game's simulation can hold a `Reveal` without anything to draw.
