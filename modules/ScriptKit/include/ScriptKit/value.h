// Value — a typed value a script may be handed, and Values — the values handed over for one tick.
//
//     const ScriptKit::Value<bool> Ready;       // declared once, as constants; the declaration is the
//     const ScriptKit::Value<int>  Score;        // value's identity
//
//     ScriptKit::Values values{{Ready, true}, {Score, 3}};
//     values.get(Score);                         // 3
//
// The header includes no engine header and no draw type.
#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace ScriptKit {

// The identity of a value: 1 and up for a declared value, 0 for none.
using ValueId = std::uint32_t;

// The types a value has: bool, int, or one of the game's enums.
template <class T>
concept ValueType = std::same_as<T, bool> || std::same_as<T, int> || std::is_enum_v<T>;

namespace detail {

// The next unused id. Ids are handed out in the order values are constructed.
[[nodiscard]] inline ValueId mintValueId() noexcept {
    static ValueId next = 0;
    return ++next;
}

// A value of any ValueType as the one integer it is stored and evaluated as.
template <ValueType T>
[[nodiscard]] constexpr std::int64_t encode(T value) noexcept {
    return static_cast<std::int64_t>(value);
}

template <ValueType T>
[[nodiscard]] constexpr T decode(std::int64_t raw) noexcept {
    if constexpr (std::same_as<T, bool>) {
        return raw != 0;
    } else {
        return static_cast<T>(raw);
    }
}

struct ValueAccess;

}  // namespace detail

// A value a script may be handed, with its type. The game declares each once, as a constant; two
// declarations are two values whatever they are named. A Value holds only its identity: what it is
// worth on a tick is in the Values a script processor is handed.
//
// A Value<T> is copyable, and a copy is the same value. In an expression it reads the value:
// `Score + 1`, `!Ready`, `Signal == Lamp::Red` build expressions (expression.h), so two values are
// compared for identity through `id()`.
template <class T>
class Value {
    static_assert(ValueType<T>, "ScriptKit::Value<T>: T is bool, int or an enum type");

public:
    Value() noexcept : id_(detail::mintValueId()) {}

    [[nodiscard]] ValueId id() const noexcept { return id_; }

private:
    friend struct detail::ValueAccess;
    explicit Value(ValueId id) noexcept : id_(id) {}

    ValueId id_;
};

namespace detail {

// Makes the Value<T> an id already names, minting nothing.
struct ValueAccess {
    template <class T>
    [[nodiscard]] static Value<T> named(ValueId id) noexcept {
        return Value<T>(id);
    }
};

}  // namespace detail

// The values one script processor is handed for one tick, by id. Bindings::gather builds one from the
// game's state; a test builds one by hand:
//
//     ScriptKit::Values values{{Ready, true}, {Score, 3}};
//     values.set(Score, 4);
class Values {
public:
    // One value and what it is worth, as written in a braced list: {Score, 3}.
    struct Item {
        ValueId id = 0;
        std::int64_t raw = 0;

        template <ValueType T>
        Item(Value<T> value, T worth) noexcept : id(value.id()), raw(detail::encode(worth)) {}
    };

    Values() = default;
    Values(std::initializer_list<Item> items) {
        for (const Item& item : items) {
            setRaw(item.id, item.raw);
        }
    }

    // Sets what `value` is worth, replacing what it was.
    template <ValueType T>
    void set(Value<T> value, T worth) {
        setRaw(value.id(), detail::encode(worth));
    }

    // What `value` is worth. Throws std::out_of_range naming the id when it was never set.
    template <ValueType T>
    [[nodiscard]] T get(Value<T> value) const {
        return detail::decode<T>(raw(value.id()));
    }

    [[nodiscard]] bool has(ValueId id) const noexcept { return find(id) != nullptr; }

    // The untyped forms, for code that holds an id and not a Value<T>: every value is stored as one
    // integer — a bool as 0 or 1, an enum as its underlying value.
    void setRaw(ValueId id, std::int64_t raw) {
        for (Slot& slot : slots_) {
            if (slot.id == id) {
                slot.raw = raw;
                return;
            }
        }
        slots_.push_back(Slot{.id = id, .raw = raw});
    }

    [[nodiscard]] std::int64_t raw(ValueId id) const {
        if (const Slot* slot = find(id)) {
            return slot->raw;
        }
        throw std::out_of_range("ScriptKit::Values: no value with id " + std::to_string(id));
    }

    // Forgets every value and keeps the storage, so filling it again allocates nothing.
    void clear() noexcept { slots_.clear(); }

    [[nodiscard]] std::size_t size() const noexcept { return slots_.size(); }

private:
    struct Slot {
        ValueId id = 0;
        std::int64_t raw = 0;
    };

    [[nodiscard]] const Slot* find(ValueId id) const noexcept {
        for (const Slot& slot : slots_) {
            if (slot.id == id) {
                return &slot;
            }
        }
        return nullptr;
    }

    std::vector<Slot> slots_;   // a handful per processor: a scan is the fastest lookup
};

}  // namespace ScriptKit
