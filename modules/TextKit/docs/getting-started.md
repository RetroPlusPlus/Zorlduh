# Getting started

This page builds a complete program on the module: the word TEXTKIT centered across the top of the
screen in gold with a gray shadow, and beneath it a passage in white, typed out a character at a time.
Space, Return or a pad's A button shows the rest of the passage at once; pressed again, it starts the
next passage.

The full source is [`examples/getting_started.cpp`](../examples/getting_started.cpp). It builds as the
`TextKit-example-getting-started` target whenever `TEXTKIT_BUILD_EXAMPLES` is on, so what you read here
is what compiles. Its art is in [`examples/assets/`](../examples/assets/): open the PNGs in any image
viewer alongside this page.

## Contents

- [1. Build it](#1-build-it)
- [2. The glyph sheet](#2-the-glyph-sheet)
- [3. The palettes](#3-the-palettes)
- [4. Loading them](#4-loading-them)
- [5. The fonts](#5-the-fonts)
- [6. The passages](#6-the-passages)
- [7. The tick](#7-the-tick)
- [8. Drawing it](#8-drawing-it)
- [9. Where to go next](#9-where-to-go-next)

## 1. Build it

The module needs the engine as the CMake target `retropp::engine`; the engine's
`docs/guide/build-and-consume.md` covers building it. Add the module and turn its examples on:

```cmake
add_subdirectory(engine)
set(TEXTKIT_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(modules/TextKit)
```

```sh
cmake -S . -B build
cmake --build build --target TextKit-example-getting-started
```

The executable lands in the build tree under `modules/TextKit/`. Run it: TEXTKIT in gold across the
top, and beneath it EACH GLYPH IS TYPED OUT IN TURN. appearing a letter at a time. Close the window to
quit.

## 2. The glyph sheet

[`examples/assets/font.png`](../examples/assets/font.png) is 128×128 pixels: an 8×8 grid of 16×16 cells,
one glyph per cell, read left to right and then down.

```
0 1 2 3 4 5 6 7
8 9 A B C D E F
G H I J K L M N
O P Q R S T U V
W X Y Z   _ . -
! ? < > ( ) * &
^ $ # @ % + [ ]
" '
```

The cell after Z is blank. After `'` come three more cells, a caret pointing up, a caret pointing down
and an underline: art for something other than text, which this program does not load.

The sheet is an indexed PNG whose pixels are palette indices, not colors. Index 0 is the background
around each glyph; index 1 is its dark outline and index 2 the shade along its right and lower edges;
indices 4 to 7 are its face, 7 at the top of each letter down to 4 at the bottom. The colors come from
whichever palette a glyph is drawn through.

## 3. The palettes

[`examples/assets/palettes/`](../examples/assets/palettes/) holds four palette images, each 8×1 pixels:
one pixel per palette entry, entry 0 at the left.

| File | Entries 4–7, the face |
|---|---|
| `text_white.png` | grays rising to near white |
| `text_gold.png` | browns rising to pale gold |
| `text_cyan.png` | teals rising to pale cyan |
| `text_gray.png` | dark grays |

In all four, entry 0 is fully transparent and entries 1–3 are the same near-blacks, so only the face
changes color. A palette image is a truecolor PNG; the engine reads its pixels as colors in order.

## 4. Loading them

```cpp
constexpr std::string_view kGlyphs = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ _.-!?<>()*&^$#@%+[]\"'";

const retropp::AtlasManifest sheet = renderer.loadAtlasFromMemory(
    readAsset("font.png"), retropp::AssetDimensions{.width = 16, .height = 16},
    retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
    /*count=*/static_cast<int>(kGlyphs.size()), retropp::TransparentIndices::of({0}));
const retropp::PaletteId white = loadPalette(renderer, "palettes/text_white.png");
const retropp::PaletteId gold = loadPalette(renderer, "palettes/text_gold.png");
const retropp::PaletteId gray = loadPalette(renderer, "palettes/text_gray.png");
```

- **`kGlyphs`** is the sheet written out as a string: character n is the glyph in cell n. The blank cell
  after Z is written as a space, so every cell after it keeps its place in the string.
- **`count`** stops the carve after the characters, so the three cells at the end are not slots.
- **`TransparentIndices::of({0})`** makes index 0 draw nothing, so only the glyphs show.

`readAsset` reads a file from the example's own `examples/assets/` directory, and `loadPalette` uploads
a palette image's colors:

```cpp
retropp::PaletteId loadPalette(retropp::Renderer& renderer, const std::string& name) {
    const std::vector<retropp::Rgba16> colors =
        retropp::slicePaletteImage(retropp::loadPngFromMemory(readAsset(name)));
    return renderer.uploadPalette(std::span<const retropp::Rgba16>(colors));
}
```

The program reads its files itself because they live beside the module rather than in the project that
builds it. A game loads its own font sheet with `renderer.loadAtlas("assets/…", …)` and its palettes with
`renderer.loadPaletteImage("assets/…")`, and the build bakes them into the executable; the result is the
same `AtlasManifest` and `PaletteId`s.

## 5. The fonts

```cpp
const TextKit::Font title{.sheet = sheet, .glyphs = kGlyphs, .palette = gold, .shadow = gray,
                          .advance = {.x = 16, .y = 16}};
const TextKit::Font body{.sheet = sheet, .glyphs = kGlyphs, .palette = white,
                         .advance = {.x = 16, .y = 16}};
```

A font is the sheet, the string, and how the glyphs are colored. Two fonts share one sheet here:

- **`title`** draws through the gold palette, with a shadow through the gray one: each glyph is also
  drawn one pixel right and one down, beneath it.
- **`body`** draws through the white palette and has no shadow.

**`advance`** is how far the pen moves for each character and for each line: 16 pixels, the cell size.

The sheet has uppercase letters only. Text written in lowercase is drawn with the uppercase glyphs, so
"TextKit" draws as TEXTKIT ([concepts.md](concepts.md#the-case-rule)).

## 6. The passages

```cpp
constexpr std::array<std::string_view, 3> kPassages{
    "Each glyph is typed out in turn.",
    "Press Space to show the rest at once.",
    "Press again for the next one!",
};

std::array<std::string, kPassages.size()> passages;
for (std::size_t i = 0; i < kPassages.size(); ++i) {
    passages[i] = TextKit::wrap(kPassages[i], kPassageColumns);
}
```

The screen is 160 pixels wide: ten 16-pixel glyphs. `wrap` breaks each passage at spaces into lines of
at most nine characters, leaving 8 pixels either side. The first becomes:

```
EACH
GLYPH IS
TYPED OUT
IN TURN.
```

The wrapped strings are made once and kept: the sprites are built from them every frame.

## 7. The tick

```cpp
std::size_t passage = 0;
TextKit::Reveal reveal;
loop.simTick([&](const retropp::InputState& input) {
    reveal.advance();
    if (input.justPressed(Action::Next)) {
        if (!reveal.done(passages[passage].size())) {
            reveal.skip();
        } else {
            passage = (passage + 1) % passages.size();
            reveal = TextKit::Reveal{};
        }
    }
});
```

A `Reveal` counts ticks. It shows one more character every `ticksPerGlyph` ticks, two by default, so at
60 ticks a second the passage appears at 30 characters a second. Spaces and line breaks count as
characters, as they are written.

Pressing the action while the passage is still typing calls `skip`, which shows the rest at once. Once
`done` says the whole passage is out, the press moves to the next passage and starts a fresh `Reveal`.

## 8. Drawing it

```cpp
sprites = TextKit::Text("title", "TextKit", {.x = 0, .y = 8}, title,
                        {.align = TextKit::Align::Center, .width = kScreenWidth});
const std::vector<retropp::Sprite> typed =
    TextKit::Text("passage", passages[passage], {.x = 8, .y = 40}, body, {.shown = reveal.shown()});
sprites.insert(sprites.end(), typed.begin(), typed.end());
frame.layers = {
    retropp::DrawLayer{.key = "text", .z = 0, .size = retropp::Renderer::defaultViewport.size(),
                       .content = retropp::SpriteContent{.sprites = sprites}},
};
```

`Text` returns the sprites a passage is:

- **The title** starts at x = 0 and is centered across the 160-pixel width: seven glyphs are 112 pixels,
  so it moves right by half of the 48 left over and starts at x = 24. Each glyph's shadow comes first in
  the vector and sits one z below it.
- **The passage** starts at (8, 40) and steps 16 pixels per character and per line. `shown` is how much
  the reveal has typed; only those characters are returned.

Each sprite is keyed by its character's position: the title's are `title:0` to `title:6`, their shadows
`title:0:shadow` to `title:6:shadow`, and the passage's `passage:0` onward. A glyph keeps its key as
more of the passage appears, so the engine sees the same sprite from frame to frame.

The program puts every sprite on one plain engine sprite layer. A game puts them on whatever sprite
layer it uses. The sprite vector lives outside the render callback because the layer's content is a
span the renderer reads while it draws.

## 9. Where to go next

- The model: a font as a sheet and a string, keys by position, the reveal — [concepts.md](concepts.md).
- Every type, rule and what throws — [text.md](text.md).
- Recipes: a sheet in another order, a lowercase-only font, a highlighted run, text in a HUD —
  [how-to.md](how-to.md).
