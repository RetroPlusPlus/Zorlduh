# Messages

Reference for `dialog.h` and `dialog_box.h`.

```cpp
#include "WindowKit/dialog.h"       // Dialog
#include "WindowKit/dialog_box.h"   // DialogText, DialogArrow, DialogBox
```

## Contents

- [Dialog](#dialog)
- [A message, tick by tick](#a-message-tick-by-tick)
- [Drawing a message](#drawing-a-message)

## Dialog

```cpp
struct Dialog {
    std::uint32_t ticksPerGlyph = 2;
    std::uint32_t arrowBlinkTicks = 16;

    void open(std::string_view text, retropp::PixelSize area, const TextKit::Font& font);
    void close() noexcept;
    void advance(bool confirm);

    bool showing() const noexcept;
    std::size_t page() const noexcept;
    std::size_t pageCount() const noexcept;
    std::string_view text() const noexcept;
    std::size_t shown() const noexcept;
    bool pageDone() const noexcept;
    bool lastPage() const noexcept;
    bool arrowOn() const noexcept;
};
```

A message being read: its text cut into pages that fit a text area, which page is up, and how much of it
is typed out. It is a value: a game keeps it in its state and copies it like anything else there.

- **`ticksPerGlyph`** — the ticks each character takes to appear, as TextKit's `Reveal`; 0 is read as 1.
- **`arrowBlinkTicks`** — how long the arrow shows, then hides, once a page is done; 0 keeps it on.

### open

Starts `text`. It is wrapped with TextKit's `wrap` to as many characters as `area` is wide in the font
(`area.width / font.advance.x`) and cut into pages of as many lines as it is tall
(`area.height / font.advance.y`), at least one of each; an advance of 0 on an axis counts as 1 pixel. Each
page is its lines joined by `'\n'`, and the dialog keeps its own copy. The first page starts typing from
its first character.

Opening while a message is showing replaces it. An empty `text` opens nothing and leaves the dialog as it
was.

Hand `open` the size of `inside(box, border)` for the box the message is drawn in, so the pages are the
pages the box has room for.

### advance

One tick. Nothing while no message is showing. Otherwise the page types on one tick, and then, with
`confirm`:

- a page still typing is shown whole;
- a whole page gives way to the next, typed from its start;
- the last page, whole, closes the message.

### close

Ends the message: nothing is showing, `pageCount()` is 0 and `text()` is empty.

### What it reports

| Call | Is |
|---|---|
| `showing()` | a message is open |
| `page()`, `pageCount()` | the page up, from 0, and how many there are (0 when closed) |
| `text()` | the page up, its lines joined by `'\n'`; empty when nothing is showing |
| `shown()` | how much of the page is typed, as `Reveal::shown`: `std::string_view::npos` once a confirm has shown it whole |
| `pageDone()` | the whole page is shown |
| `lastPage()` | the page up is the last |
| `arrowOn()` | the page is done, another follows, and the blink is on: on for `arrowBlinkTicks` ticks from the tick the page is done, then off as long, and so on |

## A message, tick by tick

A passage paged to a room of 18 characters by 2 lines, `ticksPerGlyph` 2:

```
open("The quick brown fox jumps over the lazy dog and runs far away.", {144, 16}, font)
    pageCount() == 2
    text() == "The quick brown\nfox jumps over the"
advance(false) × 5        shown() == 2: "Th"
advance(true)             the page is shown whole; arrowOn()
advance(false) × 15       arrowOn() still
advance(false)            16 ticks done: the arrow hides
advance(true)             page 1, "lazy dog and runs\nfar away.", shown() == 0
advance(true)             shown whole; lastPage(), so no arrow
advance(true)             closed: showing() is false
```

## Drawing a message

```cpp
std::vector<retropp::Sprite> DialogText(std::string_view key, const Dialog& dialog, retropp::Vec2i at,
                                        const TextKit::Font& font, std::int32_t z = 0);
std::vector<retropp::Sprite> DialogArrow(std::string_view key, const Dialog& dialog, retropp::IntRect area,
                                         const Border& border, std::int32_t z = 0);
retropp::DrawLayer DialogBox(std::string_view key, std::int32_t z, retropp::IntRect box, const Border& border,
                             const TextKit::Font& font, const Dialog& dialog,
                             LayerKit::LayerOptions options = {});
```

- **`DialogText`** — the page up as sprites, typed as far as the dialog has got: TextKit's `Text` keyed
  `"<key>:<page>"`, so each character is `"<key>:<page>:<n>"` and a new page's glyphs are new sprites,
  laid out from `at`. Nothing while no message is showing.
- **`DialogArrow`** — the border sheet's `kArrowSlot`, at its slot's size, through the border's palette,
  its bottom-right corner at the bottom-right corner of `area`, keyed `"<key>:arrow:<page>"`. Nothing unless
  `arrowOn()` and the sheet has that slot.
- **`DialogBox`** — the message box: a `Window` over `box` holding the page, from the top-left of
  `inside(box, border)`, and the arrow at that room's bottom-right, one z above the text. While no message
  is showing it is a sprite layer keyed `key` at `z` with no sprites, so a game adds it every frame.

```cpp
// Each frame.
frame.layers.push_back(WindowKit::DialogBox("message", kMessageZ, kMessageBox, border, font, state.dialog));
```
