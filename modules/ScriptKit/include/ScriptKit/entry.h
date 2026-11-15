// Entry — one line of a script's body — and the functions that write the lines.
//
//     .body = {
//         say("Hello!"),
//         when(Ready, {
//             walk(Direction::Up, 1),
//             set(LidOpen, true),
//         }).otherwise({
//             set(Tries, Tries + 1),
//         }),
//         loopWhile(Heat < 100, { stoke(), wait(1) }),
//         wait(500ms),
//     }
//
// Each function returns an Entry that describes the line; writing a body runs nothing. A script
// processor turns the body into instructions and steps through them (script_processor.h).
#pragma once

#include <chrono>
#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

#include "ScriptKit/command.h"
#include "ScriptKit/expression.h"
#include "ScriptKit/value.h"

namespace ScriptKit {

struct Script;
class Entry;

// A body: entries, run from the first to the last.
using Body = std::vector<Entry>;

namespace detail {

enum class EntryKind : std::uint8_t {
    Ask, WaitTicks, WaitTime, WaitWhile, Call, Set, Stop, Restart, Loop, LoopWhile, When,
};

// One arm of a `when`: its condition and what runs when the condition holds.
struct Branch {
    NodePtr condition;
    std::vector<Entry> body;
};

// Everything an entry can hold; which fields mean something follows from `kind`.
struct EntryData {
    EntryKind kind = EntryKind::Ask;
    const Command* command = nullptr;      // Ask
    std::vector<Parameter> parameters;     // Ask
    std::uint64_t ticks = 0;               // WaitTicks
    std::chrono::nanoseconds time{};       // WaitTime
    NodePtr expression;                    // WaitWhile and LoopWhile: the condition; Set: the new value
    const Script* script = nullptr;        // Call, Stop, Restart
    ValueId value = 0;                     // Set
    ChangeWriter writer = nullptr;         // Set
    std::vector<Entry> body;               // Loop, LoopWhile; When: what runs when no arm holds
    std::vector<Branch> branches;          // When
};

struct EntryAccess;

}  // namespace detail

// One line of a body. It is made by a command, or by one of the functions below, and is read only by a
// script processor.
class Entry {
private:
    friend struct detail::EntryAccess;
    explicit Entry(detail::EntryData data) : data_(std::move(data)) {}

    detail::EntryData data_;
};

namespace detail {

struct EntryAccess {
    [[nodiscard]] static Entry make(EntryData data) { return Entry(std::move(data)); }
    [[nodiscard]] static const EntryData& data(const Entry& entry) noexcept { return entry.data_; }
};

}  // namespace detail

template <class... Args>
    requires(detail::CommandArgument<Args> && ...)
Entry Command::operator()(Args... arguments) const {
    detail::EntryData data{.kind = detail::EntryKind::Ask, .command = this};
    data.parameters.reserve(sizeof...(Args));
    (data.parameters.push_back(detail::parameterOf(arguments)), ...);
    return detail::EntryAccess::make(std::move(data));
}

// Hold for `time`: wait(500ms), wait(5s) — any std::chrono duration that converts to nanoseconds
// exactly. The script processor turns the time into ticks when it is built, through the timing of the
// EngineConfig it is built with; a time that comes to no ticks is wait(0).
[[nodiscard]] inline Entry wait(std::chrono::nanoseconds time) {
    return detail::EntryAccess::make({.kind = detail::EntryKind::WaitTime, .time = time});
}

// Hold for `ticks` ticks: a whole count, such as wait(1), or what the engine's
// TimingProfile::ticksForDuration returns. The entry after it runs `ticks` ticks after the script
// reached the wait; a count of zero or less moves straight on. A fraction, and a bool, do not build.
template <std::integral Count>
    requires(!std::same_as<Count, bool>)
[[nodiscard]] Entry wait(Count ticks) {
    return detail::EntryAccess::make(
        {.kind = detail::EntryKind::WaitTicks,
         .ticks = ticks > Count{0} ? static_cast<std::uint64_t>(ticks) : std::uint64_t{0}});
}

// Hold while `condition` holds. It is tested once each tick; the entry after it runs on the tick the
// condition is false.
[[nodiscard]] inline Entry waitWhile(Expr<bool> condition) {
    return detail::EntryAccess::make(
        {.kind = detail::EntryKind::WaitWhile, .expression = detail::ExprAccess::node(condition)});
}

// Run `script`'s body, then carry on with the entry after this one. The script is referred to by
// address and outlives every processor that runs this body.
[[nodiscard]] inline Entry call(const Script& script) {
    return detail::EntryAccess::make({.kind = detail::EntryKind::Call, .script = &script});
}
Entry call(Script&&) = delete;

// Ask the game to make `value` worth `to`. The expression is evaluated on the tick the script reaches
// the entry; the game writes the value when it applies the request.
template <ValueType T>
[[nodiscard]] Entry set(Value<T> value, std::type_identity_t<Expr<T>> to) {
    return detail::EntryAccess::make({.kind = detail::EntryKind::Set,
                                      .expression = detail::ExprAccess::node(to),
                                      .value = value.id(),
                                      .writer = &detail::writeChange<T>});
}

// End `script`, wherever it is: running, waiting, or held by another script.
[[nodiscard]] inline Entry stop(const Script& script) {
    return detail::EntryAccess::make({.kind = detail::EntryKind::Stop, .script = &script});
}
Entry stop(Script&&) = delete;

// Put `script` back at its first entry, running. A script that was not running starts.
[[nodiscard]] inline Entry restart(const Script& script) {
    return detail::EntryAccess::make({.kind = detail::EntryKind::Restart, .script = &script});
}
Entry restart(Script&&) = delete;

// Run `body` over and over, until the script is stopped.
[[nodiscard]] inline Entry loop(Body body) {
    return detail::EntryAccess::make({.kind = detail::EntryKind::Loop, .body = std::move(body)});
}

// Run `body` over and over while `condition` holds. The condition is tested before each trip.
[[nodiscard]] inline Entry loopWhile(Expr<bool> condition, Body body) {
    return detail::EntryAccess::make({.kind = detail::EntryKind::LoopWhile,
                                      .expression = detail::ExprAccess::node(condition),
                                      .body = std::move(body)});
}

class When;
[[nodiscard]] inline When when(Expr<bool> condition, Body body);

// A branch being written: when(a, {…}).orWhen(b, {…}).otherwise({…}). The first arm whose condition
// holds runs, and only that one; `otherwise` runs when none does. A When becomes an Entry wherever one
// is needed, so it is written straight into a body.
class When {
public:
    [[nodiscard]] When orWhen(Expr<bool> condition, Body body) && {
        data_.branches.push_back({.condition = detail::ExprAccess::node(condition), .body = std::move(body)});
        return std::move(*this);
    }

    [[nodiscard]] When otherwise(Body body) && {
        data_.body = std::move(body);
        return std::move(*this);
    }

    operator Entry() && { return detail::EntryAccess::make(std::move(data_)); }

private:
    friend When when(Expr<bool> condition, Body body);
    When() = default;

    detail::EntryData data_{.kind = detail::EntryKind::When};
};

// Run `body` when `condition` holds on the tick the script reaches the entry.
inline When when(Expr<bool> condition, Body body) {
    When branch;
    branch.data_.branches.push_back({.condition = detail::ExprAccess::node(condition), .body = std::move(body)});
    return branch;
}

}  // namespace ScriptKit
