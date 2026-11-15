// Command — something a script can ask the game to do — and Request, what comes out of a script
// processor when a script asks.
//
//     enum class Bubble : std::uint8_t { Plain, Shout };
//
//     const ScriptKit::Command Say{"say"};
//     ScriptKit::Entry say(std::string_view text, Bubble bubble = Bubble::Plain) { return Say(text, bubble); }
//
//     // In a body:  say("Hello!", Bubble::Shout)
//     // In the game, when the request comes out:
//     //     arguments.text(0)           "Hello!"
//     //     arguments.word<Bubble>(1)   Bubble::Shout
//
// A command has a name and, each time a body uses it, arguments. It does nothing and reads nothing
// itself: the game does it when the request comes out. Command's call operator is defined in entry.h,
// which every header that writes a body includes.
#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "ScriptKit/expression.h"
#include "ScriptKit/value.h"

namespace ScriptKit {

class Entry;
class Request;

// The identity of a command: 1 and up, in the order commands are constructed.
using CommandId = std::uint32_t;

namespace detail {

[[nodiscard]] inline CommandId mintCommandId() noexcept {
    static CommandId next = 0;
    return ++next;
}

// One object per type; its address tells one enum from another.
template <class T>
inline constexpr char typeTag{};
template <class T>
[[nodiscard]] constexpr const void* typeOf() noexcept {
    return &typeTag<T>;
}

enum class ArgumentKind : std::uint8_t { Text, Number, Truth, Word };

// One argument as a body wrote it: a literal, or an expression still to be evaluated.
struct Parameter {
    ArgumentKind kind = ArgumentKind::Number;
    std::string_view text;        // Text
    std::int64_t number = 0;      // Number, Truth and Word literals
    const void* type = nullptr;   // Word: which enum
    NodePtr expression;           // Number and Truth: set when the argument is an expression
};

// One argument as the game receives it.
struct Argument {
    ArgumentKind kind = ArgumentKind::Number;
    std::string_view text;
    std::int64_t number = 0;
    const void* type = nullptr;
};

// What a command takes: text, an int, a bool, any enum, or an expression working out to an int or a
// bool.
template <class A>
concept CommandArgument =
    std::same_as<A, std::string_view> || std::same_as<A, const char*> || std::same_as<A, int> ||
    std::same_as<A, bool> || std::is_enum_v<A> ||
    (Scripted<A> && (OperandOfType<A, int> || OperandOfType<A, bool>));

template <CommandArgument A>
[[nodiscard]] Parameter parameterOf(const A& argument) {
    if constexpr (std::same_as<A, std::string_view> || std::same_as<A, const char*>) {
        return Parameter{.kind = ArgumentKind::Text, .text = argument};
    } else if constexpr (std::same_as<A, int>) {
        return Parameter{.kind = ArgumentKind::Number, .number = argument};
    } else if constexpr (std::same_as<A, bool>) {
        return Parameter{.kind = ArgumentKind::Truth, .number = argument ? 1 : 0};
    } else if constexpr (std::is_enum_v<A>) {
        return Parameter{.kind = ArgumentKind::Word, .number = encode(argument), .type = typeOf<A>()};
    } else if constexpr (OperandOfType<A, int>) {
        return Parameter{.kind = ArgumentKind::Number, .expression = nodeOf(argument)};
    } else {
        return Parameter{.kind = ArgumentKind::Truth, .expression = nodeOf(argument)};
    }
}

struct RequestAccess;

}  // namespace detail

// A command: something a script can ask the game to do. The game declares each once, as a constant,
// and usually wraps it in a function that gives its arguments their types and defaults:
//
//     const ScriptKit::Command Walk{"walk"};
//     ScriptKit::Entry walk(Direction direction, int cells = 1) { return Walk(direction, cells); }
//
// An argument is text (a std::string_view or a string literal), an int, a bool, any enum, or an
// expression working out to an int or a bool; anything else does not build. An expression is evaluated
// on the tick the script asks. Text is viewed, never copied: it outlives every script that uses it, as
// a string literal does.
//
// A body refers to the command by address, so the command outlives every script that uses it.
class Command {
public:
    explicit Command(std::string_view name) noexcept : name_(name), id_(detail::mintCommandId()) {}

    // An entry that asks for this command with `arguments`. Defined in entry.h.
    template <class... Args>
        requires(detail::CommandArgument<Args> && ...)
    [[nodiscard]] Entry operator()(Args... arguments) const;

    [[nodiscard]] std::string_view name() const noexcept { return name_; }
    [[nodiscard]] CommandId id() const noexcept { return id_; }

private:
    std::string_view name_;
    CommandId id_;
};

// A command's arguments as the game receives them, every expression already worked out. Each accessor
// reads the argument at `index` as one kind:
//
//     text      what was written as text
//     number    an int, or an expression working out to one
//     truth     a bool, or an expression working out to one
//     word<E>   an enumerator of E
//
// Reading an argument as another kind, or as another enum, throws std::logic_error naming the command
// and the argument; an index past the last argument throws std::out_of_range.
class Arguments {
public:
    [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }

    [[nodiscard]] std::string_view text(std::size_t index) const {
        return at(index, detail::ArgumentKind::Text).text;
    }
    [[nodiscard]] int number(std::size_t index) const {
        return static_cast<int>(at(index, detail::ArgumentKind::Number).number);
    }
    [[nodiscard]] bool truth(std::size_t index) const {
        return at(index, detail::ArgumentKind::Truth).number != 0;
    }
    template <class Word>
        requires std::is_enum_v<Word>
    [[nodiscard]] Word word(std::size_t index) const {
        const detail::Argument& argument = at(index, detail::ArgumentKind::Word);
        if (argument.type != detail::typeOf<Word>()) {
            throw std::logic_error(describe(index) + " is a word of another enum than the one it was read as");
        }
        return static_cast<Word>(argument.number);
    }

private:
    friend class Request;
    friend struct detail::RequestAccess;

    [[nodiscard]] std::string describe(std::size_t index) const {
        return "ScriptKit: argument " + std::to_string(index) + " of command '" +
               std::string(command_ != nullptr ? command_->name() : std::string_view{}) + "'";
    }

    [[nodiscard]] static std::string_view nameOf(detail::ArgumentKind kind) noexcept {
        switch (kind) {
        case detail::ArgumentKind::Text:
            return "text";
        case detail::ArgumentKind::Number:
            return "a number";
        case detail::ArgumentKind::Truth:
            return "a truth";
        case detail::ArgumentKind::Word:
            return "a word";
        }
        return "";
    }

    [[nodiscard]] const detail::Argument& at(std::size_t index, detail::ArgumentKind wanted) const {
        if (index >= values_.size()) {
            throw std::out_of_range(describe(index) + " was read, and the command has " +
                                    std::to_string(values_.size()));
        }
        const detail::Argument& argument = values_[index];
        if (argument.kind != wanted) {
            throw std::logic_error(describe(index) + " is " + std::string(nameOf(argument.kind)) +
                                   ", read as " + std::string(nameOf(wanted)));
        }
        return argument;
    }

    const Command* command_ = nullptr;
    std::vector<detail::Argument> values_;
};

// What a `set` asks for: `value` should become `to`.
template <class T>
struct Change {
    Value<T> value;
    T to;
};

// One thing a script asked for: a command with its arguments, or a Change.
//
//     for (const ScriptKit::Request& request : processor.requests()) {
//         if (const ScriptKit::Command* command = request.command()) {
//             // command->id(), request.arguments()
//         } else if (const auto* change = request.change<bool>()) {
//             // change->value.id(), change->to
//         }
//     }
//
// Handlers and Bindings (bindings.h) do this routing for a game.
class Request {
public:
    // The command asked for; nullptr when the request is a Change.
    [[nodiscard]] const Command* command() const noexcept { return arguments_.command_; }

    // The command's arguments; none when the request is a Change.
    [[nodiscard]] const Arguments& arguments() const noexcept { return arguments_; }

    // The Change asked for, when the request is one of a Value<T>; nullptr when it is a command or a
    // Change of another type.
    template <ValueType T>
    [[nodiscard]] const Change<T>* change() const noexcept {
        if (changeType_ != detail::typeOf<T>()) {
            return nullptr;
        }
        return std::launder(reinterpret_cast<const Change<T>*>(change_.data()));
    }

private:
    friend struct detail::RequestAccess;

    Arguments arguments_;
    ValueId changed_ = 0;                // the value a Change names, whatever its type
    std::int64_t changedTo_ = 0;         // what it becomes, as the integer it is stored as
    const void* changeType_ = nullptr;   // the T of the Change held in change_
    alignas(8) std::array<std::byte, 16> change_{};
};

namespace detail {

// Fills and reads requests for the script processor and the bindings.
struct RequestAccess {
    // Makes `request` a request for `command`, with no arguments yet. The argument storage is kept.
    static void ask(Request& request, const Command& command) noexcept {
        request.arguments_.command_ = &command;
        request.arguments_.values_.clear();
        request.changed_ = 0;
        request.changedTo_ = 0;
        request.changeType_ = nullptr;
    }

    static void add(Request& request, const Argument& argument) { request.arguments_.values_.push_back(argument); }

    // Makes `request` a Change<T> of the value `id` names.
    template <ValueType T>
    static void change(Request& request, ValueId id, std::int64_t raw) noexcept {
        static_assert(sizeof(Change<T>) <= 16 && alignof(Change<T>) <= 8 && std::is_trivially_copyable_v<Change<T>>);
        request.arguments_.command_ = nullptr;
        request.arguments_.values_.clear();
        request.changed_ = id;
        request.changedTo_ = raw;
        request.changeType_ = typeOf<T>();
        ::new (static_cast<void*>(request.change_.data())) Change<T>{ValueAccess::named<T>(id), decode<T>(raw)};
    }

    [[nodiscard]] static bool isChange(const Request& request) noexcept { return request.changeType_ != nullptr; }
    [[nodiscard]] static ValueId changed(const Request& request) noexcept { return request.changed_; }
    [[nodiscard]] static std::int64_t changedTo(const Request& request) noexcept { return request.changedTo_; }
};

// Writes a Change of one type into a request; a `set` entry carries the one for its value's type.
using ChangeWriter = void (*)(Request&, ValueId, std::int64_t) noexcept;

template <ValueType T>
void writeChange(Request& request, ValueId id, std::int64_t raw) noexcept {
    RequestAccess::change<T>(request, id, raw);
}

}  // namespace detail

}  // namespace ScriptKit
