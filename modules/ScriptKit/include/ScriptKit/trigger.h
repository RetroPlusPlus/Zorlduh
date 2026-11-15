// Trigger — what sets a script off, named by the game's own enum.
//
//     enum class On : std::uint8_t { Interact, Touch, Near, Becomes };
//
//     constexpr ScriptKit::Trigger Interact{On::Interact};
//     constexpr ScriptKit::Trigger Near(int distance) { return {On::Near, distance}; }
//     ScriptKit::Trigger Becomes(ScriptKit::Value<bool> value) { return {On::Becomes, value}; }
//
// ScriptKit stores and compares triggers; the game decides when one has happened and tells a script
// processor with ScriptProcessor::start(trigger, values).
#pragma once

#include <cstdint>
#include <type_traits>

#include "ScriptKit/value.h"

namespace ScriptKit {

// What sets a script off: a kind from the game's enum and, when the kind takes one, a number or a
// Value<bool>. Two triggers are the same when all three match, so Near(48) and Near(32) are two
// triggers.
//
// A default Trigger is no trigger: `kind` is 0, and a script whose `on` is left at it is started only
// by the game, by name. A trigger made from an enum holds the enumerator's value plus one, so the
// enum's first enumerator is a trigger like any other.
struct Trigger {
    std::uint32_t kind = 0;   // 0 for none; else the enumerator's value plus one
    int number = 0;           // the kind's number, when it takes one: a distance, a count
    ValueId value = 0;        // the kind's value, when it takes one: the Value<bool> that is watched

    constexpr Trigger() noexcept = default;

    template <class Kind>
        requires std::is_enum_v<Kind>
    constexpr Trigger(Kind kindOf) noexcept : kind(static_cast<std::uint32_t>(kindOf) + 1) {}

    template <class Kind>
        requires std::is_enum_v<Kind>
    constexpr Trigger(Kind kindOf, int numberOf) noexcept
        : kind(static_cast<std::uint32_t>(kindOf) + 1), number(numberOf) {}

    template <class Kind>
        requires std::is_enum_v<Kind>
    Trigger(Kind kindOf, Value<bool> valueOf) noexcept
        : kind(static_cast<std::uint32_t>(kindOf) + 1), value(valueOf.id()) {}

    // Whether this is a trigger at all.
    [[nodiscard]] constexpr bool set() const noexcept { return kind != 0; }

    [[nodiscard]] constexpr bool operator==(const Trigger&) const noexcept = default;
};

}  // namespace ScriptKit
