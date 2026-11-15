# Concepts

How ScriptKit models a script and the machine that runs it, and where each part lives in a game. The
reference pages are [scripts.md](scripts.md), [script-processor.md](script-processor.md) and
[commands.md](commands.md).

## Contents

- [A script is a rule over a program](#a-script-is-a-rule-over-a-program)
- [Values are names with types](#values-are-names-with-types)
- [Expressions are data](#expressions-are-data)
- [Triggers are the game's](#triggers-are-the-games)
- [Commands are requests](#commands-are-requests)
- [The script processor is a stepped machine](#the-script-processor-is-a-stepped-machine)
- [One tick, in priority order](#one-tick-in-priority-order)
- [A script reads on the tick after it asks](#a-script-reads-on-the-tick-after-it-asks)
- [Holding a script still](#holding-a-script-still)
- [Calls, stop and restart](#calls-stop-and-restart)
- [The two ends of the tick](#the-two-ends-of-the-tick)
- [An advance touches only its own processor](#an-advance-touches-only-its-own-processor)
- [What a script reads and sets](#what-a-script-reads-and-sets)
- [The family shape](#the-family-shape)
- [Where each part lives in a game](#where-each-part-lives-in-a-game)

## A script is a rule over a program

```cpp
const ScriptKit::Script greet{
    .name = "greet",
    .on   = Interact,          // what sets it off
    .when = !Greeted,          // and when that applies
    .body = {                  // the program
        say("Welcome."), waitWhile(MessageShowing),
        set(Greeted, true),
    },
};
```

The header is a rule: a trigger, criteria, a priority, and the scripts it holds still while it runs.
The body is a program: a list of entries run from the first to the last, with branches, loops and
calls.

A script is data. Writing one runs nothing; it is a constant the game declares once, and a script
processor built over it runs it. Because it is ordinary C++ data, a function can build one, an array
can hold six, and a file can hold one each.

## Values are names with types

```cpp
const ScriptKit::Value<bool> Greeted;
const ScriptKit::Value<int>  Score;
const ScriptKit::Value<Lamp> Signal;     // one of the game's own enums
```

A `Value<T>` is a name with a type, and holds no data. What a value is worth lives in the game's state;
each tick, the values a processor's scripts read are gathered into a `Values` and handed to it.

The type is checked where the script is written. `Score + 20` builds and `Greeted + 1` does not;
`when(Greeted, …)` builds and `when(Score, …)` does not — it says `Score > 0`; `Signal` compares only
against a `Lamp`.

## Expressions are data

`!Greeted`, `Score >= 3` and `clamp(PlayerX, 16, 144)` evaluate nothing where they are written. Each
operator returns an `Expr`: a small tree that says what to work out. The processor works it out on the
tick the script reaches it, against the values it was handed on that tick.

So a condition in a loop is tested afresh each trip, a command's argument is as fresh as the tick it is
asked on, and a script's `when` is tested at the moment its trigger fires.

## Triggers are the game's

A `Trigger` is a kind from the game's own enum and, when the kind takes one, a number or a
`Value<bool>`. ScriptKit stores and compares triggers and does nothing else with them. The game decides
that one has happened — a press, a touch, a distance crossed — and says so:

```cpp
host.start(Interact, values);
```

Every script of that processor whose `on` is that trigger and whose `when` holds starts, unless it is
already running. Two scripts on one trigger both start; priority decides which is stepped first, never
whether one runs.

## Commands are requests

```cpp
const ScriptKit::Command Say{"say"};
```

A command has a name and, each time a body uses it, arguments. It does nothing and reads nothing. When
a script reaches one, a `Request` comes out of the processor — the command and its arguments, every
expression among them already worked out — and the game does it.

A script that needs to know its command has finished waits on a value that says so:

```cpp
say("Welcome."), waitWhile(MessageShowing),
walk(Direction::Up, 2), waitWhile(Walking),
```

Whether something should happen is the script's `when`. What the script waits for afterward is its
`waitWhile`. The command itself only asks.

`set(Greeted, true)` is a request too: a `Change`, saying `Greeted` should become true. A script never
writes a value.

## The script processor is a stepped machine

A `ScriptProcessor` is the running state of a set of scripts, usually everything one actor does. When it
is built it turns each body into a flat list of instructions: a `when` and the loops become tests and
jumps. For each running script it keeps the instruction the script is on, the calls it is inside, and
how much of a wait is left. That is plain data, kept in the game's state like anything else there; a
copy of a processor runs on by itself.

Each tick the game calls `advance` once, and every running script runs some instructions and stops.
What stops a script for the tick:

- a `wait`, until its ticks have passed;
- a `waitWhile` whose condition holds;
- the end of its body, which ends it;
- the **budget**: a script runs at most `budget` instructions a tick, 32 unless the game sets it. A loop
  with nothing in it that waits goes round a little each tick and carries on from there the next;
- reading a value after asking for something — [below](#a-script-reads-on-the-tick-after-it-asks).

A script never holds the game still. It waits; the game's tick goes on round it.

## One tick, in priority order

Within one `advance`, running scripts are stepped from the highest `priority` to the lowest, and
scripts of one priority in the order of the processor's list. The requests come out in that order.

So when two scripts ask for the same thing on one tick — two `say`s — the game receives the higher
priority's first. What it does with two is the game's: the getting-started program queues messages, and
lets a shouted one cut in.

## A script reads on the tick after it asks

Everything a script reads on a tick is one of the values gathered before the processor stepped. What a
script asks for on that tick is written by the game afterward. So when a script that has asked for
something reaches an instruction that reads a value, it stops for the tick and reads on the next one —
after its own requests have been written and the values gathered again.

```cpp
set(Presses, Presses + 1),
when(Presses >= 3, { say("Enough!") }),      // tests the count it just asked for
```

```cpp
say("Welcome."),
waitWhile(MessageShowing),                   // holds until the message it asked for is gone
```

Without the rule the first would test the old count, and the second would find no message showing yet
and fall straight through. With it a script sees its own effects in the order it wrote them.

Entries that read nothing follow in the same tick: `say("a"), set(Open, true), say("b")` is three
requests on one tick.

## Holding a script still

```cpp
const ScriptKit::Script answer{.name = "answer", .on = Interact, .interrupts = {&nap}, .body = { … }};
```

A script that lists others in `interrupts` holds them for as long as it runs. A held script is not
stepped: its waits do not count down, and its position does not move. When the last script holding it
ends, it carries on from exactly where it was.

Holding is a relation, not an event. A script is held whenever any running script lists it, so:

- a script started while one that interrupts it is running starts held, and begins when that one ends;
- a script held by two others carries on when both have ended;
- a script that is restarted while held goes back to its start and stays held.

A processor refuses scripts that interrupt each other in a ring, since each would hold the next
forever.

## Calls, stop and restart

`call(other)` runs another script's body and comes back to the entry after the call. The caller is
still the one running script, one level deeper; a wait inside the callee holds it. A script that exists
only to be called has no trigger.

`stop(other)` ends a script wherever it is — running, waiting, or held. The scripts it was holding
carry on. Stopping a script ends whatever it had called.

`restart(other)` puts a script back at its first entry, running. A script that was off starts.

All three name a script on the same processor, and so does `interrupts`. That is what decides which
scripts share a processor: the ones that call, stop, restart or hold each other. Scripts on different
processors work together through values — one sets, the other waits on it.

## The two ends of the tick

```cpp
ScriptKit::applyScripts({&state.host, &state.lamp}, bindings, handlers, state);   // first
// … the game's own logic …
ScriptKit::stepScripts({&state.host, &state.lamp}, bindings, state);              // last
```

- **`stepScripts`**, at the end of the tick, gathers the values each processor's scripts read from the
  game's state, then advances each processor once. All the values are gathered before any processor
  advances, so no processor sees what another did on this tick.
- **`applyScripts`**, at the start of the next, writes what they asked for: each `Change` through the
  value's binding, each command through its handler, processor by processor in the order given.

The game's tick is the only thing that writes its state, and it writes a script's requests in one
place, in an order the game chose. Between the two calls is the game's own logic, and the only time it
touches a processor: to say a trigger happened, to stop or restart a script, to read where one is.

It follows that a request made on one tick is written at the top of the next, and that a script started
on one tick first speaks on the next.

## An advance touches only its own processor

An `advance` reads the values it is handed, the scripts and the processor's own state. It writes the
processor's own state and its own list of requests, and nothing else: no value, no other processor, no
global. Handed every value its scripts read, it does not throw, and once its request list has grown to
what the scripts ask for in a tick it allocates nothing.

So an advance may run on any thread, two processors may advance at the same moment, and the result is
what advancing them one after another gives. `stepScripts` advances them one after another on the
thread that calls it; the stepping falls where the game's state is not being written, which is what
leaves room to spread it across cores.

## What a script reads and sets

A script names the values it reads and sets where it uses them, and nowhere else. When a processor is
built it works out, for each script, every value it reads — in `when`, in conditions, in expressions, in
command arguments — and every value it sets, including through the scripts it calls.

```cpp
host.reads();          // every value any of its scripts reads: what to gather
host.reads(greet);     // what one script reads
host.sets(greet);      // what one script sets
```

`reads()` is what `stepScripts` gathers. The other two are for a readout, an editor or a test that
wants to show or check a script's reach.

## The family shape

The engine's players share one shape: pure data, a pure resolver, and a cursor the game owns.

| Data | The game's cursor | Each tick it resolves |
|---|---|---|
| `Tween<T>` | `TweenPlayer<T>` | a value |
| `Animation` | `AnimationPlayer` | a frame |
| `Curve` | `PathWalker` | a position and a facing |
| `SpritePathNode`s | `SpritePath` | a whole sprite |
| `Script`s | `ScriptProcessor` | what the scripts ask for |

`SpritePath` already has a sequence, a wait node and an interrupt stack for one sprite's movement. A
script processor is that shape for anything: the sequence is a program with branches and loops, the
wait is on a time or on any condition, and what comes out is whatever the game's commands are.

## Where each part lives in a game

| Part | Where it lives |
|---|---|
| Values, triggers, commands | declared once, as constants, where the scripts can see them |
| Scripts | constants; one file each works well |
| `ScriptProcessor`s | the game's state, one per thing that acts |
| `Bindings` and `Handlers` | built once, beside the state they read and write |
| `applyScripts`, `stepScripts` | the first and the last line of the tick |
| Firing triggers | the tick's own logic, between those two |
| What a command does | its handler: ordinary C++ over the game's state |
| What the screen shows | components that read the state the handlers and bindings wrote |
