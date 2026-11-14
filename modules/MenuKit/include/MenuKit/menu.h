// Menu — the menus a player has open: one over another, each with its cursor, the rows it shows and
// its window's animation, run from one call per tick that reports what happened.
//
//     MenuKit::Menu pause;                                       // in the game's state
//     pause.open(config.pause);                                  // when the menu is wanted
//     const MenuKit::Step step = pause.advance(config.pause, input.menu);   // once per tick
//     if (step.chosen == MenuKit::Value{Choice::Resume}) { pause.close(); }
//
// A Menu holds positions in the config it was opened on, so the same config is handed to every
// advance, and to MenuWindows each frame (menu_windows.h). One Menu per config: a game with a title
// menu and a pause menu holds two. The header names no draw type and returns none, so a game's
// simulation can hold a Menu.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include "retropp/geometry.h"

#include "WindowKit/animation.h"
#include "WindowKit/border.h"

#include "MenuKit/input.h"
#include "MenuKit/menu_config.h"

namespace MenuKit {

// One open menu.
struct Level {
    std::size_t from = 0;             // the item of the menu beneath that opened it; not read on the first
    std::size_t cursor = 0;           // the item the cursor is on
    std::size_t top = 0;              // the first row showing
    WindowKit::Animation animation;   // its window's: a copy of its config's, advanced by the Menu
};

// What one tick did. Each field is set on the tick it happens.
struct Step {
    std::optional<Value> chosen;      // confirm on an enabled item that opens nothing: its value
    bool refused = false;             // confirm on a disabled item
    bool moved = false;               // the cursor went to another item
    bool opened = false;              // confirm opened a menu over the one on top
    bool cancelled = false;           // cancel started closing the menu on top
    bool closed = false;              // the last window has closed: nothing is showing
};

namespace detail {

[[nodiscard]] inline std::size_t columnsOf(const MenuConfig& config) noexcept {
    return static_cast<std::size_t>(std::max(1, config.columns));
}

// The rows the items take: the item count over the columns, rounded up.
[[nodiscard]] inline std::size_t rowCount(const MenuConfig& config) noexcept {
    const std::size_t columns = columnsOf(config);
    return (config.items.size() + columns - 1) / columns;
}

// The rows showing at once: `rows`, or every row when `rows` is 0 or more than the items take.
[[nodiscard]] inline std::size_t rowsShowing(const MenuConfig& config) noexcept {
    const std::size_t all = rowCount(config);
    return config.rows <= 0 || static_cast<std::size_t>(config.rows) > all ? all : static_cast<std::size_t>(config.rows);
}

// The cursor, brought inside the items: the last item when it is past the end, 0 for a menu with none.
[[nodiscard]] inline std::size_t cursorIn(const MenuConfig& config, const Level& level) noexcept {
    return config.items.empty() ? 0 : std::min(level.cursor, config.items.size() - 1);
}

enum class Direction : std::uint8_t { Up, Down, Left, Right };

[[nodiscard]] inline bool holds(const Directions& directions, Direction direction) noexcept {
    switch (direction) {
        case Direction::Up:
            return directions.up;
        case Direction::Down:
            return directions.down;
        case Direction::Left:
            return directions.left;
        case Direction::Right:
            return directions.right;
    }
    return false;
}

// The first of up, down, left and right that `directions` holds.
[[nodiscard]] inline std::optional<Direction> firstOf(const Directions& directions) noexcept {
    for (const Direction direction : {Direction::Up, Direction::Down, Direction::Left, Direction::Right}) {
        if (holds(directions, direction)) {
            return direction;
        }
    }
    return std::nullopt;
}

// Where the cursor goes from `cursor` in `direction`: a row up or down, or a column left or right,
// keeping the other. Past an edge it comes round to the other end of that column or row with `wraps`,
// and stays without. A move into the last row at a column it does not have lands on its last item.
[[nodiscard]] inline std::size_t moved(const MenuConfig& config, std::size_t cursor, Direction direction) noexcept {
    const std::size_t count = config.items.size();
    if (count == 0) {
        return 0;
    }
    const std::size_t columns = columnsOf(config);
    const std::size_t rows = rowCount(config);
    std::size_t row = cursor / columns;
    std::size_t column = cursor % columns;
    const std::size_t lastColumn = std::min(columns, count - row * columns) - 1;
    switch (direction) {
        case Direction::Up:
            if (row > 0) {
                --row;
            } else if (config.wraps) {
                row = rows - 1;
            }
            break;
        case Direction::Down:
            if (row + 1 < rows) {
                ++row;
            } else if (config.wraps) {
                row = 0;
            }
            break;
        case Direction::Left:
            if (column > 0) {
                --column;
            } else if (config.wraps) {
                column = lastColumn;
            }
            break;
        case Direction::Right:
            if (column < lastColumn) {
                ++column;
            } else if (config.wraps) {
                column = 0;
            }
            break;
    }
    return std::min(row * columns + column, count - 1);
}

// The first row showing once the cursor's row is in view: under Scroll the rows move only as far as
// the cursor takes them, and never past the last; under Pages they are the page the cursor's row is on.
[[nodiscard]] inline std::size_t settled(const MenuConfig& config, std::size_t cursor, std::size_t top) noexcept {
    const std::size_t showing = rowsShowing(config);
    if (showing == 0) {
        return 0;
    }
    const std::size_t row = cursor / columnsOf(config);
    if (config.overflow == Overflow::Pages) {
        return row / showing * showing;
    }
    if (row < top) {
        top = row;
    } else if (row >= top + showing) {
        top = row - showing + 1;
    }
    return std::min(top, rowCount(config) - showing);
}

}  // namespace detail

// The menus open, the first at the bottom: a value in a game's state. Advanced once per tick with the
// config it was opened on; it advances its own windows' animations. Copyable.
class Menu {
public:
    // Opens the first menu: one level, the cursor on item 0 and the first row showing, its window opening
    // with a copy of `config.animation`. Nothing while a menu is showing, and nothing when `config` has
    // no items.
    void open(const MenuConfig& config) {
        if (showing() || config.items.empty()) {
            return;
        }
        levels_.push_back(Level{.animation = config.animation});
        levels_.back().animation.open();
        closingAll_ = false;
        repeat_.reset();
    }

    // Closes every menu: each window starts to close, and no input is read from then on. `advance`
    // reports `closed` on the tick the last has closed. Nothing while no menu is showing.
    void close() noexcept {
        if (!showing()) {
            return;
        }
        for (Level& level : levels_) {
            level.animation.close();
        }
        closingAll_ = true;
    }

    // Closes the menu on top, as cancel does, whether or not the first menu closes on cancel. Nothing
    // while no menu is showing or once close() has run.
    void back() noexcept {
        if (!showing() || closingAll_) {
            return;
        }
        levels_.back().animation.close();
    }

    // One tick. Every window's animation moves on. After close(), `confirm` closes every window at once.
    // Otherwise, by the window on top: while it opens, `confirm` opens it at once; while it closes,
    // `confirm` closes it at once; once open, exactly one of cancel, confirm or a direction is read, in
    // that order. Then every closed window on top is taken off, and `closed` is reported when none is
    // left. Nothing while no menu is showing. Throws std::out_of_range when `config` no longer holds
    // the menus open (configOf).
    Step advance(const MenuConfig& config, const MenuInput& input) {
        Step step;
        if (!showing()) {
            return step;
        }
        for (Level& level : levels_) {
            level.animation.advance();
        }
        if (closingAll_) {
            if (input.confirm) {
                for (Level& level : levels_) {
                    level.animation.skip();
                }
            }
        } else {
            switch (levels_.back().animation.phase()) {
                case WindowKit::Phase::Opening:
                case WindowKit::Phase::Closing:
                    if (input.confirm) {
                        levels_.back().animation.skip();
                    }
                    break;
                case WindowKit::Phase::Open:
                    readOpen(config, input, step);
                    break;
                case WindowKit::Phase::Closed:
                    break;
            }
        }
        while (!levels_.empty() && levels_.back().animation.phase() == WindowKit::Phase::Closed) {
            levels_.pop_back();
        }
        if (levels_.empty()) {
            step.closed = true;
            closingAll_ = false;
            repeat_.reset();
        }
        return step;
    }

    // Whether a menu is showing: from open until the advance that reports `closed`.
    [[nodiscard]] bool showing() const noexcept { return !levels_.empty(); }

    // The open menus, the first at the bottom; empty while nothing is showing.
    [[nodiscard]] std::span<const Level> levels() const noexcept { return levels_; }

private:
    // A direction held since it was pressed, the ticks since, and how many menus were open then.
    struct Held {
        detail::Direction direction;
        std::uint64_t ticks = 0;
        std::size_t depth = 0;
    };

    void readOpen(const MenuConfig& root, const MenuInput& input, Step& step);

    std::vector<Level> levels_;
    std::optional<Held> repeat_;
    bool closingAll_ = false;
};

// The config of the menu open at `level`: `root` for the first, and for each one above it the menu
// opened by item `from` of the one beneath. Throws std::out_of_range for a level `menu` does not have,
// or a path through the tree that no longer leads to a menu.
[[nodiscard]] inline const MenuConfig& configOf(const MenuConfig& root, const Menu& menu, std::size_t level) {
    const std::span<const Level> levels = menu.levels();
    if (level >= levels.size()) {
        throw std::out_of_range("MenuKit::configOf: no menu is open at that level");
    }
    const MenuConfig* config = &root;
    for (std::size_t n = 1; n <= level; ++n) {
        const std::size_t from = levels[n].from;
        if (from >= config->items.size() || !config->items[from].opens) {
            throw std::out_of_range("MenuKit::configOf: the config no longer holds the menus open");
        }
        config = &*config->items[from].opens;
    }
    return *config;
}

inline void Menu::readOpen(const MenuConfig& root, const MenuInput& input, Step& step) {
    const std::size_t depth = levels_.size();
    const MenuConfig& config = configOf(root, *this, depth - 1);
    Level& top = levels_.back();
    top.cursor = detail::cursorIn(config, top);
    top.top = detail::settled(config, top.cursor, top.top);

    if (input.cancel) {
        repeat_.reset();
        if (depth > 1 || root.closesOnCancel) {
            top.animation.close();
            step.cancelled = true;
        }
        return;
    }

    if (input.confirm) {
        repeat_.reset();
        if (config.items.empty()) {
            return;
        }
        const Item& item = config.items[top.cursor];
        if (!item.enabled) {
            step.refused = true;
        } else if (item.opens) {
            if (!item.opens->items.empty()) {
                Level opened{.from = top.cursor, .animation = item.opens->animation};
                opened.animation.open();
                levels_.push_back(opened);
                step.opened = true;
            }
        } else if (item.value) {
            step.chosen = item.value;
        }
        return;
    }

    std::optional<detail::Direction> direction = detail::firstOf(input.pressed);
    if (direction) {
        repeat_ = Held{.direction = *direction, .depth = depth};
    } else if (repeat_ && repeat_->depth == depth && detail::holds(input.held, repeat_->direction)) {
        const std::uint64_t every = detail::everyLength(config.repeat);
        if (every == 0) {
            return;
        }
        const std::uint64_t delay = detail::delayLength(config.repeat);
        const std::uint64_t first = delay != 0 ? delay : every;
        const std::uint64_t ticks = ++repeat_->ticks;
        if (ticks < first || (ticks - first) % every != 0) {
            return;
        }
        direction = repeat_->direction;
    } else {
        repeat_.reset();
        return;
    }

    const std::size_t from = top.cursor;
    top.cursor = detail::moved(config, from, *direction);
    top.top = detail::settled(config, top.cursor, top.top);
    step.moved = top.cursor != from;
}

// The cell of `item` in the menu `config` with `level` open: the room the border leaves inside the box,
// cut into `columns` across and the rows showing down (integer division); the cell of column c and row
// r is at the room's top-left plus (c · width, (r − top) · height). A rectangle of no size for an item
// whose row is not showing, an item the menu does not have, or a menu with no items.
[[nodiscard]] inline retropp::IntRect itemBox(const MenuConfig& config, const Level& level,
                                              const WindowKit::Border& border, std::size_t item) noexcept {
    if (item >= config.items.size()) {
        return {};
    }
    const std::size_t columns = detail::columnsOf(config);
    const std::size_t showing = detail::rowsShowing(config);
    const std::size_t row = item / columns;
    if (row < level.top || row >= level.top + showing) {
        return {};
    }
    const retropp::IntRect room = WindowKit::inside(config.box, border);
    const int width = room.width / static_cast<int>(columns);
    const int height = room.height / static_cast<int>(showing);
    return retropp::IntRect{
        .x = room.x + static_cast<int>(item % columns) * width,
        .y = room.y + static_cast<int>(row - level.top) * height,
        .width = width,
        .height = height,
    };
}

}  // namespace MenuKit
