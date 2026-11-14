# MenuKit — guide

The guide to the MenuKit module, for anyone building menus in a game on the Polyrhythm engine. Every
page is written against the shipped headers in `include/MenuKit/`.

**New here? Start with [getting-started.md](getting-started.md)**: a complete program, a menu of five
items opening a grid, a list in pages and a scrolling list, with a line saying what each tick reported,
explained part by part. Then read [concepts.md](concepts.md) for the model, [menu.md](menu.md) and
[drawing.md](drawing.md) for every type and rule, and [how-to.md](how-to.md) for recipes.

## What the module is

MenuKit runs menus — a tree of items the game declares once, a cursor the player moves, pages or
scrolling when a menu holds more than it shows, menus opening over menus — from one call per tick that
reports what was chosen, and draws every open menu as a window. Everything it uses is the engine's own
or its neighbors': a menu's window is a WindowKit `Window` with a WindowKit `Animation`, its labels are
TextKit `Text`, its cursor and arrows are the border sheet's arrow slot, and each window is a
`retropp::DrawLayer` built by LayerKit's `SpriteLayer`.

What the module takes care of:

- the stack of open menus, each with its cursor, the rows it shows and its window's animation;
- moving the cursor on a grid of any number of columns, wrapping or stopping at the edges, and moving
  again while a direction is held, after a delay and at a rate in time or ticks;
- pages or scrolling when a menu has more rows than it shows;
- what confirm and cancel do: choose an item, refuse a disabled one, open the menu an item opens, close
  the menu on top — and finish a window's opening or closing at once while it moves;
- drawing every open menu: its window, its labels in up to three fonts, the cursor, and the arrows that
  say more rows lie above or below.

What stays with the game: what its menus hold and what each item is called, where each window is and how
big, the border and the fonts, how long a window takes to open and close and the style it does it in,
which buttons move, confirm and cancel, and what a choice does.

## Index

### Start here

| Page | Covers |
|---|---|
| [getting-started.md](getting-started.md) | A complete program, a menu that cancel does not close opening a grid three across, a list in pages and a scrolling list, built as `MenuKit-example-getting-started`, explained part by part |
| [concepts.md](concepts.md) | A menu is a tree the game declares; the state is positions in it; a menu opens over the one beneath, each in its own window and layer; the grid, the rows showing, pages and scroll; the cursor keyed by its item; the one rule for confirm; `Step` reports and the game decides; where each part lives in a game |
| [how-to.md](how-to.md) | Recipes: a menu cancel does not close, a disabled item and enabling it later, a grid, a long list in pages or scrolling, closing after a choice and stepping back, changing a label while a menu is open, an icon beside each label, a window style from a game setting, reacting to a move or a refusal, two menus in one game |

### Reference

| Page | Covers | Header |
|---|---|---|
| [menu.md](menu.md) | `Value`, `Overflow`, `Repeat`, `MenuConfig`, `Item`, `Level`, `Step`, `Menu`, `configOf`, `itemBox`, `Directions`, `MenuInput`, `MenuActions`, `readMenuInput` | `menu_config.h`, `menu.h`, `input.h` |
| [drawing.md](drawing.md) | `Look`, `MenuItems`, `MenuCursor`, `MenuArrows`, `MenuWindows` | `menu_windows.h` |

## Coverage

| Surface | Status | Page |
|---|---|---|
| A tree of menus declared as one value, nested to any depth | available | concepts.md, menu.md |
| A cursor on a grid of any number of columns, wrapping or stopping at the edges | available | menu.md |
| Moving again while a direction is held, over a time or a count of ticks | available | menu.md |
| More rows than a menu shows: pages, or scrolling a row at a time | available | menu.md, how-to.md |
| Items that cannot be chosen, the cursor landing on them | available | menu.md, how-to.md |
| Confirm and cancel as reported events; the game deciding what a choice does | available | concepts.md, menu.md |
| A menu cancel does not close | available | how-to.md |
| Closing every menu, or stepping back one | available | menu.md, how-to.md |
| Each window opening and closing in any WindowKit style; confirm finishing it | available | drawing.md |
| Labels in a font, the one under the cursor and disabled ones each in a font of their own | available | drawing.md |
| The cursor and the more-rows arrows from the border sheet's arrow | available | drawing.md |
| A game's own sprites in a menu's window, placed by item | available — `itemBox` and a WindowKit `Window` | how-to.md |
| Any number of menus in one game | available — one `Menu` per config | how-to.md |
| An item holding a value changed with left and right (a slider, an on/off) | not provided | — |
| Text entry | not provided | — |
| A controls screen | not provided | — |
| Sound on a move, a choice or a refusal | not provided — the game reads `Step` | how-to.md |
| A cursor that slides from item to item | not provided | — |
