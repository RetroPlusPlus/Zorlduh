# Getting started

This page walks through `examples/getting_started.cpp`, a complete program built as
`MenuKit-example-getting-started`. It opens one menu that cancel does not close — PLAY; LOAD, which
cannot be chosen; COLORS, opening six colors three across; NUMBERS, opening ONE to TWELVE four at a time
in pages; LETTERS, opening ALPHA to HOTEL three at a time, scrolling and stopping at either end — and a
line across the top saying what the last tick reported.

The arrow keys, WASD or a pad's directions move the cursor; Z, Space, Return or a pad's A button
confirms; X, Backspace or a pad's B button cancels. Hold a direction and the cursor moves again after
400 ms, then every 100 ms. Each window opens and closes over 200 ms, growing from its middle; confirm
while one moves and it finishes at once.

Build it with the project: `MENUKIT_BUILD_EXAMPLES` is on when `modules/MenuKit` is the top-level
project, and this repository turns it on for its own build.

## 1. The art

The program reads its art from `examples/assets/`, the module's own:

| File | What it is |
|---|---|
| `border.png` | a border sheet of 8×8 cells: the nine pieces of a box in `WindowKit::Piece` order, then the arrow |
| `font.png` | a glyph sheet of 8×8 cells, one per character of `kGlyphs` |
| `palettes/window.png` | the border's colors |
| `palettes/text.png` | a label's colors |
| `palettes/text_selected.png` | the colors of the label under the cursor |
| `palettes/text_dim.png` | the colors of a label that cannot be chosen |

The arrow in slot 9 points down. MenuKit draws it as it is for the arrow that says more rows lie below,
flipped for the one above, and turned a quarter to point right as the cursor, so one cell serves all
three.

```cpp
const WindowKit::Border border{
    .sheet = renderer.loadAtlasFromMemory(readAsset("border.png"), retropp::AssetDimensions{.width = 8, .height = 8},
                                          retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
                                          /*count=*/static_cast<int>(WindowKit::kArrowSlot + 1),
                                          retropp::TransparentIndices::of({0})),
    .palette = loadPalette(renderer, "palettes/window.png"),
};
```

## 2. The look

A `MenuKit::Look` is how every menu is drawn: the border, the font for a label, and optionally a font
for the label under the cursor and one for a label that cannot be chosen. The three fonts here share one
glyph sheet and differ only in their palette.

```cpp
TextKit::Font selected = font;
selected.palette = loadPalette(renderer, "palettes/text_selected.png");
TextKit::Font dim = font;
dim.palette = loadPalette(renderer, "palettes/text_dim.png");
const MenuKit::Look look{.border = border, .font = font, .selected = selected, .disabled = dim};
```

`Look::style` is left as it is, `WindowKit::Style{}`: grow from the window's middle.

## 3. The menus

A menu is a `MenuKit::MenuConfig`: its window's box, its layout, and its items. An `Item` is a label, a
value the game names it by, whether it can be chosen, and the menu it opens if it opens one. The
program makes its menus through one helper, which gives every item a value — its place in `labels` — and
every menu the same window animation and repeat:

```cpp
const auto menu = [&](retropp::IntRect box, std::initializer_list<std::string_view> names) {
    MenuKit::MenuConfig config{
        .box = box,
        .repeat = {.delay = std::chrono::milliseconds{400}, .every = std::chrono::milliseconds{100}},
    };
    config.animation.duration = std::chrono::milliseconds{200};
    for (const std::string_view name : names) {
        config.items.push_back(MenuKit::Item{.label = std::string(name), .value = labels.size()});
        labels.emplace_back(name);
    }
    return config;
};
```

A time resolves to ticks through the timing it is built with, which `EngineConfig::setActive` sets to
the program's own, so the configs are made after `setActive`: 200 ms at 60 Hz is 12 ticks.

The three menus the first one opens each show something else:

```cpp
MenuKit::MenuConfig colors = menu({.x = 0, .y = 64, .width = 160, .height = 32},
                                  {"RED", "GREEN", "BLUE", "GOLD", "PINK", "GRAY"});
colors.columns = 3;                                // a grid: three across, two rows
MenuKit::MenuConfig numbers = menu({.x = 40, .y = 48, .width = 80, .height = 48}, {"ONE", ..., "TWELVE"});
numbers.rows = 4;                                  // four rows at a time, in pages (the default)
MenuKit::MenuConfig letters = menu({.x = 56, .y = 56, .width = 88, .height = 40}, {"ALPHA", ..., "HOTEL"});
letters.rows = 3;
letters.overflow = MenuKit::Overflow::Scroll;      // the rows follow the cursor one at a time
letters.wraps = false;                             // and stop at either end
```

The first menu holds them. An item with `opens` opens its menu on confirm; LOAD has `enabled` false, so
the cursor lands on it and confirm is refused; and `closesOnCancel` false keeps cancel from closing the
first menu:

```cpp
MenuKit::MenuConfig root = menu({.x = 8, .y = 24, .width = 80, .height = 56}, {"PLAY", "LOAD"});
root.closesOnCancel = false;
root.items[1].enabled = false;
root.items.push_back(MenuKit::Item{.label = "COLORS", .opens = std::move(colors)});
root.items.push_back(MenuKit::Item{.label = "NUMBERS", .opens = std::move(numbers)});
root.items.push_back(MenuKit::Item{.label = "LETTERS", .opens = std::move(letters)});
```

Each window's box is in the pixels of the layer it is drawn on — here the screen. With an 8-pixel border
the first menu's room is 64×40: five rows of 8, one item each.

## 4. The state, and the tick

A `MenuKit::Menu` is the menus a player has open: one level per open menu, each with its cursor, the
first row it shows and its window's animation. It is opened once, and advanced once per tick with the
config it was opened on and what the player asked:

```cpp
MenuKit::Menu open;
open.open(root);
std::string status;
loop.simTick([&](const retropp::InputState& input) {
    const MenuKit::Step step = open.advance(
        root, MenuKit::readMenuInput(input, {.up = Action::Up, .down = Action::Down, .left = Action::Left,
                                             .right = Action::Right, .confirm = Action::Confirm,
                                             .cancel = Action::Cancel}));
    if (step.chosen) {
        status = "CHOSE " + labels[step.chosen->id];
    } else if (step.refused) {
        status = "REFUSED";
    } else if (step.cancelled) {
        status = "CANCELLED";
    }
});
```

`readMenuInput` reads the directions held and pressed, and whether confirm and cancel were pressed,
through the program's own actions. `advance` moves every window's animation on, reads the input against
the menu on top, and reports what happened in a `Step`. Choosing changes nothing in the menu: what a
choice does is the program's, here a line of text. Confirm on COLORS opens it over the first menu and
reports `opened`; cancel there closes it and reports `cancelled`, and the first menu takes input again
once that window has closed.

## 5. The frame

`MenuWindows` draws every open menu, one sprite layer each, the first at the depth given and each one
over it a depth higher:

```cpp
frame.layers = {LayerKit::SpriteLayer("status", 0, {TextKit::Text("status", status, {.x = 8, .y = 8}, font)})};
for (retropp::DrawLayer& layer : MenuKit::MenuWindows("menu", 1, root, open, look)) {
    frame.layers.push_back(std::move(layer));
}
```

Each layer is a WindowKit window: while it opens or closes, the box alone in the look's style; once open,
the box with the labels, the cursor and the arrows. The layers are keyed `"menu:0"`, `"menu:1"`, so the
depths from 1 up are the menus': as many as the deepest path through the tree opens — two here.

## What to read next

- [concepts.md](concepts.md) — the model: the tree, the state, the grid, the confirm rule.
- [menu.md](menu.md) — every rule of `Menu::advance`, followed tick by tick.
- [drawing.md](drawing.md) — where each label, the cursor and the arrows go, and their keys.
- [how-to.md](how-to.md) — recipes.
