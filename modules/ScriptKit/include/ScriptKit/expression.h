// Expr — an expression over values, built as data and worked out later against a Values.
//
//     Score + 20                          // Expr<int>
//     !Ready && Score >= 3               // Expr<bool>
//     Signal == Lamp::Red                 // Expr<bool>, from a Value<Lamp>
//     clamp(PlayerX, 16, 144)             // Expr<int>
//
//     ScriptKit::evaluate(Score + 20, values);   // 23 when Score is 3
//
// Writing an expression evaluates nothing: each operator returns the expression. A script processor
// evaluates it on the tick the script reaches it, against the values the processor is handed.
#pragma once

#include <concepts>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "ScriptKit/value.h"

namespace ScriptKit {

template <class T>
class Expr;

namespace detail {

enum class Op : std::uint8_t {
    Literal, Read,
    Add, Subtract, Multiply, Divide, Negate,
    Not, And, Or,
    Less, LessEqual, Greater, GreaterEqual, Equal, NotEqual,
    Clamp, Min, Max,
};

// One node of an expression's tree. Every node works out to one integer: a bool as 0 or 1, an enum as
// its underlying value.
struct Node {
    Op op = Op::Literal;
    std::int64_t literal = 0;          // Literal
    ValueId value = 0;                 // Read
    std::shared_ptr<const Node> a;     // the operand, or the left one
    std::shared_ptr<const Node> b;     // the right operand; Clamp's low bound
    std::shared_ptr<const Node> c;     // Clamp's high bound
};

using NodePtr = std::shared_ptr<const Node>;

// An int result as int arithmetic leaves it: wrapped to 32 bits.
[[nodiscard]] constexpr std::int64_t wrapped(std::int64_t value) noexcept {
    return static_cast<std::int32_t>(value);
}

// What `node` works out to against `values`. Throws std::out_of_range, naming the id, when `values`
// lacks a value the node reads.
[[nodiscard]] inline std::int64_t evaluate(const Node& node, const Values& values) {
    switch (node.op) {
    case Op::Literal:
        return node.literal;
    case Op::Read:
        return values.raw(node.value);
    case Op::Add:
        return wrapped(evaluate(*node.a, values) + evaluate(*node.b, values));
    case Op::Subtract:
        return wrapped(evaluate(*node.a, values) - evaluate(*node.b, values));
    case Op::Multiply:
        return wrapped(evaluate(*node.a, values) * evaluate(*node.b, values));
    case Op::Divide: {
        const std::int64_t divisor = evaluate(*node.b, values);
        return divisor == 0 ? 0 : wrapped(evaluate(*node.a, values) / divisor);
    }
    case Op::Negate:
        return wrapped(-evaluate(*node.a, values));
    case Op::Not:
        return evaluate(*node.a, values) == 0 ? 1 : 0;
    case Op::And:
        return evaluate(*node.a, values) != 0 && evaluate(*node.b, values) != 0 ? 1 : 0;
    case Op::Or:
        return evaluate(*node.a, values) != 0 || evaluate(*node.b, values) != 0 ? 1 : 0;
    case Op::Less:
        return evaluate(*node.a, values) < evaluate(*node.b, values) ? 1 : 0;
    case Op::LessEqual:
        return evaluate(*node.a, values) <= evaluate(*node.b, values) ? 1 : 0;
    case Op::Greater:
        return evaluate(*node.a, values) > evaluate(*node.b, values) ? 1 : 0;
    case Op::GreaterEqual:
        return evaluate(*node.a, values) >= evaluate(*node.b, values) ? 1 : 0;
    case Op::Equal:
        return evaluate(*node.a, values) == evaluate(*node.b, values) ? 1 : 0;
    case Op::NotEqual:
        return evaluate(*node.a, values) != evaluate(*node.b, values) ? 1 : 0;
    case Op::Clamp: {
        const std::int64_t value = evaluate(*node.a, values);
        const std::int64_t low = evaluate(*node.b, values);
        const std::int64_t high = evaluate(*node.c, values);
        return value < low ? low : value > high ? high : value;
    }
    case Op::Min: {
        const std::int64_t left = evaluate(*node.a, values);
        const std::int64_t right = evaluate(*node.b, values);
        return right < left ? right : left;
    }
    case Op::Max: {
        const std::int64_t left = evaluate(*node.a, values);
        const std::int64_t right = evaluate(*node.b, values);
        return right > left ? right : left;
    }
    }
    return 0;
}

// Adds every value `node` reads to `into`, each once, in the order first read.
inline void collectValues(const Node& node, std::vector<ValueId>& into) {
    if (node.op == Op::Read) {
        for (const ValueId id : into) {
            if (id == node.value) {
                return;
            }
        }
        into.push_back(node.value);
        return;
    }
    for (const NodePtr* child : {&node.a, &node.b, &node.c}) {
        if (*child) {
            collectValues(**child, into);
        }
    }
}

// Whether `node` reads any value at all.
[[nodiscard]] inline bool readsValues(const Node& node) noexcept {
    if (node.op == Op::Read) {
        return true;
    }
    return (node.a && readsValues(*node.a)) || (node.b && readsValues(*node.b)) ||
           (node.c && readsValues(*node.c));
}

// Reaches an expression's tree.
struct ExprAccess {
    template <class T>
    [[nodiscard]] static const NodePtr& node(const Expr<T>& expression) noexcept {
        return expression.node_;
    }
    template <class T>
    [[nodiscard]] static Expr<T> make(NodePtr node) {
        return Expr<T>(std::move(node));
    }
};

}  // namespace detail

// An expression that works out to a T: bool, int, or one of the game's enums. It is made from a
// Value<T> or a T, implicitly, and from the operators below. It is a small handle on a tree that never
// changes, so it copies cheaply.
//
// Only matching types meet: an Expr<int> is made from a Value<int> or an int and from nothing else,
// so `set(Ready, 3)` and `Ready + 1` do not build.
template <class T>
class Expr {
    static_assert(ValueType<T>, "ScriptKit::Expr<T>: T is bool, int or an enum type");

public:
    // The literal `value`. The argument's type is exactly T: an int does not become an Expr<bool>.
    template <std::same_as<T> U>
    Expr(U value)
        : node_(std::make_shared<const detail::Node>(
              detail::Node{.op = detail::Op::Literal, .literal = detail::encode(value)})) {}

    // What `value` is worth on the tick the expression is evaluated.
    Expr(Value<T> value)
        : node_(std::make_shared<const detail::Node>(
              detail::Node{.op = detail::Op::Read, .value = value.id()})) {}

private:
    friend struct detail::ExprAccess;
    explicit Expr(detail::NodePtr node) noexcept : node_(std::move(node)) {}

    detail::NodePtr node_;
};

namespace detail {

// What can stand on either side of an operator, and the type it works out to: a T, a Value<T> or an
// Expr<T>.
template <class X>
struct OperandOf {};
template <ValueType T>
struct OperandOf<T> {
    using type = T;
};
template <class T>
struct OperandOf<Value<T>> {
    using type = T;
};
template <class T>
struct OperandOf<Expr<T>> {
    using type = T;
};

template <class X>
inline constexpr bool isScripted = false;
template <class T>
inline constexpr bool isScripted<Value<T>> = true;
template <class T>
inline constexpr bool isScripted<Expr<T>> = true;

// X is a Value or an Expr.
template <class X>
concept Scripted = isScripted<X>;

// X works out to a T.
template <class X, class T>
concept OperandOfType = requires { typename OperandOf<X>::type; } && std::same_as<typename OperandOf<X>::type, T>;

// A and B both work out to a T, and at least one of them is a Value or an Expr — so an operator
// between two plain values stays the language's own.
template <class A, class B, class T>
concept Operands = (isScripted<A> || isScripted<B>) && OperandOfType<A, T> && OperandOfType<B, T>;

// A and B work out to the same type, whichever it is.
template <class A, class B>
concept MatchingOperands = (isScripted<A> || isScripted<B>) && requires {
    typename OperandOf<A>::type;
    typename OperandOf<B>::type;
} && std::same_as<typename OperandOf<A>::type, typename OperandOf<B>::type>;

template <class X>
[[nodiscard]] NodePtr nodeOf(const X& operand) {
    return ExprAccess::node(Expr<typename OperandOf<X>::type>(operand));
}

template <class T, class A>
[[nodiscard]] Expr<T> unary(Op op, const A& a) {
    return ExprAccess::make<T>(std::make_shared<const Node>(Node{.op = op, .a = nodeOf(a)}));
}

template <class T, class A, class B>
[[nodiscard]] Expr<T> binary(Op op, const A& a, const B& b) {
    return ExprAccess::make<T>(std::make_shared<const Node>(Node{.op = op, .a = nodeOf(a), .b = nodeOf(b)}));
}

}  // namespace detail

// Arithmetic on int: each side an int, a Value<int> or an Expr<int>, at least one of them not a plain
// int. The result wraps as int arithmetic does. Dividing by zero is 0.
template <class A, class B>
    requires detail::Operands<A, B, int>
[[nodiscard]] Expr<int> operator+(const A& a, const B& b) {
    return detail::binary<int>(detail::Op::Add, a, b);
}
template <class A, class B>
    requires detail::Operands<A, B, int>
[[nodiscard]] Expr<int> operator-(const A& a, const B& b) {
    return detail::binary<int>(detail::Op::Subtract, a, b);
}
template <class A, class B>
    requires detail::Operands<A, B, int>
[[nodiscard]] Expr<int> operator*(const A& a, const B& b) {
    return detail::binary<int>(detail::Op::Multiply, a, b);
}
template <class A, class B>
    requires detail::Operands<A, B, int>
[[nodiscard]] Expr<int> operator/(const A& a, const B& b) {
    return detail::binary<int>(detail::Op::Divide, a, b);
}
template <detail::Scripted A>
    requires detail::OperandOfType<A, int>
[[nodiscard]] Expr<int> operator-(const A& a) {
    return detail::unary<int>(detail::Op::Negate, a);
}

// Logic on bool. Both sides of && and || are part of the expression: nothing is skipped when it is
// written, and the evaluation gives what the language's own operators give.
template <detail::Scripted A>
    requires detail::OperandOfType<A, bool>
[[nodiscard]] Expr<bool> operator!(const A& a) {
    return detail::unary<bool>(detail::Op::Not, a);
}
template <class A, class B>
    requires detail::Operands<A, B, bool>
[[nodiscard]] Expr<bool> operator&&(const A& a, const B& b) {
    return detail::binary<bool>(detail::Op::And, a, b);
}
template <class A, class B>
    requires detail::Operands<A, B, bool>
[[nodiscard]] Expr<bool> operator||(const A& a, const B& b) {
    return detail::binary<bool>(detail::Op::Or, a, b);
}

// Ordering on int.
template <class A, class B>
    requires detail::Operands<A, B, int>
[[nodiscard]] Expr<bool> operator<(const A& a, const B& b) {
    return detail::binary<bool>(detail::Op::Less, a, b);
}
template <class A, class B>
    requires detail::Operands<A, B, int>
[[nodiscard]] Expr<bool> operator<=(const A& a, const B& b) {
    return detail::binary<bool>(detail::Op::LessEqual, a, b);
}
template <class A, class B>
    requires detail::Operands<A, B, int>
[[nodiscard]] Expr<bool> operator>(const A& a, const B& b) {
    return detail::binary<bool>(detail::Op::Greater, a, b);
}
template <class A, class B>
    requires detail::Operands<A, B, int>
[[nodiscard]] Expr<bool> operator>=(const A& a, const B& b) {
    return detail::binary<bool>(detail::Op::GreaterEqual, a, b);
}

// Equality on any one type: two ints, two bools, or two of the same enum.
template <class A, class B>
    requires detail::MatchingOperands<A, B>
[[nodiscard]] Expr<bool> operator==(const A& a, const B& b) {
    return detail::binary<bool>(detail::Op::Equal, a, b);
}
template <class A, class B>
    requires detail::MatchingOperands<A, B>
[[nodiscard]] Expr<bool> operator!=(const A& a, const B& b) {
    return detail::binary<bool>(detail::Op::NotEqual, a, b);
}

// `value` held within `low` and `high`: `low` when it is below, `high` when it is above.
template <detail::Scripted A>
    requires detail::OperandOfType<A, int>
[[nodiscard]] Expr<int> clamp(const A& value, int low, int high) {
    return detail::ExprAccess::make<int>(std::make_shared<const detail::Node>(detail::Node{
        .op = detail::Op::Clamp, .a = detail::nodeOf(value), .b = detail::nodeOf(Expr<int>(low)),
        .c = detail::nodeOf(Expr<int>(high))}));
}

// The lesser and the greater of two ints.
template <class A, class B>
    requires detail::Operands<A, B, int>
[[nodiscard]] Expr<int> min(const A& a, const B& b) {
    return detail::binary<int>(detail::Op::Min, a, b);
}
template <class A, class B>
    requires detail::Operands<A, B, int>
[[nodiscard]] Expr<int> max(const A& a, const B& b) {
    return detail::binary<int>(detail::Op::Max, a, b);
}

// What `expression` works out to against `values`. Throws std::out_of_range, naming the id, when
// `values` lacks a value the expression reads.
template <ValueType T>
[[nodiscard]] T evaluate(const Expr<T>& expression, const Values& values) {
    return detail::decode<T>(detail::evaluate(*detail::ExprAccess::node(expression), values));
}

// Every value `expression` reads, each once, in the order first read.
template <ValueType T>
[[nodiscard]] std::vector<ValueId> valuesOf(const Expr<T>& expression) {
    std::vector<ValueId> ids;
    detail::collectValues(*detail::ExprAccess::node(expression), ids);
    return ids;
}

}  // namespace ScriptKit
