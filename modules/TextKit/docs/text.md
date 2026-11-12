# Reference

Every type and function in `include/TextKit/`, with the rules each follows and what throws. The model
behind them is in [concepts.md](concepts.md).

## Contents

- [Font](#font) — `font.h`
- [glyphOf](#glyphof) — `font.h`
- [Align and TextOptions](#align-and-textoptions) — `text.h`
- [Text](#text) — `text.h`
- [wrap](#wrap) — `text.h`
- [measure](#measure) — `text.h`
- [Reveal](#reveal) — `reveal.h`

## Font

```cpp
struct Font {
    retropp::AtlasManifest sheet;
    std::string_view glyphs;
    retropp::PaletteId palette{};
    std::optional<retropp::PaletteId> shadow;
    retropp::Vec2i advance{.x = 8, .y = 8};
};
```

| Field | What it is |
|---|---|
| `sheet` | The glyph sheet as `Renderer::loadAtlas` or `uploadAtlas` returns it, usually a `Tileset`. Each slot is one glyph: its `tile` and its `dimensions` are the sprite's `tile` and `size`. Its transparent index is what shows around each glyph. Copied into the font. |
| `glyphs` | The character of each slot, in slot order: character n is what slot n draws. A view — the string it views outlives the font. A space in it marks a blank cell and is never drawn. |
| `palette` | The palette the glyphs are drawn through. |
| `shadow` | When set, each glyph is also drawn one pixel right and one down through this palette, one `z` below. |
| `advance` | How far the pen moves: `x` per character, `y` per line. |

## glyphOf

```cpp
std::optional<std::size_t> glyphOf(const Font& font, char character) noexcept;
```

The slot `character` is drawn from:

1. a space has none;
2. the first slot `glyphs` lists the character at;
3. otherwise, for a letter A–Z or a–z, the first slot listing its other case;
4. otherwise `std::nullopt`.

With `glyphs = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"`: `'C'` and `'c'` are slot 2, `'7'` is slot 33,
`'%'` and `' '` are `std::nullopt`.

## Align and TextOptions

```cpp
enum class Align : std::uint8_t { Left, Center, Right };

struct TextOptions {
    std::size_t shown = std::string_view::npos;
    Align align = Align::Left;
    int width = 0;
    std::int32_t z = 0;
};
```

| Field | What it does |
|---|---|
| `shown` | Draw only the first `shown` characters, counting every character as written. `npos`, the default, draws them all. |
| `align` | Where each line sits within `width`. |
| `width` | The width in pixels lines align within. 0 is the passage's own width, its `measure`. |
| `z` | The glyphs' `z` within their layer; shadows are at `z - 1`. |

## Text

```cpp
std::vector<retropp::Sprite> Text(std::string_view key, std::string_view text, retropp::Vec2i at,
                                  const Font& font, const TextOptions& options = {});
```

The sprites `text` is. The pen starts at `at`; each character moves it `advance.x` right, and `'\n'`
moves it back to `at.x` and `advance.y` down. A space moves it and draws nothing. Each line moves right
by its alignment's offset, where `room = width − line length × advance.x`:

| `align` | Offset |
|---|---|
| `Left` | 0 |
| `Center` | `room / 2`, integer division |
| `Right` | `room` |

A line wider than the width has a negative `room` and moves left.

For character n of `text`, when the font has a glyph for it and n is below `shown`:

| Sprite field | Glyph | Shadow (when the font has one) |
|---|---|---|
| `key` | `"<key>:<n>"` | `"<key>:<n>:shadow"` |
| `x`, `y` | the pen, plus the line's offset | one more each |
| `z` | `options.z` | `options.z - 1` |
| `size` | the slot's `dimensions` | the same |
| `atlas` | `font.sheet.atlasId` | the same |
| `tile` | the slot's `tile` | the same |
| `palette` | `font.palette` | `*font.shadow` |

The shadow comes immediately before its glyph in the vector. Every other `Sprite` field keeps its
default.

**Throws** `std::out_of_range`, naming the character in quotes, when the font has no glyph for a
character of `text` — every character is checked, shown or not — or when `glyphs` names a slot past the
end of the sheet.

**Worked values.** With 8×8 cells, `glyphs` starting `"ABCDEFGHI"` and a shadow,
`Text("t", "HI", {10, 20}, font)` is four sprites:

| `key` | `x` | `y` | `z` | `tile` | `palette` |
|---|---|---|---|---|---|
| `t:0:shadow` | 11 | 21 | −1 | 7 | shadow |
| `t:0` | 10 | 20 | 0 | 7 | palette |
| `t:1:shadow` | 19 | 21 | −1 | 8 | shadow |
| `t:1` | 18 | 20 | 0 | 8 | palette |

`"A B"` keys its glyphs `t:0` and `t:2`, at x 10 and 26. `"AB\nABCD"` centered within a width of 64
starts its first line 24 pixels right of `at.x` and its second 16; with a width of 0, the first starts 8
right and the second at `at.x`.

## wrap

```cpp
std::string wrap(std::string_view text, int columns);
```

`text` with `'\n'` put in at spaces so that no line is longer than `columns` characters:

- a `'\n'` already in `text` is kept, and each line between them is wrapped on its own;
- the run of spaces where a line breaks is dropped; spaces anywhere else are kept;
- a word longer than `columns` takes a line to itself and runs past;
- a `columns` below 1 puts each word on a line of its own.

| `text` | `columns` | Result |
|---|---|---|
| `"the quick brown fox"` | 9 | `"the quick\nbrown fox"` |
| `"to abcdefghijkl and"` | 9 | `"to\nabcdefghijkl\nand"` |
| `"the\nquick brown fox"` | 9 | `"the\nquick\nbrown fox"` |
| `"the quick   brown"` | 9 | `"the quick\nbrown"` |
| `"a  b"` | 9 | `"a  b"` |

## measure

```cpp
retropp::PixelSize measure(std::string_view text, const Font& font) noexcept;
```

The pixels `text` takes: its longest line's character count times `advance.x` across, its line count
times `advance.y` down. Spaces count as characters. A passage with n `'\n'`s has n + 1 lines, so `""`
is one line tall and 0 wide. With 8×8 advance, `"AB\nABCD\nA"` measures 32×24.

## Reveal

```cpp
struct Reveal {
    std::uint32_t ticksPerGlyph = 2;
    void advance() noexcept;
    void skip() noexcept;
    std::size_t shown() const noexcept;
    bool done(std::size_t length) const noexcept;
};
```

| Member | What it does |
|---|---|
| `ticksPerGlyph` | The ticks each character takes to appear. 0 is read as 1. |
| `advance()` | One tick more. |
| `skip()` | Shows everything from here on: `shown()` is `std::string_view::npos`. |
| `shown()` | The ticks advanced divided by `ticksPerGlyph`, or `npos` after `skip`. Pass it as `TextOptions::shown`. |
| `done(length)` | Whether `shown() >= length`: a passage of `length` characters is all out. |

A `Reveal{}` shows 0. At the default of 2 ticks a glyph, it shows 1 after 2 advances and 2 after 5;
`done(2)` is true from the fourth advance. Every character counts toward `length`, spaces and `'\n'`
included, as `Text` counts them — pass the passage's `size()`.
