# Drawing menus — reference

`menu_windows.h`: how a menu is drawn, the three leaves a menu's window holds, and every open menu as a
layer. This is the one MenuKit header that builds draw types. Every position is in the pixels of the
layer the menus are drawn on — the pixels each `MenuConfig::box` is written in.

- [Look](#look)
- [The arrow](#the-arrow)
- [The leaves](#the-leaves) — `MenuItems`, `MenuCursor`, `MenuArrows`
- [MenuWindows](#menuwindows)
- [Keys and depths](#keys-and-depths)

## Look

```cpp
struct Look {
    WindowKit::Border border;                  // each menu's window, its cursor and its arrows
    TextKit::Font font;                        // an item's label
    std::optional<TextKit::Font> selected;     // the label under the cursor; none draws it in `font`
    std::optional<TextKit::Font> disabled;     // a disabled item's label; none draws it in `font`
    WindowKit::Style style{};                  // how each window opens and closes
};
```

How every menu of a tree is drawn. The fonts are TextKit fonts; a game with one glyph sheet makes the
three by copying its font and giving each copy another palette. `style` is any WindowKit style; the
default is grow from the window's middle. A `Look` is built in the frame, so it can follow the game's
state — a style chosen in an options menu, say.

## The arrow

The arrow is the border sheet's `WindowKit::kArrowSlot` — the slot a WindowKit message box shows its
arrow from — at its slot's size, through the border's palette. It is drawn pointing down, as the sheet
has it, so a sheet's arrow is drawn once and used three ways:

| Use | How it is drawn |
|---|---|
| more rows below | as the sheet has it |
| more rows above | flipped (`flipY`) |
| the cursor | turned three quarters clockwise (`retropp::Rotation::Rot270`): pointing right |

A turn is a texture operation within the sprite's own quad, so a square arrow slot turns whole. A sheet
without the arrow slot draws no cursor and no arrows, and its labels start at their cell's left edge.

## The leaves

Each leaf takes the config of the menu it draws — the level's own, `configOf(root, menu, level)` — and
the level. Each returns nothing unless the level's window is open (`WindowKit::Phase::Open`), and nothing
for a menu with no items.

```cpp
LayerKit::Sprites MenuItems(std::string_view key, const MenuConfig& config, const Level& level,
                            const Look& look, std::int32_t z = 0);
```

The labels of the items among the rows showing, each through `TextKit::Text` keyed `"<key>:item:<n>"`
— so each glyph `"<key>:item:<n>:<character>"` — at `z`. A label is in the look's `disabled` font for a
disabled item, else its `selected` font for the item under the cursor, else its `font`; each falls back
to `font` when the look has none. A label starts the arrow slot's width in from its cell's left edge,
leaving the cursor's place, and is centered down the cell: its top is the cell's top plus half what its
measured height leaves (integer division, never above the cell's top). An empty label draws nothing.

```cpp
LayerKit::Sprites MenuCursor(std::string_view key, const MenuConfig& config, const Level& level,
                             const Look& look, std::int32_t z = 0);
```

One sprite keyed `"<key>:cursor:<item>"`: the arrow pointing right, against the left edge of the
cursor's cell, centered down it (integer division), at `z`. The item is in the key, so on the tick the
cursor moves the sprite is a new one, and the engine puts it straight where it is told.

```cpp
LayerKit::Sprites MenuArrows(std::string_view key, const MenuConfig& config, const Level& level,
                             const Look& look, std::int32_t z = 0);
```

The arrow pointing up, keyed `"<key>:up"`, in the top-right corner of the room the border leaves while
rows lie above the ones showing (`top > 0`); the arrow pointing down, keyed `"<key>:down"`, in the room's
bottom-right corner while rows lie below. At `z`. A menu whose rows all show has none. The arrows sit
inside the room, so a menu that shows them leaves its rightmost labels a cell short of the room's right
edge.

Worked: the getting-started program's first menu, `{8, 24, 80, 56}` in an 8-pixel border with five
items, has cells 64×8 from `(16, 32)`. PLAY's first glyph is at `(24, 32)`, the cursor on PLAY at
`(16, 32)`, and the cursor on LETTERS at `(16, 64)`.

## MenuWindows

```cpp
LayerKit::Layers MenuWindows(std::string_view key, std::int32_t z, const MenuConfig& root, const Menu& menu,
                             const Look& look, LayerKit::LayerOptions options = {});
```

Every open menu, the first at the bottom: menu i as

```cpp
WindowKit::Window("<key>:<i>", z + i, config.box, look.border, level.animation, look.style,
                  {MenuItems(k, config, level, look), MenuCursor(k, config, level, look, 1),
                   MenuArrows(k, config, level, look, 1)},
                  options)
```

with `k` the layer's key `"<key>:<i>"` and `config` the level's own. So each window is WindowKit's: while
it opens or closes, the box alone in the look's style, and nothing on an opening's first tick; once
open, the box beneath the labels, and the cursor and the arrows above the labels. A closed window
beneath one still closing is an empty layer. `options` goes onto every layer. No layers while no menu
shows. Throws `std::out_of_range` when `root` no longer holds the menus open.

A game that wants more in a menu's window than labels builds the window itself — see
[an icon beside each label](how-to.md#an-icon-beside-each-label).

## Keys and depths

| What | Key | Depth |
|---|---|---|
| menu i's layer | `"<key>:<i>"` | `z + i` |
| its box | `"<key>:<i>:box:…"` (WindowKit's) | the lowest in the layer |
| item n's label | `"<key>:<i>:item:<n>:<character>"`, shadows `…:shadow` | 0, shadows −1 |
| the cursor | `"<key>:<i>:cursor:<item>"` | 1 |
| the arrows | `"<key>:<i>:up"`, `"<key>:<i>:down"` | 1 |

Every sprite carries its menu's level, so two open menus share no sprite key. `MenuWindows` is called
once per key per frame, as every LayerKit layer is, and a game leaves the depths from `z` up free for as
many menus as its deepest path through the tree opens.
