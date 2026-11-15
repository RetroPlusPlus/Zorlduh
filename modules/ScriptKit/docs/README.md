# ScriptKit — guide

The guide to the ScriptKit module, for anyone scripting what the things in a game do on the Polyrhythm
engine. Every page is written against the shipped headers in `include/ScriptKit/`.

**New here? Start with [getting-started.md](getting-started.md)**: a complete program, a street
crossing run by twenty-two scripts, explained part by part. Then read [concepts.md](concepts.md) for the
model, the three reference pages for every type and rule, and [how-to.md](how-to.md) for recipes.

## What the module is

ScriptKit runs scripts. A script is a rule — what sets it off and when it applies — over a program
written in C++ as a list of entries: commands, waits, branches, loops, calls. A script processor the game
keeps in its state steps every running script a little each tick. A script reads the values the
processor is handed and changes nothing itself: everything it wants done comes out of the processor as
a request, and the game's tick writes it. Of the engine it uses two things, the `EngineConfig` a
processor is built with and that configuration's `TimingProfile`, which turns a time into ticks.

What the module takes care of:

- scripts as data, written with designated initializers and checked when a processor is built over
  them;
- typed values and expressions over them, checked by the compiler: an `int` does not meet a `bool`;
- the body's language: `wait` by time or by ticks, `waitWhile`, `when` / `orWhen` / `otherwise`, `loop`,
  `loopWhile`, `set`, `call`, `stop`, `restart`;
- the processor: a budget of instructions per script per tick, priority as stepping order, waiting,
  calls, scripts that hold others still while they run, stop and restart, and where each script is;
- requests: commands with typed arguments and changes of values, in the order asked;
- the game's side as two tables and two calls: `Bindings` for how each value is read and written,
  `Handlers` for what each command does, `applyScripts` and `stepScripts` at the two ends of the tick.

What stays with the game: which values, triggers and commands exist and what each means; deciding
when a trigger has happened; doing what a command asks; and everything on screen.

## Index

### Start here

| Page | Covers |
|---|---|
| [getting-started.md](getting-started.md) | A complete program, a street crossing with a signal, a walk button, six cars, an officer, his backup and an ambulance, built as `ScriptKit-example-getting-started`, explained part by part |
| [concepts.md](concepts.md) | A script as a rule over a program; typed values; expressions as data; commands as requests; the processor as a stepped machine; priority; reading after asking; holding and carrying on; the two ends of the tick; where an advance may run; where each part lives in a game |
| [how-to.md](how-to.md) | Recipes: two scripts on one trigger, wait for something to finish, run a script once, hold another script still, start over, build many scripts from one function, a script per file, bind a computed value, a setter that does more, show what a processor is doing, test a script with no game |

### Reference

| Page | Covers | Header |
|---|---|---|
| [scripts.md](scripts.md) | `Value`, `Values`, `Expr` and its operators, `evaluate`, `valuesOf`, `Trigger`, `Script`, every entry of a body, what does not build | `value.h`, `expression.h`, `trigger.h`, `script.h`, `entry.h` |
| [script-processor.md](script-processor.md) | `ScriptProcessor`: building one and what it refuses, starting, stopping, one tick instruction by instruction, a worked trace, what an advance reads and writes | `script_processor.h` |
| [commands.md](commands.md) | `Command`, `Arguments`, `Request`, `Change`, `Binding`, `Bindings`, `Handler`, `Handlers`, `applyScripts`, `stepScripts` | `command.h`, `bindings.h` |

## Coverage

| Surface | Status | Page |
|---|---|---|
| Scripts written in C++ as data | available | scripts.md |
| Values of type `bool`, `int` and any enum | available | scripts.md |
| Expressions: arithmetic, logic, comparison, `clamp`, `min`, `max` | available | scripts.md |
| Waiting by a time, by ticks, and on a condition | available | scripts.md |
| Branches, loops, calls | available | scripts.md |
| Several scripts running side by side, in priority order | available | script-processor.md |
| A script holding others still while it runs | available | concepts.md, script-processor.md |
| Stopping and restarting a script, from a script or from the game | available | script-processor.md |
| What each script reads and sets, worked out from its body | available | script-processor.md |
| Commands with typed arguments, handled by the game | available | commands.md |
| Reading and writing a game's state through bindings | available | commands.md |
| An advance that touches only its own processor, so it may run on any thread | available | concepts.md, script-processor.md |
| Deciding when a trigger has happened | not provided — the game's; a few lines in its tick ([getting-started.md](getting-started.md#13-the-tick)) | — |
| Stepping processors on other threads | not provided — `stepScripts` advances them one after another on the calling thread | — |
| A text form of scripts, read from a file | not provided | — |
| Values of other types: text, floating point, positions | not provided — a position is two `int` values | — |
| A command that holds a script until it is done | not provided — a script waits on a value ([how-to.md](how-to.md#wait-for-something-to-finish)) | how-to.md |
| Loops that count | not provided — count in a value, or write the entry as many times as needed | — |
| Saving and restoring a processor | not provided — a processor is plain data and copies whole, but has no saved form | — |
