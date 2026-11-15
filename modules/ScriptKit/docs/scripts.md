# Scripts — reference

Everything a script is written in: values, expressions, triggers, the script itself, and each entry of
a body, with the rules each follows and what does not build. The model behind them is in
[concepts.md](concepts.md); the machine that runs them is in
[script-processor.md](script-processor.md).

## Contents

- [Value](#value) — `value.h`
- [Values](#values) — `value.h`
- [Expr](#expr) — `expression.h`
- [evaluate and valuesOf](#evaluate-and-valuesof) — `expression.h`
- [Trigger](#trigger) — `trigger.h`
- [Script](#script) — `script.h`
- [The entries](#the-entries) — `entry.h`
  - [wait](#wait) · [waitWhile](#waitwhile) · [when](#when) · [loop and loopWhile](#loop-and-loopwhile) ·
    [set](#set) · [call](#call) · [stop and restart](#stop-and-restart)
- [What does not build](#what-does-not-build)
- [Finding the entry functions](#finding-the-entry-functions)

## Value

```cpp
template <class T>
class Value {
public:
    Value() noexcept;
    ValueId id() const noexcept;
};

using ValueId = std::uint32_t;
```

A value a script reads or sets, with its type. `T` is `bool`, `int` or an enum type; any other type
fails to compile with a message naming the rule.

The game declares each value once, as a constant. The declaration is the value's identity: constructing
a `Value` hands it the next unused id, from 1 up, so two declarations are two values whatever they are
named. A copy is the same value.

A `Value` holds no data. What it is worth on a tick is in the [`Values`](#values) a processor is
handed.

In an expression a value reads itself: `Score + 1`, `!Ready`, `Signal == Lamp::Red` build
[expressions](#expr). So `==` between two values builds an expression too; to ask whether two `Value`
objects are the same value, compare their `id()`.

## Values

```cpp
class Values {
public:
    Values() = default;
    Values(std::initializer_list<Item> items);                 // {{Ready, true}, {Score, 3}}

    template <class T> void set(Value<T> value, T worth);
    template <class T> T    get(Value<T> value) const;         // throws std::out_of_range
    bool has(ValueId id) const noexcept;

    void         setRaw(ValueId id, std::int64_t raw);
    std::int64_t raw(ValueId id) const;                        // throws std::out_of_range
    void         clear() noexcept;
    std::size_t  size() const noexcept;
};
```

What each value is worth for one tick, by id. [`Bindings::gather`](commands.md#bindings) builds one
from the game's state; a test builds one by hand.

| Member | What it does |
|---|---|
| the braced list | Each element is a value and what it is worth, of the value's own type. |
| `set` | Sets what `value` is worth, replacing what it was. |
| `get` | What `value` is worth. Throws `std::out_of_range` naming the id when it was never set. |
| `has` | Whether the value with that id has been set. |
| `setRaw`, `raw` | The same two by id, for code that holds an id and not a `Value<T>`. Every value is stored as one integer: a `bool` as 0 or 1, an enum as its underlying value. |
| `clear` | Forgets every value and keeps the storage, so filling it again allocates nothing. |

```cpp
ScriptKit::Values values{{Ready, true}, {Score, 3}};
values.set(Score, 4);
values.get(Score);      // 4
values.get(Lives);      // throws: Lives was never set
```

## Expr

```cpp
template <class T>
class Expr {
public:
    Expr(T value);            // a literal of exactly type T
    Expr(Value<T> value);     // what the value is worth when the expression is evaluated
};
```

An expression that works out to a `T`: `bool`, `int` or an enum. It is made from a `Value<T>` or a `T`,
implicitly, and from the operators below. It is a handle on a tree that never changes, so it copies
cheaply. Writing one evaluates nothing.

On either side of an operator stands a `T`, a `Value<T>` or an `Expr<T>`, and at least one side is a
`Value` or an `Expr`.

| Operators | Operand type | Result | Notes |
|---|---|---|---|
| `+` `-` `*` `/`, unary `-` | `int` | `Expr<int>` | Wraps as 32-bit `int` arithmetic does. Dividing by zero is 0. |
| `!` `&&` `\|\|` | `bool` | `Expr<bool>` | |
| `<` `<=` `>` `>=` | `int` | `Expr<bool>` | |
| `==` `!=` | two of the same type: `int`, `bool`, or one enum | `Expr<bool>` | |
| `clamp(value, low, high)` | `int`; `low` and `high` plain `int` | `Expr<int>` | `low` when below, `high` when above. |
| `min(a, b)`, `max(a, b)` | `int` | `Expr<int>` | |

```cpp
Score + 20                               // Expr<int>
(Score + Bonus) * 2 > 50 && !Spent       // Expr<bool>
Signal == Lamp::Red                      // Expr<bool>, from a Value<Lamp>
clamp(PlayerX, 16, 144)                  // Expr<int>
```

Both sides of `&&` and `||` are part of the expression. Evaluating gives what the language's own
operators give: when the left side decides the answer, the right side is not read.

## evaluate and valuesOf

```cpp
template <class T> T evaluate(const Expr<T>& expression, const Values& values);
template <class T> std::vector<ValueId> valuesOf(const Expr<T>& expression);
```

`evaluate` is what the expression works out to against `values`. It throws `std::out_of_range`, naming
the id, when `values` lacks a value the expression reads. A script processor calls it; so can a test.

`valuesOf` is every value the expression reads, each once, in the order first read.

```cpp
const ScriptKit::Values values{{Score, 7}, {Bonus, 2}};
ScriptKit::evaluate(Score * Bonus, values);            // 14
ScriptKit::evaluate(Score / 0, values);                // 0
ScriptKit::evaluate(clamp(Score, 0, 5), values);       // 5
ScriptKit::valuesOf((Score + Bonus) * Score > 3);      // {Score.id(), Bonus.id()}
```

## Trigger

```cpp
struct Trigger {
    std::uint32_t kind = 0;
    int number = 0;
    ValueId value = 0;

    constexpr Trigger() noexcept;
    template <class Kind> constexpr Trigger(Kind kind) noexcept;               // Kind: any enum
    template <class Kind> constexpr Trigger(Kind kind, int number) noexcept;
    template <class Kind> Trigger(Kind kind, Value<bool> value) noexcept;

    constexpr bool set() const noexcept;
    constexpr bool operator==(const Trigger&) const noexcept;
};
```

What sets a script off: a kind from any enum of the game's and, when the kind takes one, a number or a
`Value<bool>`.

```cpp
enum class On : std::uint8_t { Interact, Touch, Near, Becomes };

constexpr ScriptKit::Trigger Interact{On::Interact};
constexpr ScriptKit::Trigger Near(int distance) { return {On::Near, distance}; }
ScriptKit::Trigger Becomes(ScriptKit::Value<bool> value) { return {On::Becomes, value}; }
```

| Rule | |
|---|---|
| Equality | Two triggers are the same when `kind`, `number` and `value` all match. `Near(48)` and `Near(32)` are two triggers. |
| `kind` | The enumerator's value plus one, so the enum's first enumerator is a trigger like any other. |
| A default `Trigger` | No trigger: `kind` is 0 and `set()` is false. A script whose `on` is left at it is started only by name, and `start` handed one starts nothing. |

ScriptKit compares triggers. The game decides when one has happened and tells a processor with
[`ScriptProcessor::start`](script-processor.md#starting-stopping-restarting).

## Script

```cpp
struct Script {
    std::string_view name;
    Trigger on{};
    int from = 0;
    Expr<bool> when = true;
    int priority = 0;
    std::vector<const Script*> interrupts;
    Body body;
};
```

Written with designated initializers, in the order the fields are declared.

| Field | What it is |
|---|---|
| `name` | Names the script in every error about it, and in a readout. A view: the text outlives the script, as a string literal does. The scripts of one processor have different names. |
| `on` | The trigger that starts it. Left unset, only the game starts it, by name, or another script, by `restart`. |
| `from` | A side or a source, in the game's own enum. ScriptKit stores it and reads it nowhere; the code that fires triggers may read it to decide whether this script's trigger applies. |
| `when` | The criteria: an expression that holds for the trigger to start the script. Evaluated when the trigger fires. |
| `priority` | Where it comes in the stepping order: higher is stepped first each tick, scripts of one priority in the order of the processor's list. It never decides whether a script runs. |
| `interrupts` | The scripts it holds still for as long as it runs ([concepts.md](concepts.md#holding-a-script-still)), by address. |
| `body` | The program: a `std::vector<Entry>`. |

A script is referred to by address wherever it is named: a processor's list, `interrupts`, `call`,
`stop`, `restart`. So it is declared once, as a constant, and outlives every processor that names it.

A script names the values it reads and sets where it uses them. A processor works out both lists when
it is built: [`ScriptProcessor::reads` and `sets`](script-processor.md#what-a-script-reads-and-sets).

## The entries

An `Entry` is one line of a body. Each of the functions below returns one; a
[command](commands.md#command) returns one too. `Body` is `std::vector<Entry>`. An entry is read only by
a script processor.

### wait

```cpp
Entry wait(std::chrono::nanoseconds time);
template <std::integral Count> Entry wait(Count ticks);
```

Hold the script. `wait` takes exactly three things:

| Written | Holds for |
|---|---|
| `wait(500ms)`, `wait(5s)` | A time: any `std::chrono` duration that converts to nanoseconds exactly. The processor turns it into ticks when it is built, through the `TimingProfile` of the `EngineConfig` it is built with, rounding to the nearest tick. |
| `wait(config.timing.ticksForDuration(500ms))` | What the engine's own conversion returns, which is a count of ticks. |
| `wait(1)`, `wait(30)` | A count of ticks. |

The entry after a `wait(n)` runs `n` ticks after the tick the script reached the wait. A count of zero
or less, and a time that comes to no ticks, hold for nothing: the script moves straight on.

| Entry | At 60 ticks a second | At 50 |
|---|---|---|
| `wait(500ms)` | 30 ticks | 25 |
| `wait(5s)` | 300 | 250 |
| `wait(1ms)` | 0 | 0 |
| `wait(30)` | 30 | 30 |

Ticks count only while the script is stepped. A script [held](concepts.md#holding-a-script-still) in the
middle of a wait carries on with the wait where it was.

`wait(1)` at the end of a loop's body makes the loop go round once a tick.

### waitWhile

```cpp
Entry waitWhile(Expr<bool> condition);
```

Hold while `condition` holds. It is tested once each tick; the entry after it runs on the tick the
condition is false. A condition that is false when the script reaches it holds for nothing.

```cpp
walk(Direction::Up, 2), waitWhile(Walking),
```

### when

```cpp
When when(Expr<bool> condition, Body body);

class When {
public:
    When orWhen(Expr<bool> condition, Body body) &&;
    When otherwise(Body body) &&;
    operator Entry() &&;
};
```

A branch. The first arm whose condition holds runs, and only that one; `otherwise` runs when none
does. Each condition is evaluated on the tick the script reaches it. A `When` becomes an `Entry`
wherever one is needed, so it is written straight into a body.

```cpp
when(Ready, { say("Here we go."), set(LidOpen, true) })
    .orWhen(Score >= 50, { say("Close enough."), set(Score, Score - 50) })
    .otherwise({ say("Not yet.") }),
```

### loop and loopWhile

```cpp
Entry loop(Body body);
Entry loopWhile(Expr<bool> condition, Body body);
```

`loop` runs its body over and over until the script is stopped or restarted. `loopWhile` tests its
condition before each trip, and the script moves on past it on the tick the condition is false.

A loop with nothing in it that waits runs until the script's budget for the tick is spent, and carries
on from there on the next tick ([script-processor.md](script-processor.md#the-budget)).

```cpp
loop({ face(Left), wait(1s), face(Right), wait(1s) }),
loopWhile(Heat < 100, { stoke(), wait(1) }),
```

### set

```cpp
template <class T> Entry set(Value<T> value, Expr<T> to);
```

Ask the game to make `value` worth `to`. The expression is evaluated on the tick the script reaches the
entry, and the request comes out of the processor as a [`Change<T>`](commands.md#change-and-request).
The script does not write the value; the game does, when it applies the request.

```cpp
set(LidOpen, true),
set(Score, Score + 20),
set(Signal, Lamp::Red),
```

### call

```cpp
Entry call(const Script& script);
```

Run `script`'s body, then carry on with the entry after this one. The caller stays the running script,
one level deeper; the callee's `on`, `when`, `priority` and `interrupts` play no part. A script
does not call itself, directly or through others: a processor refuses one that does.

### stop and restart

```cpp
Entry stop(const Script& script);
Entry restart(const Script& script);
```

`stop` ends a script wherever it is: running, waiting or held. `restart` puts it back at its first
entry, running; one that was off starts, and one that is held stays held.

A script may name itself. One that stops itself is done; one that restarts itself is done for the tick
and runs from its first entry on the next.

`call`, `stop` and `restart` take the script by reference and keep its address. Handing one a temporary
does not build.

## What does not build

The types of values are checked where a script is written.

| Written | Why it does not build |
|---|---|
| `set(Ready, 3)` | A `Value<bool>` is set to a `bool`. |
| `set(Score, Ready)` | A `Value<int>` is set to an `int`. |
| `when(Score, { … })` | A condition is a `bool`: write `Score > 0`. |
| `Ready + 1` | Arithmetic is on `int`. |
| `Score && Ready` | Logic is on `bool`. |
| `Signal == Direction::Up` | Equality is between two of the same type. |
| `Say(1.5)`, `Say(std::string{"hi"})` | A command's argument is text, an `int`, a `bool`, an enum, or an expression working out to an `int` or a `bool` ([commands.md](commands.md#command)). |
| `wait(1.5)`, `wait(true)`, `wait("soon")` | A wait is a time, or a whole count of ticks. |
| `wait(1.5s)` | A time converts to nanoseconds exactly; write `wait(1500ms)`. |
| `call(drive("east", 0))` | `call`, `stop` and `restart` keep the address of a script that outlives the body. |

## Finding the entry functions

The functions above are in the `ScriptKit` namespace, and most are found through their arguments: a
`Value`, an `Expr` or a `Script` is a ScriptKit type, so `set(Score, 5)`, `waitWhile(Walking)`,
`when(Ready, …)`, `loopWhile(Heat < 100, …)`, `call(blink)`, `stop(nap)` and `restart(nap)`
need no qualifying.

`loop({ … })` and `wait(1)` take a braced list and a bare number, which name no namespace. A file that
writes scripts names those two, and usually the time literals:

```cpp
using ScriptKit::loop;
using ScriptKit::wait;
using namespace std::chrono_literals;
```
