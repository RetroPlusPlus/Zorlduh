# TextKit — guide

The guide to the TextKit module, for anyone putting text on screen in a game on the Polyrhythm engine.
Every page is written against the shipped headers in `include/TextKit/`.

**New here? Start with [getting-started.md](getting-started.md)**: a complete program, a title and a
passage typed out beneath it, explained part by part. Then read [concepts.md](concepts.md) for the
model, [text.md](text.md) for every type and rule, and [how-to.md](how-to.md) for recipes.

## What the module is

TextKit puts text on screen from a glyph sheet: a font says which character each cell of the sheet is,
and a passage becomes one sprite per glyph, laid out in lines, wrapped to a width, aligned, with a drop
shadow if the font has one, and typed out over ticks if the game wants it. Everything it uses is the
engine's own: the sheet is a `retropp::AtlasManifest`, the colors are `retropp::PaletteId`s, and what a
passage draws is a `std::vector<retropp::Sprite>`, so the sprites go on any sprite layer.

What the module takes care of:

- the font as a sheet plus a string naming the character of each slot, and the case rule: a character
  is drawn by its own glyph, or else by its other case's;
- one sprite per glyph, keyed by the character's position in the passage, so a glyph keeps its key as
  more of the passage is shown;
- lines, wrapping at spaces, measuring, and aligning each line within a width;
- the drop shadow: a second sprite one pixel right and down, through its own palette, beneath the glyph;
- the reveal: how much of a passage is shown, one character every few ticks, or all of it at once.

What stays with the game: the sheet and its palettes, what the text says, where it goes, the layer it
is on, and when a reveal starts or is skipped.

## Index

### Start here

| Page | Covers |
|---|---|
| [getting-started.md](getting-started.md) | A complete program, a centered title with a shadow and a passage typed out beneath it, built as `TextKit-example-getting-started`, explained part by part |
| [concepts.md](concepts.md) | A font is a sheet and a string; the case rule; a passage is sprites keyed by position; lines and the pen; wrapping; alignment; the shadow; the reveal; where each part lives in a game |
| [how-to.md](how-to.md) | Recipes: center a title, type a message out and let the player skip it, a sheet in another order, a lowercase-only font, dim or highlight a run, a font of 8×16 cells, text in a HUD strip |

### Reference

| Page | Covers | Header |
|---|---|---|
| [text.md](text.md) | `Font`, `glyphOf`, `Align`, `TextOptions`, `Text`, `wrap`, `measure`, `Reveal` | `font.h`, `text.h`, `reveal.h` |

## Coverage

| Surface | Status | Page |
|---|---|---|
| A font from any glyph sheet in any slot order | available | text.md |
| Uppercase-only, lowercase-only and mixed-case sheets | available | concepts.md, text.md |
| One sprite per glyph, keyed by character position | available | concepts.md, text.md |
| Lines, wrapping at spaces, measuring | available | text.md |
| Left, center and right alignment within a width | available | text.md |
| A drop shadow through its own palette | available | concepts.md, text.md |
| A typewriter reveal, and skipping it | available | text.md |
| Glyphs of different widths (proportional fonts, kerning) | not provided | — |
| Text as cells on a tile layer | not provided | — |
| More than one color within one passage | not provided — draw the run with a second font ([how-to.md](how-to.md#dim-or-highlight-a-run)) | how-to.md |
| Characters outside ASCII | not provided | — |
| A cursor, choices, pages | not provided — a menu's concern | — |
| A box drawn around the text | not provided — a window's concern | — |
