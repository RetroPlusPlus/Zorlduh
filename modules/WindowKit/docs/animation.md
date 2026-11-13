# Opening and closing

Reference for `animation.h` and `animate.h`, and the `Window` that takes an animation.

```cpp
#include "WindowKit/animation.h"   // Phase, Animation
#include "WindowKit/animate.h"     // Grow, Clip, Squash, Fade, Slide, middle, Style, Animated, Animate
#include "WindowKit/window.h"      // Window, with an animation and a style
```

A window that opens and closes over time is two things: an **`Animation`**, state the tick advances that
says how far open the window is, and a **style**, a function from that to what a box or a layer takes.
The engine's `retropp::Animation`, a run of sprite frames, is another type; outside the namespace this one
is written `WindowKit::Animation`.

## Contents

- [Animation](#animation)
- [An opening, tick by tick](#an-opening-tick-by-tick)
- [The styles](#the-styles)
- [Style](#style)
- [Animate](#animate)
- [Window with an animation](#window-with-an-animation)

## Animation

```cpp
enum class Phase : std::uint8_t { Closed, Opening, Open, Closing };

struct Animation {
    std::chrono::nanoseconds duration{};
    std::uint32_t ticks = 0;
    retropp::Easing easing = retropp::Easing::InOutQuad;
    retropp::TimingProfile timing = retropp::TweenPlayer<float>::defaultTiming;

    void open() noexcept;
    void close() noexcept;
    void advance() noexcept;
    void skip() noexcept;

    Phase phase() const noexcept;
    bool moving() const noexcept;
    float progress() const noexcept;
    std::uint64_t length() const noexcept;
};
```

A window opening and closing over time: how long each takes, the curve it follows, and how far along it
is. It is a value: a game keeps it in its state and copies it like anything else there. It names no draw
type. Whatever owns the window advances it once per tick; a `Dialog` holds one and advances it itself
([dialog.md](dialog.md#the-box-opening-and-closing)).

- **`duration`** — how long an opening takes, and a closing, as a time.
- **`ticks`** — the same as a count of ticks. When it is not 0 it is the length and `duration` is not read.
- **`easing`** — the curve progress follows, from the engine's `retropp::Easing` set, as a `Tween`'s segment.
- **`timing`** — what resolves `duration` to ticks. It is `TweenPlayer<float>::defaultTiming` as it stands
  when the value is built, which `EngineConfig::setActive` sets to the game's cadence; build the animation
  after `setActive` runs, or set `timing` yourself.

### length

`ticks` when it is not 0; otherwise `timing.ticksForDuration(duration)`, rounded to the nearest tick, so a
time under half a tick is 0. At the Game Boy Color cadence 150 ms is 9 ticks and 8 ms is 0. A length of 0
is **instant**: `open()` is open at once and `close()` closed at once, and nothing moves.

### The steps

| Call | From Closed | From Opening | From Open | From Closing |
|---|---|---|---|---|
| `open()` | Opening at progress 0; Open at once when the length is 0 | nothing | nothing | Opening at the progress it is at |
| `close()` | nothing | Closing at the progress it is at | Closing at progress 1; Closed at once when the length is 0 | nothing |
| `advance()` | nothing | a tick on; Open on the tick it reaches the length | nothing | a tick on; Closed on the tick it reaches the length |
| `skip()` | nothing | Open | nothing | Closed |

### What it reports

| Call | Is |
|---|---|
| `phase()` | `Closed`, `Opening`, `Open` or `Closing` |
| `moving()` | opening or closing |
| `progress()` | 0 closed, 1 open; opening, `ease(easing, elapsed / length)`; closing, `ease(easing, 1 − elapsed / length)` |
| `length()` | the ticks an opening takes, and a closing |

A closing is the opening played backward, so turning back partway carries on from the same progress: a
closing started one tick into a four-tick opening is at the same progress and closed one tick later.

## An opening, tick by tick

Four ticks, `Easing::Linear`:

```
open()        Opening   progress 0
advance()     Opening   0.25
advance()     Opening   0.5
advance()     Opening   0.75
advance()     Open      1
close()       Closing   1
advance()     Closing   0.75
open()        Opening   0.75     turned back: carries on from where it is
advance()     Open      1
```

The same four ticks with the default `Easing::InOutQuad` run 0, 0.125, 0.5, 0.875, 1: slow out of each
end, fastest in the middle.

`skip()` finishes the opening or closing in progress: after `open()` and one `advance()`, `skip()` is Open.

## The styles

```cpp
retropp::IntRect            Grow(retropp::IntRect box, const Animation& animation, retropp::Vec2i origin) noexcept;
retropp::IntRect            Grow(retropp::IntRect box, const Animation& animation) noexcept;
std::vector<retropp::Region> Clip(retropp::IntRect box, const Animation& animation, retropp::Vec2i origin);
std::vector<retropp::Region> Clip(retropp::IntRect box, const Animation& animation);
retropp::Transform          Squash(retropp::IntRect box, const Animation& animation,
                                   retropp::Axis axis = retropp::Axis::Vertical) noexcept;
float                       Fade(const Animation& animation) noexcept;
retropp::LayerScroll        Slide(retropp::IntRect box, const Animation& animation, retropp::Vec2i from) noexcept;
retropp::Vec2i              middle(retropp::IntRect box) noexcept;
```

Each is a function from where the animation is to what a box or a layer takes, with `p` its progress. At
progress 1 each is its identity: Grow gives the box, Clip no regions, Squash the identity transform, Fade
1 and Slide no scroll. `box` and every point are in the pixels of the layer the window is drawn on.

- **`Grow`** — the rectangle the box has grown to, from `origin` at progress 0 to `box` at 1: x is
  `origin.x + (box.x − origin.x)·p`, the width `box.width·p`, each rounded to the nearest pixel, and
  likewise down. Without an origin it grows from `middle(box)`. The box is drawn as a real box at every
  step, its border whole around the grown rectangle.
- **`Clip`** — regions that leave only the rectangle `Grow` gives drawn and the rest of the layer
  see-through: the box is drawn whole and shown through the rectangle, with no border at the moving edge.
  None once open; at progress 0 one region keyed `"clip"` with no shape and a `Transparency` effect, the
  whole layer see-through; between them `retropp::stencil` over the grown rectangle,
  `StencilMode::TransparentOutside` at `ScreenSpaceEffectScope::Layer`, keyed `"clip"`.
- **`Squash`** — the layer scaled about the box's middle along `axis` to the progress: a vertical squash
  presses the box to a line across its middle and opens it back out, the border's art squashed with it.
  The scale is never below one pixel of the box on that axis, so the transform always has an inverse; the
  other axis keeps a scale of 1.
- **`Fade`** — the layer's alpha: the progress.
- **`Slide`** — the layer's scroll that shows the box's top-left at `from` at progress 0 and in its place
  at 1: `(box − from)·(1 − p)` on each axis, rounded to the nearest pixel. A layer's scroll moves its
  content by the negative, so the box travels from `from` to where it is.
- **`middle`** — `{box.x + box.width / 2, box.y + box.height / 2}`.

Over a 160×48 box at (0, 96), with `Slide` from (0, 144), just below a 144-pixel screen:

| | progress 0 | progress ½ | progress 1 |
|---|---|---|---|
| `Grow` | 0×0 at (80, 120) | 80×24 at (40, 108) | 160×48 at (0, 96) |
| `Grow` from (0, 96) | 0×0 at (0, 96) | 80×24 at (0, 96) | 160×48 at (0, 96) |
| `Clip` | one shapeless region | the stencil over 80×24 at (40, 108) | none |
| `Squash` | the box 1 pixel tall across y 120 | the top edge at y 108, the bottom at 132 | the identity |
| `Fade` | 0 | 0.5 | 1 |
| `Slide` | scroll (0, −48) | scroll (0, −24) | scroll (0, 0) |

## Style

```cpp
struct Style {
    enum class Kind : std::uint8_t { Pop, Grow, Clip, Squash, Fade, Slide };
    Kind kind = Kind::Grow;
    std::optional<retropp::Vec2i> origin;
    retropp::Axis axis = retropp::Axis::Vertical;
    retropp::Vec2i from{};

    static Style pop();
    static Style grow();   static Style grow(retropp::Vec2i origin);
    static Style clip();   static Style clip(retropp::Vec2i origin);
    static Style squash(retropp::Axis axis = retropp::Axis::Vertical);
    static Style fade();
    static Style slide(retropp::Vec2i from);
};
```

Which style a window is drawn in while its animation moves, and the style's own parameters: `origin` for
Grow and Clip (none is the box's middle), `axis` for Squash, `from` for Slide. **`Style{}`** is grow from the
middle. The named constructors fill the fields their kind reads; aggregate initialization works too.
**Pop** is no style: the box as it is from the opening's second tick to the closing's last.

## Animate

```cpp
struct Animated {
    retropp::IntRect box;
    LayerKit::LayerOptions options;
};

Animated Animate(retropp::IntRect box, const Animation& animation, const Style& style,
                 LayerKit::LayerOptions options = {});
```

`box` and `options` with `style` applied for where `animation` is, composed over what the caller already
set:

| Kind | What changes |
|---|---|
| Pop | nothing |
| Grow | `box` becomes `Grow(box, animation, origin)` |
| Clip | `Clip(…)` is appended after the caller's `regions` |
| Squash | `transform` becomes `Squash(…).then(options.transform)`: the squash first, the caller's transform over it |
| Fade | `alpha` is multiplied by `Fade(animation)` |
| Slide | `Slide(…)` is added to `scroll` on each axis |

While the animation is Open every kind returns its inputs as they are.

## Window with an animation

```cpp
retropp::DrawLayer Window(std::string_view key, std::int32_t z, retropp::IntRect box, const Border& border,
                          const Animation& animation, const Style& style,
                          std::initializer_list<LayerKit::Sprites> children,
                          LayerKit::LayerOptions options = {});
```

| Phase | The layer |
|---|---|
| Closed | a sprite layer keyed `key` at `z` with no sprites |
| Opening at progress 0 (its first tick) | a sprite layer with no sprites, its options the style's at progress 0 |
| Opening, Closing | the box alone, drawn in `style` for where the animation is: `Animate`, then the plain `Window` over the animated box and options |
| Open | the plain `Window`: the box and the children |

The children are left out while the window moves, so its contents appear once it is open and are gone
the moment it starts to close. A game adds the layer every frame whatever the phase.

**The opening's first tick is empty.** The engine eases a layer's scroll, alpha and transform from one
tick to the next by its key, from whatever the layer was before: closed, at the caller's options, or in
another style. On the opening's first tick the layer takes the style's options at progress 0 with nothing
on it, so that easing shows nothing; from the next tick the box is drawn, and the layer eases on from the
style's start. A fade starts invisible, a slide starts at its point, a squash starts as a line, with no
frame of the whole box before them.

```cpp
// In the game's state, built after EngineConfig::setActive:
WindowKit::Animation menu;
menu.duration = std::chrono::milliseconds{200};

// In the tick: menu.open() when the menu is wanted, menu.close() when it is not, and every tick
menu.advance();

// Each frame.
frame.layers.push_back(WindowKit::Window("menu", kMenuZ, kMenuBox, border, state.menu,
                                         WindowKit::Style::grow({kMenuBox.x, kMenuBox.y}), {Items(state)}));
```

**Keys while a box grows.** Every piece of a box carries the box's size in its key
([window.md](window.md#box)). A box drawn at a new size on each tick of a grow is drawn whole at that
size, every piece in place with its cut; no piece eases toward where a piece of another size was.
