# Messages

Reference for `dialog.h` and `dialog_box.h`.

```cpp
#include "WindowKit/dialog.h"       // Dialog
#include "WindowKit/dialog_box.h"   // DialogText, DialogArrow, DialogBox
```

A `Dialog` is the message; the `Animation` it holds is on [animation.md](animation.md).

## Contents

- [Dialog](#dialog)
- [The box opening and closing](#the-box-opening-and-closing)
- [A message, tick by tick](#a-message-tick-by-tick)
- [Drawing a message](#drawing-a-message)

## Dialog

```cpp
struct Dialog {
    std::uint32_t ticksPerGlyph = 2;
    std::uint32_t arrowBlinkTicks = 16;
    Animation animation;

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

A message being read: its box opening and closing, its text cut into pages that fit a text area, which
page is up, and how much of it is typed out. It is a value: a game keeps it in its state and copies it
like anything else there.

- **`ticksPerGlyph`** — the ticks each character takes to appear, as TextKit's `Reveal`; 0 is read as 1.
- **`arrowBlinkTicks`** — how long the arrow shows, then hides, once a page is done; 0 keeps it on.
- **`animation`** — how the message's box opens and closes ([animation.md](animation.md#animation)). With
  no length, the default, the box is open the moment a message starts and gone the moment it ends. Give
  it a length before a message starts — `dialog.animation.duration = std::chrono::milliseconds{150};`, or
  a whole `Animation` assigned — and the box opens and closes over that time. The dialog advances it; a
  game never does.

### open

Starts `text`. It is wrapped with TextKit's `wrap` to as many characters as `area` is wide in the font
(`area.width / font.advance.x`) and cut into pages of as many lines as it is tall
(`area.height / font.advance.y`), at least one of each; an advance of 0 on an axis counts as 1 pixel. Each
page is its lines joined by `'\n'`, and the dialog keeps its own copy. The box opens, and the first page
types from its first character once the box is open.

Opening while a message is showing replaces its pages, and the box carries on from where it is: an open
box stays open with the new page typing, and a closing one opens back out. An empty `text` opens nothing
and leaves the dialog as it was.

Hand `open` the size of `inside(box, border)` for the box the message is drawn in, so the pages are the
pages the box has room for.

### advance

One tick, and one rule: **confirm finishes whatever is in progress.** Nothing while no message is
showing. Otherwise the box's animation moves on a tick, and then:

- **while the box opens**, `confirm` opens it at once, and the page waits for the next tick to type;
- **while it is open**, the page types on one tick, and with `confirm` a page still typing is shown whole,
  a whole page gives way to the next, typed from its start, and the last page, whole, starts the close;
- **while it closes**, `confirm` closes it at once; on the tick it is closed nothing is showing.

The tick the opening ends on is the page's first tick of typing.

### close

Starts the close, as the last page's confirm does: the box closes over its length, and once it has,
nothing is showing, `pageCount()` is 0 and `text()` is empty. With an instant animation that is at once.
`close` while the box opens turns the opening into a closing from where it is.

### What it reports

| Call | Is |
|---|---|
| `showing()` | a message is showing: from the opening's first tick to the closing's last |
| `page()`, `pageCount()` | the page up, from 0, and how many there are (0 when closed) |
| `text()` | the page up, its lines joined by `'\n'`; empty when nothing is showing |
| `shown()` | how much of the page is typed, as `Reveal::shown`: `std::string_view::npos` once a confirm has shown it whole |
| `pageDone()` | the whole page is shown |
| `lastPage()` | the page up is the last |
| `arrowOn()` | the page is done, another follows, and the blink is on: on for `arrowBlinkTicks` ticks from the tick the page is done, then off as long, and so on |
| `animation.phase()`, `animation.progress()` | where the box is: closed, opening, open or closing, and how far open |

Every call answers while the box opens and closes; the components below decide what is drawn then.

## The box opening and closing

The dialog holds its box's `Animation` and drives it from `advance`, which a game already calls once per
tick. A game sets the animation's length and easing; the dialog does the rest:

- `open` starts the opening; the text types once the box is open;
- the last page's confirm, or `close`, starts the closing; the message is gone once the box is closed;
- a confirm finishes an opening or a closing at once.

How the moving box looks is the style `DialogBox` is given ([Drawing a message](#drawing-a-message)).

## A message, tick by tick

A passage paged to a room of 18 characters by 2 lines, `ticksPerGlyph` 2, with an instant animation:

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

The same passage with `ticksPerGlyph` 1 and an animation of four linear ticks:

```
open(…)                   showing(), the box Opening at progress 0; shown() == 0
advance(false) × 3        Opening, progress 0.75; nothing typed
advance(false)            Open; the page types its first tick: shown() == 1
advance(true)             the page is shown whole
advance(true)             page 1
advance(true)             shown whole
advance(true)             Closing at progress 1; showing() still, page 1 still reported
advance(false) × 3        Closing, progress 0.25
advance(false)            Closed: showing() is false, pageCount() is 0
```

A confirm in either moving stretch finishes it: `advance(true)` on the first tick of the opening is Open
with nothing typed, and on any tick of the closing is closed.

## Drawing a message

```cpp
std::vector<retropp::Sprite> DialogText(std::string_view key, const Dialog& dialog, retropp::Vec2i at,
                                        const TextKit::Font& font, std::int32_t z = 0);
std::vector<retropp::Sprite> DialogArrow(std::string_view key, const Dialog& dialog, retropp::IntRect area,
                                         const Border& border, std::int32_t z = 0);
retropp::DrawLayer DialogBox(std::string_view key, std::int32_t z, retropp::IntRect box, const Border& border,
                             const TextKit::Font& font, const Dialog& dialog, const Style& style = {},
                             LayerKit::LayerOptions options = {});
```

- **`DialogText`** — the page up as sprites, typed as far as the dialog has got: TextKit's `Text` keyed
  `"<key>:<page>"`, so each character is `"<key>:<page>:<n>"` and a new page's glyphs are new sprites,
  laid out from `at`. Nothing unless the dialog's box is open.
- **`DialogArrow`** — the border sheet's `kArrowSlot`, at its slot's size, through the border's palette,
  its bottom-right corner at the bottom-right corner of `area`, keyed `"<key>:arrow:<page>"`. Nothing unless
  the dialog's box is open, `arrowOn()`, and the sheet has that slot.
- **`DialogBox`** — the message box: a `Window` over `box` holding the page, from the top-left of
  `inside(box, border)`, and the arrow at that room's bottom-right, one z above the text, opening and
  closing with the dialog's animation in `style` ([animation.md](animation.md#style)) — grow from the box's
  middle unless given another. While the box opens or closes it is the box alone. While no message is
  showing it is a sprite layer keyed `key` at `z` with no sprites, so a game adds it every frame. With an
  instant animation no style is ever seen.

```cpp
// Each frame.
frame.layers.push_back(WindowKit::DialogBox("message", kMessageZ, kMessageBox, border, font, state.dialog));

// The box sliding up from below a 144-pixel screen instead of growing.
frame.layers.push_back(WindowKit::DialogBox("message", kMessageZ, kMessageBox, border, font, state.dialog,
                                            WindowKit::Style::slide({kMessageBox.x, 144})));
```
