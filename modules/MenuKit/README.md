# MenuKit — menus

`MenuKit` runs menus — a tree of items the game declares once, a cursor the player moves, pages or
scrolling when a menu holds more than it shows, menus opening over menus — from one call per tick that
reports what was chosen, and draws every open menu as a window. It is for games on the
[Polyrhythm](https://github.com/RetroPlusPlus/Polyrhythm) engine, and builds on the LayerKit, TextKit and
WindowKit modules.

```cpp
// What the menu is, declared once.
const MenuKit::MenuConfig pause{
    .box = {.x = 8, .y = 24, .width = 88, .height = 48},
    .items = {{.label = "RESUME", .value = Choice::Resume},
              {.label = "OPTIONS", .opens = optionsMenu},
              {.label = "QUIT", .value = Choice::Quit}},
};

// When it is wanted, and each tick.
menu.open(pause);
const MenuKit::Step step = menu.advance(pause, MenuKit::readMenuInput(input, actions));
if (step.chosen == MenuKit::Value{Choice::Resume}) { menu.close(); }

// Each frame.
for (retropp::DrawLayer& layer : MenuKit::MenuWindows("pause", kMenuZ, pause, menu, look)) {
    frame.layers.push_back(std::move(layer));
}
```

| Piece | What it is | Page |
|---|---|---|
| `MenuConfig`, `Item`, `Value`, `Overflow`, `Repeat` | a menu as the game declares it: its window, its grid, its items | [docs/menu.md](docs/menu.md#the-config) |
| `Menu`, `Level`, `Step` | the menus open, their cursors, rows showing and windows; what a tick did | [docs/menu.md](docs/menu.md#menu) |
| `configOf`, `itemBox` | the config of an open menu; the cell an item is drawn in | [docs/menu.md](docs/menu.md#configof-and-itembox) |
| `MenuInput`, `Directions`, `MenuActions`, `readMenuInput` | what the player asks of a menu, read from the game's own actions | [docs/menu.md](docs/menu.md#input) |
| `Look` | the border, the fonts and the window style a menu is drawn in | [docs/drawing.md](docs/drawing.md#look) |
| `MenuItems`, `MenuCursor`, `MenuArrows` | a menu's labels, its cursor and the arrows that say more rows lie beyond | [docs/drawing.md](docs/drawing.md#the-leaves) |
| `MenuWindows` | every open menu as a window, one layer each | [docs/drawing.md](docs/drawing.md#menuwindows) |

The module is header-only, depends on the engine (`retropp::engine`), LayerKit (`LayerKit::LayerKit`),
TextKit (`TextKit::TextKit`) and WindowKit (`WindowKit::WindowKit`), includes no header from the game it
sits in, and needs C++20. It is meant to leave this repository as a standalone module.

## Using it

```cmake
add_subdirectory(modules/LayerKit)
add_subdirectory(modules/TextKit)
add_subdirectory(modules/WindowKit)
add_subdirectory(modules/MenuKit)
target_link_libraries(my-game PRIVATE MenuKit::MenuKit)
```

The directory's `CMakeLists.txt` defines the INTERFACE target `MenuKit` (alias `MenuKit::MenuKit`) and
expects the four targets above. `MENUKIT_BUILD_EXAMPLES` (on by default when the directory is the
top-level project) builds `MenuKit-example-getting-started`, the program
[docs/getting-started.md](docs/getting-started.md) walks through.

## Documentation

**Start with [docs/getting-started.md](docs/getting-started.md)**, then [docs/concepts.md](docs/concepts.md)
for the model and [docs/how-to.md](docs/how-to.md) for recipes. The full index is
[docs/README.md](docs/README.md).

## Layout

```
modules/MenuKit/
  CMakeLists.txt                    the MenuKit::MenuKit target and the example option
  include/MenuKit/
    input.h                         Directions, MenuInput, ActionName, MenuActions, readMenuInput
    menu_config.h                   Value, Overflow, Repeat, MenuConfig, Item
    menu.h                          Level, Step, Menu, configOf, itemBox
    menu_windows.h                  Look, MenuItems, MenuCursor, MenuArrows, MenuWindows
  examples/getting_started.cpp      the getting-started program
  examples/assets/                  its border sheet, font sheet and palette images
  docs/                             the guide
```

`input.h`, `menu_config.h` and `menu.h` build no draw type, so a game's simulation can hold a
`MenuConfig` and a `Menu`. `menu_windows.h` returns sprites and layers.
