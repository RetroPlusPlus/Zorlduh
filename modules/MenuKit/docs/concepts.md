# Concepts

## A menu is a tree the game declares

A menu is a `MenuConfig`: a window's box, a layout — how many columns, how many rows showing, what
happens past an edge and past the rows showing — and its items. An item that opens another menu holds
that menu's config in its `opens`, so a pause menu with OPTIONS opening TEXT and WINDOWS is one value,
written once with designated initializers, and nested to any depth:

```cpp
const MenuKit::MenuConfig pause{
    .box = pauseBox,
    .items = {{.label = "RESUME", .value = Choice::Resume},
              {.label = "OPTIONS",
               .opens = MenuKit::MenuConfig{.box = optionsBox,
                                            .items = {{.label = "TEXT", .opens = textMenu},
                                                      {.label = "WINDOWS", .opens = stylesMenu}}}}},
};
```

The config is the game's. It may change any label, `enabled` or `value` at any time, and it can live
wherever the game keeps what never changes — next to the world, built once from the loaded assets.

## The state is positions in the tree

A `Menu` is what the player has open: a stack of `Level`s, one per open menu, the first at the bottom.
A level holds where its cursor is, the first row its menu shows, its window's animation, and which item
of the menu beneath opened it. It holds no copy of any config: the menu a level shows is found by
following `from` down the tree from the first menu (`configOf`).

So the config and the state are handed over together — to `Menu::advance` each tick, and to
`MenuWindows` each frame — and a `Menu` is only meaningful against the config it was opened on. A game
with two trees, a title menu and a pause menu, holds one `Menu` for each. While a menu is open, its path
through the tree stays: the items that open the menus showing keep opening them. A path the tree no
longer holds throws `std::out_of_range`.

## Each open menu is a window on its own layer

A menu opened over another is drawn over it, in a window of its own. `MenuWindows` returns one sprite
layer per open menu, keyed `"<key>:<level>"`, at the depth given for the first and one higher for each
menu over it. A frame's layers each need their own key and depth, and a window's opening and closing is
eased by the engine per layer, so one layer per window is what lets two windows move at once — the one
on top closing while the one beneath stays open.

## The grid, and the rows showing

Items are laid out `columns` across, filled a row at a time: item n is at column `n % columns` of row
`n / columns`. Up and down move a row, left and right a column. Past an edge the cursor comes round to
the other end of that column or row when `wraps` is true, and stays where it is when it is false. A
move into a last row too short for the cursor's column lands on its last item.

A menu shows `rows` rows at once, or all of them when `rows` is 0. When it has more, `overflow` says
what the rows showing are:

- **`Pages`** — the rows showing are the page the cursor's row is on: moving past a page's last row
  turns to the next.
- **`Scroll`** — the rows showing follow the cursor one row at a time, moving only as far as it takes
  them.

`Level::top` is the first row showing. The room inside the window's border is cut into cells — its width
over the columns, its height over the rows showing — and `itemBox` is the cell an item is drawn in.

## The cursor is keyed by its item

The cursor is one sprite keyed `"<key>:cursor:<item>"`. When the cursor moves, it is a sprite with a new
key, and the engine puts a sprite it has not seen before straight where it is told: the cursor lands on
the item it moves to on the tick it moves. A cursor keyed once would be eased from one item to the next.

## One rule for confirm

Confirm finishes whatever is in progress, as it does for a WindowKit message:

- while the window on top opens, confirm opens it at once; the press is spent;
- while it closes, confirm closes it at once;
- once it is open, confirm chooses the item under the cursor — or opens the menu it opens, or is
  refused if the item is disabled.

Nothing moves the cursor before a window is open, and the tick an opening ends on reads the input. After
`close()`, every window closes and confirm closes them all at once.

## Step reports; the game decides

`advance` returns a `Step`: the value chosen, and whether confirm was refused, the cursor moved, a menu
opened over the one on top, cancel started closing one, and the last window closed. Choosing changes
nothing in the menu. What a choice does is the game's: close the menus, step back one, start play, change
a setting. `close()` closes every menu, `back()` the one on top.

## Where each part lives in a game

| Part | Where |
|---|---|
| `MenuConfig` | the game's config, built once — after `EngineConfig::setActive`, so its times resolve at the game's own rate |
| `Menu` | the game's state, beside whatever else the tick advances |
| `readMenuInput` | the input boundary, once per tick |
| `Menu::advance` and what a `Step` means | the tick |
| `Look` and `MenuWindows` | a component that returns the menus' layers each frame |
