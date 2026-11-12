# Concepts

How TextKit models text, and where each part lives in a game. The reference for every type is
[text.md](text.md).

## Contents

- [A font is a sheet and a string](#a-font-is-a-sheet-and-a-string)
- [The case rule](#the-case-rule)
- [A passage is sprites, keyed by position](#a-passage-is-sprites-keyed-by-position)
- [Lines and the pen](#lines-and-the-pen)
- [Wrapping comes before layout](#wrapping-comes-before-layout)
- [Alignment within a width](#alignment-within-a-width)
- [The shadow is a second sprite beneath](#the-shadow-is-a-second-sprite-beneath)
- [The reveal is a value the game advances](#the-reveal-is-a-value-the-game-advances)
- [Where each part lives in a game](#where-each-part-lives-in-a-game)

## A font is a sheet and a string

A glyph sheet is an image carved into cells, one glyph per cell. The engine loads it as an
`AtlasManifest`: its slots are the cells in read order, each with its tile and its size. The manifest
knows where each glyph is but not which character it is, so the font adds that as a string, `glyphs`,
whose character n is what slot n draws.

```cpp
const TextKit::Font font{.sheet = sheet, .glyphs = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?",
                         .palette = white};
```

Any slot order works: write the string in the order the sheet is drawn. A sheet with a blank cell in
the middle lists that cell as a space, which keeps every later character in line with its slot. Space
is never drawn, whether the string lists it or not.

## The case rule

A sheet holds the letters its art has: some draw only capitals, some only small letters, some both. The
font draws each character by its own glyph when the sheet has one, and otherwise by the glyph of its
other case:

| Sheet holds | `"Hello"` draws as |
|---|---|
| A–Z | HELLO |
| a–z | hello |
| A–Z and a–z | Hello |

So a game writes its text once, in the case it reads best, and any font draws it. A character the sheet
has in neither case — a `%` in a font without one — makes `Text` throw `std::out_of_range` naming the
character, so a typo in a passage is found the first time it is drawn.

## A passage is sprites, keyed by position

`Text` returns a `std::vector<retropp::Sprite>`, one per glyph. Each sprite's key is the call's key and
the character's position in the passage: `"msg:0"`, `"msg:1"`, and so on. Every character counts,
spaces and line breaks included, so the n-th character of a passage always has the same key.

That matters because the engine matches sprites from one frame to the next by key. When a reveal shows
one more character, the sprites already on screen keep their keys and only the new one is added. When a
passage is laid out again — moved, realigned — each glyph is the same sprite in a new place.

The key a game passes names the passage: two passages on screen at once need two keys.

## Lines and the pen

The pen starts at the point the game gives, the top-left of the first glyph. Each character moves it
`advance.x` to the right; a `'\n'` moves it back to the start and `advance.y` down. A space moves the pen
and draws nothing.

`advance` defaults to 8 by 8, a font of 8-pixel cells. A font of 16-pixel cells, or one with a pixel
between lines, says so in its `advance`.

## Wrapping comes before layout

`Text` draws lines where the passage has `'\n'`. To fit a passage to a width, `wrap` puts the breaks in
first:

```cpp
const std::string message = TextKit::wrap("the quick brown fox", 9);   // "the quick\nbrown fox"
```

It breaks at spaces only, dropping the spaces where it breaks. A word longer than the width keeps a line
to itself and runs past it. The result is a `std::string` the game keeps while it draws from it,
usually made once when the passage is chosen, not every frame.

Wrapping counts characters, not pixels: with every glyph one `advance.x` wide, a width of n characters
is n times that many pixels.

## Alignment within a width

Each line is placed within a width by `TextOptions::align`: `Left` starts it at the pen's start, `Right`
ends it at the width's end, `Center` puts it halfway. Each line is aligned on its own, so a centered
passage of lines of different lengths has a ragged edge on both sides. The width defaults to the
passage's own (its longest line), which centers or right-aligns the shorter lines under the longest;
give a width to align across a box or the whole screen.

## The shadow is a second sprite beneath

A font with a `shadow` palette draws every glyph twice: once through its palette, and once one pixel
right and one pixel down through the shadow palette, one `z` below. The shadow sprite comes first in the
vector and has its own key, the glyph's with `:shadow` on the end.

A shadow palette is usually one dark color in every entry the glyphs use, so the second copy reads as a
solid shadow behind the first. A font whose art already has its own outline or shade leaves `shadow`
unset.

## The reveal is a value the game advances

A `Reveal` is how much of a passage is shown: a count of ticks, divided by `ticksPerGlyph`. The game
holds it in its state, calls `advance` once per tick, and hands `shown()` to `Text` when it draws.
`skip` shows everything; `done(length)` says whether the whole passage is out. A new passage starts with
a new `Reveal`.

Because it only counts, the same `Reveal` drives anything that shows a passage a piece at a time, and a
game's tests can advance it and read it without anything on screen.

## Where each part lives in a game

| Part | Where it lives |
|---|---|
| The glyph sheet and its palettes | the game's assets, loaded once at startup |
| The `Font` | built once beside them, from the loaded sheet and palettes |
| The text itself, wrapped | wherever the game keeps what is said: a table, its state |
| The `Reveal` | the game's state, advanced in its tick |
| The sprites | a component that reads the state and calls `Text` |
| The layer | the game's, at whatever depth its text belongs |
