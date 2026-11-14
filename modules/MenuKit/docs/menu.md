# Menus — reference

`menu_config.h`, `menu.h` and `input.h`: what a menu is, the menus a player has open, and what the
player asks of them. None of the three builds a draw type, so a game's simulation holds all of it.

- [The config](#the-config) — `Value`, `Overflow`, `Repeat`, `MenuConfig`, `Item`
- [Menu](#menu) — `Level`, `Step`, `Menu`
- [The rules of a tick](#the-rules-of-a-tick)
- [A menu followed tick by tick](#a-menu-followed-tick-by-tick)
- [configOf and itemBox](#configof-and-itembox)
- [Input](#input) — `Directions`, `MenuInput`, `ActionName`, `MenuActions`, `readMenuInput`

## The config

```cpp
struct Value {
    std::uint32_t id = 0;
    template <typename E> requires std::is_enum_v<E> || std::is_integral_v<E>
    constexpr Value(E value) noexcept;   // id = static_cast<std::uint32_t>(value)
    bool operator==(const Value&) const = default;
};
```

One of the game's own enumerators, or an integer, naming an item. It is reported as `Step::chosen` when
the item is chosen; `step.chosen == MenuKit::Value{Choice::Resume}` compares an optional value with one.

```cpp
enum class Overflow : std::uint8_t { Pages, Scroll };
```

What a menu with more rows than it shows does: the rows showing are a page (`Pages`), or follow the
cursor a row at a time (`Scroll`). See [the rows showing](#the-rows-showing).

```cpp
struct Repeat {
    std::chrono::nanoseconds delay{};
    std::chrono::nanoseconds every{};
    std::uint32_t delayTicks = 0;   // not 0: `delay` is not read
    std::uint32_t everyTicks = 0;   // not 0: `every` is not read
    retropp::TimingProfile timing = retropp::TweenPlayer<float>::defaultTiming;
};
```

Moving again while a direction stays held: after `delay`, then once per `every`. Each length is a count
of ticks when its `…Ticks` field is not 0, else its time through `timing`, to the nearest tick. `timing`
is whatever `TweenPlayer<float>::defaultTiming` is when the value is built; `EngineConfig::setActive`
sets it to the game's own, so build the repeat after `setActive`. Its `tickPeriodNs` may be any period:
a named preset or `static_cast<retropp::TickPeriodNs>(nanoseconds)`. With no `every`, nothing repeats;
with no delay, the first repeat comes after one `every`.

```cpp
struct MenuConfig {
    retropp::IntRect box{};
    int columns = 1;
    int rows = 0;
    Overflow overflow = Overflow::Pages;
    bool wraps = true;
    bool closesOnCancel = true;
    Repeat repeat{};
    WindowKit::Animation animation{};
    std::vector<Item> items;
};
```

| Field | Meaning |
|---|---|
| `box` | the menu's window, in the pixels of the layer it is drawn on |
| `columns` | items across; below 1 is read as 1 |
| `rows` | rows showing at once; 0, or more than the items take, shows every row |
| `overflow` | pages or scrolling, when the items take more rows than show |
| `wraps` | past an edge the cursor comes round to the other end; false, it stays |
| `closesOnCancel` | read on the first menu only: false is a first menu cancel does not close. A menu over another always closes on cancel |
| `repeat` | moving again while a direction is held; nothing repeats by default |
| `animation` | how the window opens and closes; a level opened on this menu starts with a copy. Instant unless given a length |
| `items` | the items, in grid order |

```cpp
struct Item {
    std::string label;
    std::optional<Value> value;
    bool enabled = true;
    std::optional<MenuConfig> opens;
};
```

| Field | Meaning |
|---|---|
| `label` | drawn in the look's font; an empty label draws nothing |
| `value` | reported as `Step::chosen` when the item is chosen |
| `enabled` | false: the cursor lands on the item and confirm is refused |
| `opens` | the menu confirm opens over this one |

On confirm, an item whose `opens` has items opens it, and its value is not reported. An item whose
`opens` is empty, and one with neither `opens` nor `value`, do nothing.

## Menu

```cpp
struct Level {
    std::size_t from = 0;             // the item of the menu beneath that opened it; not read on the first
    std::size_t cursor = 0;           // the item the cursor is on
    std::size_t top = 0;              // the first row showing
    WindowKit::Animation animation;   // its window's
};

struct Step {
    std::optional<Value> chosen;      // confirm on an enabled item that opens nothing: its value
    bool refused = false;             // confirm on a disabled item
    bool moved = false;               // the cursor went to another item
    bool opened = false;              // confirm opened a menu over the one on top
    bool cancelled = false;           // cancel started closing the menu on top
    bool closed = false;              // the last window has closed: no menu shows
};

class Menu {
public:
    void open(const MenuConfig& config);
    void close() noexcept;
    void back() noexcept;
    Step advance(const MenuConfig& config, const MenuInput& input);
    bool showing() const noexcept;
    std::span<const Level> levels() const noexcept;
};
```

A `Menu` is a value: copyable, and held in a game's state. `levels()` is the open menus, the first at
the bottom; empty while nothing shows.

| Call | What it does |
|---|---|
| `open(config)` | Opens the first menu: one level, cursor 0, top 0, its window opening with a copy of `config.animation`. Does nothing while a menu is showing, or when `config` has no items |
| `close()` | Every window starts to close, and no input is read from then on; `advance` reports `closed` on the tick the last has closed. Does nothing while no menu shows |
| `back()` | The window on top starts to close, as cancel would, whatever `closesOnCancel` says — on the first menu, it closes the menu. Does nothing while no menu shows, or after `close()` |
| `advance(config, input)` | One tick, by [the rules below](#the-rules-of-a-tick). `config` is the one the menu was opened on |
| `showing()` | true from `open` until the `advance` that reports `closed` |

`close()` and `back()` start a closing; with an instant animation the window is closed at once and gone
after the next `advance`.

## The rules of a tick

`advance(config, input)` returns an empty `Step` and changes nothing while no menu shows. Otherwise:

1. Every level's animation advances one tick.
2. After `close()`: `confirm` closes every window at once. Nothing else is read.
3. Otherwise, by the phase of the window on top:
   - **opening** — `confirm` opens it at once; the press is spent. Nothing else is read.
   - **closing** — `confirm` closes it at once.
   - **open** — the cursor is brought inside the menu's items (the last item when it is past them) and
     the rows showing settled. Then exactly one of these, in this order:
     - **cancel** — on a menu over another, or on the first menu when its `closesOnCancel` is true, the
       window starts to close and `cancelled` is reported. On a first menu that stays, nothing.
     - **confirm** — on a disabled item, `refused`. On an item whose `opens` has items, a level is pushed
       — `from` the cursor, cursor 0, top 0, its window opening with a copy of that menu's animation — and
       `opened` is reported. On an item with a value and no menu, `chosen` is the value.
     - **a direction** — the one pressed this tick (when several, the first of up, down, left, right), or
       [the repeat](#the-repeat). The cursor [moves](#moving); `moved` when it lands on another item.
4. Every closed window on top is taken off, top down. When none is left, `closed` is reported.

The tick an opening ends on, step 3 reads the window as open. Choosing changes nothing in the menu.
`configOf` throws `std::out_of_range` from `advance` when the config no longer holds the menus open.

### Moving

Up and down move a row, left and right a column, keeping the other. Past the first or last row, or the
first or last column of the cursor's row: with `wraps`, to the other end of that column or row; without,
the cursor stays. A move into the last row at a column it does not have lands on the last item. A move
that leaves the cursor where it was reports nothing.

### The rows showing

With `V` the rows showing and `r` the cursor's row, after every move and on every open tick:

- `Scroll`: `top` becomes `r` when `r < top`, and `r − V + 1` when `r ≥ top + V`; it is never past the
  last `V` rows.
- `Pages`: `top` is `(r / V) · V`.

### The repeat

A direction pressed moves at once and becomes the held direction, its count at 0. On each later tick
with nothing pressed, while that direction is still held and the top menu's repeat has an `every`, the
count rises by one, and the cursor moves again when the count reaches the delay — or one `every` when
there is no delay — and every `every` ticks after. Releasing it, pressing another direction, or another
menu coming on top starts the count over. A tick that reads cancel or confirm does not count.

## A menu followed tick by tick

```cpp
MenuKit::MenuConfig root{.box = {.x = 8, .y = 24, .width = 80, .height = 48}, .rows = 3};
root.animation.ticks = 4;
root.items = {{.label = "ONE", .value = 1}, {.label = "TWO", .value = 2}, {.label = "THREE", .value = 3},
              {.label = "FOUR", .value = 4}, {.label = "MORE", .opens = moreMenu}};   // moreMenu: instant
MenuKit::Menu menu;
menu.open(root);
```

Five items in one column take five rows; three show, in pages of three.

| Tick | Input | Afterward | Reported |
|---|---|---|---|
| — | `open(root)` | one level, cursor 0, top 0, opening | — |
| 1 | nothing | opening: 1 tick of 4 | nothing |
| 2 | confirm | open at once | nothing: the press is spent |
| 3 | down | cursor 1 | `moved` |
| 4 | up | cursor 0 | `moved` |
| 5 | up | wraps to cursor 4 (MORE), on row 4: top 3, the second page | `moved` |
| 6 | confirm | a second level, `from` 4, open at once | `opened` |
| 7 | cancel | the second level closed at once, and taken off | `cancelled` |
| 8 | `close()`, then nothing | the first window closing: 1 tick of 4 | nothing |
| 9, 10 | nothing | closing: 2, then 3 ticks of 4 | nothing |
| 11 | nothing | closed, taken off; `showing()` false | `closed` |

## configOf and itemBox

```cpp
const MenuConfig& configOf(const MenuConfig& root, const Menu& menu, std::size_t level);
```

The config of the menu open at `level`: `root` for level 0, and for each level above, the `opens` of
item `from` of the menu beneath. Throws `std::out_of_range` for a level the menu does not have, or a path
the tree no longer holds.

```cpp
retropp::IntRect itemBox(const MenuConfig& config, const Level& level, const WindowKit::Border& border,
                         std::size_t item) noexcept;
```

The cell `item` is drawn in: the room `WindowKit::inside(config.box, border)` leaves, cut into
`columns` across and the rows showing down (integer division); column c of row r is at the room's
top-left plus `(c · width, (r − top) · height)`. A rectangle of no size for an item whose row is not
showing, an item the menu does not have, or a menu with no items.

Worked: a box `{8, 24, 80, 56}` in an 8-pixel border leaves the room `{16, 32, 64, 40}`. With five items
in one column, all showing, each cell is 64×8, and item 2 is `{16, 48, 64, 8}`. A box `{0, 64, 160, 32}`
three across with six items leaves `{8, 72, 144, 16}`: cells of 48×8, and item 4 — column 1 of row 1 — is
`{56, 80, 48, 8}`.

## Input

```cpp
struct Directions { bool up = false, down = false, left = false, right = false; };

struct MenuInput {
    Directions held{};      // the directions held this tick
    Directions pressed{};   // the directions pressed since the last tick
    bool confirm = false;   // confirm pressed this tick
    bool cancel = false;    // cancel pressed this tick
};

struct ActionName { template <retropp::ActionLike A> constexpr ActionName(A action); };

struct MenuActions { ActionName up, down, left, right, confirm, cancel; };

MenuInput readMenuInput(const retropp::InputState& input, const MenuActions& actions);
```

`readMenuInput` reads player slot 0: `held` through `isHeld`, `pressed`, `confirm` and `cancel` through
`justPressed`, each action written as the game's own enumerator:

```cpp
const MenuKit::MenuInput menu = MenuKit::readMenuInput(
    input, {.up = Action::Up, .down = Action::Down, .left = Action::Left, .right = Action::Right,
            .confirm = Action::Accept, .cancel = Action::Back});
```

`MenuInput` is a plain value, so a game reads it once at its input boundary and its tests build one by
hand.
