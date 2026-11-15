# The script processor — reference

`ScriptProcessor` is the running state of a set of scripts, stepped once per tick. This page covers
building one, starting and stopping scripts, what one tick does instruction by instruction, a worked
trace, and what an advance reads and writes. The model is in [concepts.md](concepts.md); what scripts
are written in is in [scripts.md](scripts.md).

## Contents

- [The class](#the-class)
- [Building one](#building-one)
- [The three states of a script](#the-three-states-of-a-script)
- [Starting, stopping, restarting](#starting-stopping-restarting)
- [One tick](#one-tick)
  - [What each entry becomes](#what-each-entry-becomes)
  - [The budget](#the-budget)
  - [Reading after asking](#reading-after-asking)
- [A worked trace](#a-worked-trace)
- [Requests](#requests)
- [What a script reads and sets](#what-a-script-reads-and-sets)
- [Where a script is](#where-a-script-is)
- [What an advance reads and writes](#what-an-advance-reads-and-writes)

## The class

```cpp
class ScriptProcessor {
public:
    ScriptProcessor(std::vector<const Script*> scripts, const retropp::EngineConfig& config);

    int budget = 32;

    int  start(Trigger trigger, const Values& values);
    void start(const Script& script);
    void stop(const Script& script);
    void restart(const Script& script);

    void advance(const Values& values);

    std::span<const Request> requests() const noexcept;
    void clearRequests() noexcept;

    bool idle() const noexcept;

    std::span<const ValueId> reads() const noexcept;
    std::span<const ValueId> reads(const Script& script) const;
    std::span<const ValueId> sets(const Script& script) const;

    bool running(const Script& script) const noexcept;
    bool suspended(const Script& script) const noexcept;
    bool waiting(const Script& script) const noexcept;
    std::size_t position(const Script& script) const;
    std::size_t length(const Script& script) const;
    std::size_t depth(const Script& script) const;
};
```

A processor is plain data. It is kept in the game's state, and a copy runs on by itself from where the
original was; the instructions the two were built with are shared between them.

## Building one

```cpp
ScriptKit::ScriptProcessor host{{&greet, &tidy, &yawn}, config};
```

The constructor takes the scripts by address and the engine configuration. It turns every body into
instructions ([below](#what-each-entry-becomes)), turns every time in a `wait` into ticks through
`config.timing.ticksForDuration`, and works out what each script
[reads and sets](#what-a-script-reads-and-sets). Every script stays where it was declared; the
processor keeps addresses, so the scripts outlive it.

It throws `std::logic_error`, naming the script, for:

| Refused | Why |
|---|---|
| Two scripts of one name | A name says which script an error or a readout means. |
| A `call`, `stop`, `restart` or `interrupts` naming a script that is not in the list | A script reaches only scripts on its own processor. |
| A script that calls itself, directly or through other scripts | The calls would never come back. |
| Scripts that interrupt each other in a ring | Each would hold the next forever. |
| A null pointer in the list | |

The order of the list matters in one way: scripts of the same `priority` are stepped in list order.

## The three states of a script

| State | Means | `running` | `suspended` |
|---|---|---|---|
| **off** | Not started, or its body has ended, or it was stopped. | false | false |
| **running** | Started and not yet ended. It is stepped each tick. | true | false |
| **held** | Running, and another running script lists it in `interrupts`. It is not stepped. | true | true |

Holding is worked out, not stored: a script is held exactly while some running script lists it. So a
script started while one that interrupts it is running starts held; a script held by two others carries
on when both have ended; and the script doing the holding may itself be held.

## Starting, stopping, restarting

| Call | What it does |
|---|---|
| `start(trigger, values)` | Starts every script whose `on` equals `trigger` and whose `when` holds against `values`, unless it is already running. Returns how many started. A default `Trigger` starts nothing. Throws `std::out_of_range`, naming the id, when `values` lacks a value a matching script's `when` reads. |
| `start(script)` | Starts `script` whatever its `on` and `when` say, unless it is already running. |
| `stop(script)` | Ends `script`, wherever it is. The scripts it was holding carry on. |
| `restart(script)` | Puts `script` back at its first instruction, running; one that was off starts. One that is held stays held. |

The three that take a script throw `std::logic_error` when it is not one of this processor's.

`start(trigger, values)` is handed the values because `when` reads them. Gather the ones the processor
reads:

```cpp
host.start(Interact, bindings.gather(state, host.reads()));
```

A script that starts is at its first instruction and has run nothing yet. It runs on the processor's
next `advance`.

## One tick

`advance(values)` is one tick. It drops what the advance before asked for, then steps every running
script that is not held, from the highest `priority` to the lowest, scripts of one priority in list
order.

A script runs instructions from where it left off until one of these stops it for the tick:

| What stops it | And then |
|---|---|
| A `wait` of one tick or more | It carries on that many ticks later. |
| A `waitWhile` whose condition holds | It tests again next tick. |
| The end of its body | The script is off. The scripts it was holding carry on — on this tick if they are stepped after it, otherwise on the next. |
| The [budget](#the-budget) | It carries on next tick from where it stopped. |
| [Reading after asking](#reading-after-asking) | It carries on next tick, at the instruction that reads. |
| `stop` or `restart` of itself | Stopped, it is off. Restarted, it runs from its first instruction next tick. |

`stop` and `restart` of another script take effect at once: a script restarted by one stepped before it
runs from its first instruction on the same tick.

### What each entry becomes

A body is turned into a flat list of instructions. `position` and `length` count these.

| Entry | Instructions |
|---|---|
| a command | 1: record the request |
| `set` | 1: record the change |
| `wait` | 1 |
| `waitWhile` | 1: the test |
| `call` | 1; the callee's instructions are its own |
| `stop`, `restart` | 1 |
| `loop({…})` | the body, then 1: jump back |
| `loopWhile(c, {…})` | 1: test, and jump past the loop when false; the body; 1: jump back |
| `when(…)` | per arm, 1: test, and jump to the next arm when false; the arm's body; and, when another arm or an `otherwise` follows, 1: jump past the rest |

```cpp
.body = {loopWhile(Count < 3, {mark(), wait(1)}), wait(5), say("out")},
```

| # | Instruction |
|---|---|
| 0 | test `Count < 3`; when false, go to 4 |
| 1 | ask for `mark` |
| 2 | wait 1 |
| 3 | go to 0 |
| 4 | wait 5 |
| 5 | ask for `say` |

While `Count` is below 3 the script asks for `mark` once a tick and stops on instruction 2. On the tick
`Count` is 3 it asks for nothing and stops on instruction 4.

### The budget

`budget` is how many instructions each script may run in one tick: 32 unless the game sets it; below 1
is read as 1. It is what makes a loop that never waits safe: the loop runs a little each tick.

```cpp
.body = {loop({mark()})},     // two instructions a trip: ask, jump back
```

With the budget at 32 this asks for `mark` sixteen times a tick. With it at 5 it asks three times on one
tick, stopping on the jump, and twice on the next.

Leaving a body that has ended costs nothing, so a script whose last instruction spends the budget is
off on that tick, not the next.

### Reading after asking

Once a script has asked for something on a tick — a command or a `set` — it stops when it reaches an
instruction that reads a value, and reads on the next tick. An instruction reads when its condition,
its new value or one of its arguments names a value.

```cpp
.body = {set(Count, Count + 1), when(Count >= 1, {say("one")}).otherwise({say("none")})},
```

| Tick | `Count` handed in | Asked for | Why it stopped |
|---|---|---|---|
| 1 | 0 | `Count` becomes 1 | the `when` reads `Count`, and the script has asked |
| 2 | 1 | `say("one")` | its body ended |

The game writes the change between the two ticks, so the `when` tests the count the script asked for.
Entries that read nothing are not held up: `say("a"), set(Open, true), say("b")` is three requests on
one tick.

## A worked trace

Two scripts on one processor, listed `a` then `b`, with the same priority:

```cpp
const Script a{.name = "a", .body = {say("a1"), wait(1), say("a2"), wait(1), say("a3")}};
const Script b{.name = "b", .interrupts = {&a}, .body = {say("b1"), wait(1), say("b2")}};
```

`a` is started; after the first tick `b` is started too.

| Tick | `a` | `b` | Asked for | `a` is on |
|---|---|---|---|---|
| 1 | runs: asks, reaches its wait | off | `say a1` | 1 |
| — | | started: `a` is now held | | 1 |
| 2 | held | runs: asks, reaches its wait | `say b1` | 1 |
| 3 | held | its wait is over: asks, and ends | `say b2` | 1 |
| 4 | its wait is over: asks, reaches its next wait | off | `say a2` | 3 |
| 5 | asks, and ends | off | `say a3` | — |

`a`'s one-tick wait took three ticks of the clock, because only the ticks `a` was stepped on counted.
On tick 3 `a` is stepped before `b`, so it is still held when its turn comes, and carries on at tick 4.

## Requests

```cpp
std::span<const Request> requests() const noexcept;
void clearRequests() noexcept;
```

`requests()` is what the last `advance` asked for, in the order asked: scripts in stepping order, and
each script's requests in the order it reached them. The view holds until the next `advance` or
`clearRequests`. A request's text argument views the script's own text and is not a copy.

`clearRequests` drops them once they have been applied.
[`applyScripts`](commands.md#the-two-calls-of-the-tick) reads and clears them for every processor a
game has. `Request` itself is in [commands.md](commands.md#change-and-request).

`idle()` is whether no script is running: an advance would do nothing, and `stepScripts` skips the
processor.

## What a script reads and sets

```cpp
std::span<const ValueId> reads() const noexcept;
std::span<const ValueId> reads(const Script& script) const;
std::span<const ValueId> sets(const Script& script) const;
```

| Call | Is |
|---|---|
| `reads()` | Every value any of the processor's scripts reads, each once. It is what to gather and hand to `start` and `advance`. |
| `reads(script)` | Every value `script` reads: in its `when`, its conditions, its `set`s' new values and its commands' arguments, in the order first met; then what the scripts it calls read. |
| `sets(script)` | Every value `script` sets, then what the scripts it calls set. |

The last two throw `std::out_of_range` when `script` is not one of this processor's.

```cpp
const Script inner{.name = "inner", .body = {waitWhile(Busy), set(Speed, 3)}};
const Script outer{.name = "outer", .when = Flag,
                   .body = {move(Way::North, Count + 1), set(Count, Count + Speed), call(inner)}};
```

| | `reads` | `sets` |
|---|---|---|
| `outer` | `Flag`, `Count`, `Speed`, `Busy` | `Count`, `Speed` |
| `inner` | `Busy` | `Speed` |

## Where a script is

| Call | Is |
|---|---|
| `running(script)` | Started and not yet ended, held or not. |
| `suspended(script)` | Running, and held by another running script that interrupts it. |
| `waiting(script)` | Running, and holding when it was last stepped: at a `waitWhile` whose condition held, or in a `wait` with more than one tick still to go. A script that passes through `wait(1)` every tick is not waiting. |
| `position(script)` | The instruction of its own body it is on, from 0; inside a `call`, that call. 0 when it is off. |
| `length(script)` | How many instructions its body is. |
| `depth(script)` | How deep in calls it is: 1 in its own body, 2 inside a `call`, 0 when it is off. |

Asked about a script that is not this processor's, the first three are false and the last three throw
`std::out_of_range`.

These are what a readout draws from. The getting-started program shows, for each processor, the first
script that is running and not held, with a mark per instruction and the one at `position` lit
([getting-started.md](getting-started.md#14-drawing-it-and-the-readout)).

## What an advance reads and writes

| | |
|---|---|
| Reads | the `Values` it is handed, the scripts' compiled instructions, the processor's own state |
| Writes | the processor's own state and its own list of requests |
| Throws | `std::out_of_range`, naming the id, when `values` lacks a value a script reads. Handed every value `reads()` names, it does not throw. |
| Allocates | nothing, once the request list has grown to what the scripts ask for in a tick |

An advance touches no value, no other processor and no global. It may run on any thread, and two
processors may advance at the same moment; no other call is made on a processor while it advances.

`start`, `stop`, `restart` and the calls that say where a script is are for the game's own logic,
between one tick's `applyScripts` and its `stepScripts`.
