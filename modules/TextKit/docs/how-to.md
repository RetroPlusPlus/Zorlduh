# How-to

Recipes for common text on screen. Each one is a few lines over the types in [text.md](text.md).

## Contents

- [Center a title](#center-a-title)
- [Type a message out and let the player skip it](#type-a-message-out-and-let-the-player-skip-it)
- [Describe a sheet drawn in another order](#describe-a-sheet-drawn-in-another-order)
- [Use a lowercase-only font](#use-a-lowercase-only-font)
- [Dim or highlight a run](#dim-or-highlight-a-run)
- [Use a font of 8×16 cells](#use-a-font-of-816-cells)
- [Put text in a HUD strip](#put-text-in-a-hud-strip)

## Center a title

Start the pen at the left edge and center within the screen's width:

```cpp
TextKit::Text("title", "The Lost Shrine", {.x = 0, .y = 24}, font,
              {.align = TextKit::Align::Center, .width = 160});
```

For a title centered in a box, start the pen at the box's left edge and pass the box's width. A
multi-line title centers each line on its own.

## Type a message out and let the player skip it

Keep the wrapped message and a `Reveal` in the game's state. Advance the reveal each tick; a press
while it is typing shows the rest, and a press once it is done closes the message:

```cpp
struct Message {
    std::string text;          // wrapped once, when the message opens
    TextKit::Reveal reveal;
};

// The tick.
message.reveal.advance();
if (input.justPressed(Action::Confirm)) {
    if (!message.reveal.done(message.text.size())) {
        message.reveal.skip();
    } else {
        state.message.reset();   // or open the next one with a fresh Reveal
    }
}

// The component.
TextKit::Text("message", message.text, {.x = 8, .y = 104}, font, {.shown = message.reveal.shown()});
```

`ticksPerGlyph` sets the speed: 1 for a character every tick, 4 for a slow, deliberate line.

## Describe a sheet drawn in another order

Write `glyphs` in the order the sheet's cells are read, whatever that order is. A hand-drawn sheet of
digits first, then capitals, then a blank cell and three marks:

```cpp
const TextKit::Font font{.sheet = sheet,
                         .glyphs = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ _.-",
                         .palette = white};
```

The blank cell is written as a space so the `_`, `.` and `-` after it stay on their own cells. Cells
after the last character the string names can hold anything; load the sheet with `count` set to the
string's length so they are not slots.

## Use a lowercase-only font

A sheet of small letters only is described with small letters. Text in capitals is drawn with them:

```cpp
const TextKit::Font quiet{.sheet = lowercaseSheet, .glyphs = "abcdefghijklmnopqrstuvwxyz.,'", .palette = ink};
TextKit::Text("whisper", "WHO GOES THERE?", at, quiet);   // throws: the font has no '?'
TextKit::Text("whisper", "WHO GOES THERE", at, quiet);    // draws "who goes there"
```

## Dim or highlight a run

Make a second font over the same sheet with another palette, and choose between them by state. The
key stays the same, so a line that becomes selected is the same sprites in a new color:

```cpp
const TextKit::Font normal{.sheet = sheet, .glyphs = kGlyphs, .palette = gray};
const TextKit::Font selected{.sheet = sheet, .glyphs = kGlyphs, .palette = gold};

for (std::size_t i = 0; i < options.size(); ++i) {
    const TextKit::Font& font = i == state.cursor ? selected : normal;
    const std::vector<retropp::Sprite> line = TextKit::Text(
        "option:" + std::to_string(i), options[i], {.x = 24, .y = 40 + 16 * static_cast<int>(i)}, font);
    sprites.insert(sprites.end(), line.begin(), line.end());
}
```

For one word in another color inside a line, draw the word as a passage of its own with its own key,
starting where the text before it ends:

```cpp
const std::string_view before = "Take the ";
TextKit::Text("line", before, at, white);
TextKit::Text("line:key-word", "Silver Key", {.x = at.x + TextKit::measure(before, white).width, .y = at.y}, gold);
```

## Use a font of 8×16 cells

Load the sheet with 8×16 cells and set `advance` to match. Each sprite takes its size from the slot, so
nothing else changes:

```cpp
const retropp::AtlasManifest tall = renderer.loadAtlas("assets/gfx/font-tall.png",
                                                       retropp::AssetDimensions{.width = 8, .height = 16},
                                                       retropp::ContentKind::Tileset);
const TextKit::Font font{.sheet = tall, .glyphs = kGlyphs, .palette = white, .advance = {.x = 8, .y = 16}};
```

For a pixel between lines, make `advance.y` 17. `wrap` counts characters, so it is unchanged; `measure`
uses `advance`, so it follows.

## Put text in a HUD strip

A HUD stays put while the world scrolls, so its text goes on a sprite layer that does not scroll, above
the world's layers. The sprites are in screen pixels:

```cpp
const std::vector<retropp::Sprite> hud =
    TextKit::Text("hud:score", "Score 1200", {.x = 0, .y = 4}, font,
                  {.align = TextKit::Align::Right, .width = 152});
frame.layers.push_back(retropp::DrawLayer{.key = "hud", .z = kHudZ, .size = viewport,
                                          .content = retropp::SpriteContent{.sprites = hud}});
```

A layer's scroll defaults to {0, 0}, which is what a HUD wants. Right-aligning within the strip's width
less a margin keeps a changing value's right edge still as its length changes.
