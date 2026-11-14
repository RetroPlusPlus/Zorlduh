# How to

Recipes for things a game does with menus. Each is written against the headers in `include/MenuKit/`.

- [A menu cancel does not close](#a-menu-cancel-does-not-close)
- [A disabled item, and enabling it later](#a-disabled-item-and-enabling-it-later)
- [A grid](#a-grid)
- [A long list in pages, or scrolling](#a-long-list-in-pages-or-scrolling)
- [Closing after a choice, or stepping back](#closing-after-a-choice-or-stepping-back)
- [Changing a label while a menu is open](#changing-a-label-while-a-menu-is-open)
- [An icon beside each label](#an-icon-beside-each-label)
- [A window style from a game setting](#a-window-style-from-a-game-setting)
- [Reacting to a move or a refusal](#reacting-to-a-move-or-a-refusal)
- [Two menus in one game](#two-menus-in-one-game)

## A menu cancel does not close

A title menu stays until the player chooses something. `closesOnCancel` is read on the first menu only;
a menu opened over it still closes on cancel, back to the title menu.

```cpp
MenuKit::MenuConfig title{.box = titleBox, .closesOnCancel = false, .items = {...}};
```

The game can still close it with `close()` — after START, say.

## A disabled item, and enabling it later

```cpp
title.items[1] = {.label = "CONTINUE", .value = Choice::Continue, .enabled = false};
```

The cursor lands on a disabled item, and confirm there reports `refused` and chooses nothing. Give the
look a `disabled` font to draw it differently. The config is the game's, so enabling the item later is
`config.title.items[1].enabled = true;` — even while the menu is open.

## A grid

```cpp
MenuKit::MenuConfig colors{.box = {.x = 0, .y = 64, .width = 160, .height = 32}, .columns = 3,
                           .items = {{.label = "RED"}, {.label = "GREEN"}, {.label = "BLUE"},
                                     {.label = "GOLD"}, {.label = "PINK"}}};
```

Items fill a row at a time. Left and right move along a row, up and down along a column; a move into the
short last row at a column it does not have lands on its last item (from BLUE, down goes to PINK).

## A long list in pages, or scrolling

```cpp
MenuKit::MenuConfig areas{.box = warpBox, .rows = 5, .items = ...};                 // pages of five
MenuKit::MenuConfig letters{.box = lettersBox, .rows = 3, .overflow = MenuKit::Overflow::Scroll,
                            .wraps = false, .items = ...};                         // three at a time, scrolling
```

With pages, moving past a page's last row turns to the next, and with `wraps` the last item comes round
to the first page. With scrolling, the rows move a row at a time as the cursor leaves them. Either way the
arrows show while rows lie above or below, in the room's right-hand cells — leave the rightmost labels a
cell short of the room's right edge. Give the menu a [repeat](menu.md#the-repeat) so a held direction
runs down the list.

## Closing after a choice, or stepping back

What a choice does is the game's. Close every menu:

```cpp
if (step.chosen == MenuKit::Value{Choice::Resume}) {
    pause.close();
}
```

`advance` reports `closed` on the tick the last window has closed — the moment to start play, say. Or
apply a setting and step back one menu, as cancel would:

```cpp
if (step.chosen == MenuKit::Value{Choice::TextFast}) {
    settings.ticksPerGlyph = 1;
    pause.back();
}
```

## Changing a label while a menu is open

Change it in the config; the next frame draws it.

```cpp
config.pause.items[2].label = muted ? "SOUND OFF" : "SOUND ON";
```

Labels, `enabled` and `value` may change at any time. The tree's shape — which items open which menus —
stays while the menus over it are open, since each open menu is found by following `from` down the tree.

## An icon beside each label

Build the window from the leaves and the game's own sprites, placing each icon in its item's cell:

```cpp
const MenuKit::Level& level = state.items.levels()[0];
const retropp::IntRect room = WindowKit::inside(config.items.box, look.border);
LayerKit::Sprites icons;
for (std::size_t n = 0; n < config.items.items.size(); ++n) {
    const retropp::IntRect cell = MenuKit::itemBox(config.items, level, look.border, n);
    if (cell.width > 0) {
        icons.push_back(Icon(n, {.x = cell.x + cell.width - 8, .y = cell.y}));   // the game's own sprite
    }
}
frame.layers.push_back(WindowKit::Window("items:0", kMenuZ, config.items.box, look.border, level.animation,
                                         look.style,
                                         {MenuKit::MenuItems("items:0", config.items, level, look),
                                          MenuKit::MenuCursor("items:0", config.items, level, look, 1),
                                          MenuKit::MenuArrows("items:0", config.items, level, look, 1), icons}));
```

The leaves draw nothing until the window is open, so add icons the same way: draw them only while
`level.animation.phase() == WindowKit::Phase::Open`.

## A window style from a game setting

The look is built each frame, so its style can be read from the game's state:

```cpp
const MenuKit::Look look{.border = border, .font = font, .style = styleOf(state.menuStyle)};
```

A style changed while a window is closing takes effect from the next frame.

## Reacting to a move or a refusal

`Step` reports each on the tick it happens; play a sound, shake the window, show a note.

```cpp
if (step.moved) { audio.play(kTick); }
if (step.refused) { audio.play(kBuzz); }
```

## Two menus in one game

A `Menu` holds positions in the tree it was opened on, so each tree has its own:

```cpp
struct GameState {
    MenuKit::Menu title;   // over config.title
    MenuKit::Menu pause;   // over config.pause
};
state.title.advance(config.title, input.menu);
```

Draw each under its own key, and give each its own depths when both could show at once.
