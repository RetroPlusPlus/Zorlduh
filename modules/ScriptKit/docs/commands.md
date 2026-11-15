# Commands, bindings and handlers — reference

What a script asks for, what comes out of a processor when it does, and the game's side of it: how each
value is read and written, what each command does, and the two calls that carry a tick. The model is in
[concepts.md](concepts.md).

## Contents

- [Command](#command) — `command.h`
- [Arguments](#arguments) — `command.h`
- [Change and Request](#change-and-request) — `command.h`
- [Bindings](#bindings) — `bindings.h`
- [Handlers](#handlers) — `bindings.h`
- [The two calls of the tick](#the-two-calls-of-the-tick) — `bindings.h`
- [Doing it by hand](#doing-it-by-hand)

## Command

```cpp
class Command {
public:
    explicit Command(std::string_view name) noexcept;

    template <class... Args> Entry operator()(Args... arguments) const;

    std::string_view name() const noexcept;
    CommandId id() const noexcept;
};

using CommandId = std::uint32_t;
```

A command is something a script can ask the game to do. It has a name and, each time a body uses it,
arguments. It does nothing and reads nothing itself.

The game declares each command once, as a constant, and usually wraps it in a function that gives its
arguments their types and defaults:

```cpp
enum class Bubble : std::uint8_t { Plain, Shout };

const ScriptKit::Command Say{"say"};
ScriptKit::Entry say(std::string_view text, Bubble bubble = Bubble::Plain) { return Say(text, bubble); }
```

Calling the command returns an [entry](scripts.md#the-entries) that asks for it. A body then reads
`say("Hello!")` or `say("Hello!", Bubble::Shout)`, and a wrong argument is a compile error at the body.

**An argument** is one of:

| Written as | Read by the handler with |
|---|---|
| text: a `std::string_view` or a string literal | `text` |
| an `int`, or an expression working out to one: `Score + 1`, a `Value<int>` | `number` |
| a `bool`, or an expression working out to one: `!Ready`, a `Value<bool>` | `truth` |
| an enumerator of any enum | `word<E>` |

Anything else does not build. An expression is evaluated on the tick the script reaches the command,
against the values the processor was handed.

The arguments are the command's whole configuration. What varies from one use to the next is an
argument — the text, a style, who speaks, how far. What is the same for every use is in the handler.
What varies with the state of the game is a value, passed as an expression.

Text is viewed, never copied: it outlives every script that uses it, as a string literal does. A body
refers to the command by address, so the command outlives the scripts too. Commands are told apart by
`id()`, handed out from 1 in the order they are constructed; `name()` is for errors and readouts.

## Arguments

```cpp
class Arguments {
public:
    std::size_t size() const noexcept;
    std::string_view text(std::size_t index) const;
    int  number(std::size_t index) const;
    bool truth(std::size_t index) const;
    template <class Word> Word word(std::size_t index) const;     // Word: an enum
};
```

A command's arguments as the game receives them, every expression already worked out. Each accessor
reads the argument at `index` as one kind.

| Read | Throws |
|---|---|
| an argument as another kind: `number(0)` on text | `std::logic_error`, naming the command and the argument |
| `word<E>` on an enumerator of another enum | `std::logic_error`, the same way |
| an index past the last argument | `std::out_of_range` |

```cpp
// For say("Hello!", Bubble::Shout):
arguments.size();               // 2
arguments.text(0);              // "Hello!"
arguments.word<Bubble>(1);      // Bubble::Shout
arguments.number(1);            // throws: argument 1 of command 'say' is a word, read as a number
```

## Change and Request

```cpp
template <class T>
struct Change {
    Value<T> value;
    T to;
};

class Request {
public:
    const Command* command() const noexcept;
    const Arguments& arguments() const noexcept;
    template <class T> const Change<T>* change() const noexcept;
};
```

A `Request` is one thing a script asked for. It is one of two things:

| | `command()` | `arguments()` | `change<T>()` |
|---|---|---|---|
| a command | the command | its arguments | `nullptr` |
| a change, from `set(value, to)` | `nullptr` | none | the `Change<T>` for the value's own `T`; `nullptr` for any other |

A `Change<T>` says `value` should become `to`. It asks; the game writes.

Requests come out of [`ScriptProcessor::requests`](script-processor.md#requests) in the order asked.

## Bindings

```cpp
template <class State>
class Bindings {
public:
    Bindings(std::initializer_list<Binding<State>> bindings);

    Values gather(const State& state, std::span<const ValueId> ids) const;
    void   gather(Values& values, const State& state, std::span<const ValueId> ids) const;
    bool   apply(State& state, const Request& request) const;
};
```

How a game reads and writes each value its scripts use. It is templated on the game's state type and
built once, from a list in which each element binds one value in one of three forms:

```cpp
const ScriptKit::Bindings<State> bindings{
    {Score,   &State::score},                                        // a member: read and written
    {NearBy,  [](const State& state) { return distance(state) < 48; }},   // a getter: read only
    {LidOpen, [](const State& state) { return state.lid.open; },   // a getter and a setter
               [](State& state, bool open) { state.lid.open = open; state.lid.timer = 0; }},
};
```

| Form | Read | Written |
|---|---|---|
| `{value, &State::member}` | the member | the member. The member's type is exactly the value's. |
| `{value, getter}` | what `getter(state)` returns | never: the value is read only |
| `{value, getter, setter}` | what `getter(state)` returns | `setter(state, to)` |

A getter is how a script reads something the game works out — a distance, whether a message is
showing. A setter is how setting a value does more than store it: the getting-started program places
its patrol car at the edge of the screen when a script sets `PatrolOut`.

| Member | What it does |
|---|---|
| `gather(state, ids)` | A `Values` holding what each of `ids` is worth in `state` now. Throws `std::out_of_range` naming an id that has no binding. |
| `gather(values, state, ids)` | The same into `values`, which is emptied first and keeps its storage: gathering into one `Values` tick after tick allocates nothing. |
| `apply(state, request)` | Writes the `Change` in `request` through the value's binding and returns true. Returns false, writing nothing, when `request` is a command or the value is bound read only. Throws `std::out_of_range` naming the id when the value has no binding. |

A value bound twice keeps its first binding.

## Handlers

```cpp
template <class State>
class Handlers {
public:
    Handlers(std::initializer_list<Handler<State>> handlers);
    void apply(State& state, const Request& request) const;
};
```

What a game does for each command: one function per command, taking the state and the arguments.

```cpp
const ScriptKit::Handlers<State> handlers{
    {Say,  [](State& state, const ScriptKit::Arguments& arguments) {
               state.messages.push_back({arguments.text(0), arguments.word<Bubble>(1)});
           }},
    {Walk, [](State& state, const ScriptKit::Arguments& arguments) {
               state.host.walk = Walking{arguments.word<Direction>(0), arguments.number(1)};
           }},
};
```

`apply` runs the function of the command `request` asks for. It does nothing when `request` is a
change, and throws `std::out_of_range` naming the command when the command has no function. A command
listed twice keeps its first function.

A handler is where the world's rules are: what a message box does with a second message, whether a
step is blocked by a wall. A script says what should happen and when; the handler does it, in ordinary
C++ over the game's state.

## The two calls of the tick

```cpp
template <class State>
void applyScripts(std::span<ScriptProcessor* const> processors, const Bindings<State>& bindings,
                  const Handlers<State>& handlers, State& state);

template <class State>
void stepScripts(std::span<ScriptProcessor* const> processors, const Bindings<State>& bindings,
                 const State& state);
```

Both also take the processors as a braced list of addresses.

```cpp
void tick(State& state, const Input& input) {
    ScriptKit::applyScripts({&state.host, &state.lamp}, bindings, handlers, state);
    // … the game's own logic: input, movement, firing triggers …
    ScriptKit::stepScripts({&state.host, &state.lamp}, bindings, state);
}
```

**`applyScripts`** is the first call of the tick. For each processor in the order given, it applies
what the processor's last advance asked for, request by request in the order asked — a change through
`bindings`, a command through `handlers` — and then drops those requests, so nothing is applied twice.
It is the one place a script's effects are written, on the thread that runs the tick.

**`stepScripts`** is the last. For every processor that is not idle it gathers the values the
processor's scripts read; then it advances each of them. Every processor's values are gathered before
any advances, so none sees what another did on this tick, and the order the processors advance in does
not change the result. An idle processor is neither gathered for nor advanced.

The processors and their order are the game's choice. The order is the order requests are applied in,
which makes it part of how the game plays out the same way every time. A step and the apply that
follows it are given the same processors.

Between the two calls is the game's own logic and the only time it touches a processor: `start`,
`stop`, `restart`, and reading where a script is.

| On tick N | On tick N+1 |
|---|---|
| The game fires a trigger; a script starts. | |
| `stepScripts`: the script runs, and asks for something. | `applyScripts`: what it asked for is written. |
| | `stepScripts`: every script reads the state with that written. |

## Doing it by hand

The two calls are a few lines each, and a test or a tool can do the same with one processor:

```cpp
ScriptKit::Values values = bindings.gather(state, processor.reads());
processor.advance(values);
for (const ScriptKit::Request& request : processor.requests()) {
    if (request.command() != nullptr) {
        handlers.apply(state, request);
    } else {
        bindings.apply(state, request);
    }
}
processor.clearRequests();
```

A test needs neither table: it hands the processor a `Values` it built by hand and reads
`processor.requests()` ([how-to.md](how-to.md#test-a-script-with-no-game)).
