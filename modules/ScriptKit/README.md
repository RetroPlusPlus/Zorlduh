# ScriptKit — scripts that run a little each tick

`ScriptKit` runs scripts: a script is a rule — what sets it off and when it applies — over a program
written in C++ as a list of entries, and a script processor the game keeps steps every running script a
little each tick, handing back what each one asks for, so the game's tick stays the only thing that
writes. It is for games on the [Polyrhythm](https://github.com/RetroPlusPlus/Polyrhythm) engine.

```cpp
// Values, each with its real type, and a command: something a script can ask the game to do.
const ScriptKit::Value<bool> Greeted;
const ScriptKit::Value<bool> MessageShowing;
const ScriptKit::Command Say{"say"};
ScriptKit::Entry say(std::string_view text) { return Say(text); }

// A script: the rule, then the program.
const ScriptKit::Script greet{
    .name = "greet",
    .on   = Interact,
    .when = !Greeted,
    .body = {
        say("Welcome."), waitWhile(MessageShowing),
        set(Greeted, true),
    },
};

// In the game's state, built once.
ScriptKit::ScriptProcessor host{{&greet}, config};

// Each tick: what the scripts asked for is written, the game runs, and the scripts step.
ScriptKit::applyScripts({&state.host}, bindings, handlers, state);
if (playerPressedInteract) state.host.start(Interact, bindings.gather(state, state.host.reads()));
ScriptKit::stepScripts({&state.host}, bindings, state);
```

| Piece | What it is | Page |
|---|---|---|
| `Value<T>`, `Values` | a typed value a script reads or sets; what the values are worth on one tick | [docs/scripts.md](docs/scripts.md#value) |
| `Expr<T>` | an expression over values, built as data: `Score + 20`, `!Ready && Lamp == Color::Red` | [docs/scripts.md](docs/scripts.md#expr) |
| `Trigger` | what sets a script off, named by the game's own enum | [docs/scripts.md](docs/scripts.md#trigger) |
| `Script` | the rule and the body | [docs/scripts.md](docs/scripts.md#script) |
| `wait`, `waitWhile`, `when`, `loop`, `loopWhile`, `set`, `call`, `stop`, `restart` | the lines of a body | [docs/scripts.md](docs/scripts.md#the-entries) |
| `ScriptProcessor` | the running state of a set of scripts, stepped once per tick | [docs/script-processor.md](docs/script-processor.md) |
| `Command`, `Arguments`, `Request`, `Change` | what a script asks for, and what comes out when it does | [docs/commands.md](docs/commands.md) |
| `Bindings`, `Handlers` | how a game reads and writes each value, and what it does for each command | [docs/commands.md](docs/commands.md#bindings) |
| `applyScripts`, `stepScripts` | the two calls of the tick | [docs/commands.md](docs/commands.md#the-two-calls-of-the-tick) |

The module is header-only, depends on the engine alone (`retropp::engine`, of which it includes
`retropp/engine_config.h` and `retropp/timing.h`), includes no header from the game it sits in, builds no
draw type, and needs C++20. It is meant to leave this repository as a standalone module.

## Using it

```cmake
add_subdirectory(modules/ScriptKit)
target_link_libraries(my-game PRIVATE ScriptKit::ScriptKit)
```

The directory's `CMakeLists.txt` defines the INTERFACE target `ScriptKit` (alias `ScriptKit::ScriptKit`)
and expects the engine as the target `retropp::engine`. `SCRIPTKIT_BUILD_EXAMPLES` (on by default when
the directory is the top-level project) builds `ScriptKit-example-getting-started`, the program
[docs/getting-started.md](docs/getting-started.md) walks through.

## Documentation

**Start with [docs/getting-started.md](docs/getting-started.md)**, then
[docs/concepts.md](docs/concepts.md) for the model and [docs/how-to.md](docs/how-to.md) for recipes. The
full index is [docs/README.md](docs/README.md).

## Layout

```
modules/ScriptKit/
  CMakeLists.txt                    the ScriptKit::ScriptKit target and the example option
  include/ScriptKit/
    value.h                         Value, Values
    expression.h                    Expr, its operators, clamp, min, max, evaluate, valuesOf
    trigger.h                       Trigger
    command.h                       Command, Arguments, Change, Request
    entry.h                         Entry, Body, wait, waitWhile, when, loop, loopWhile, set, call, stop, restart
    script.h                        Script
    script_processor.h              ScriptProcessor
    bindings.h                      Binding, Bindings, Handler, Handlers, applyScripts, stepScripts
  examples/getting_started.cpp      the getting-started program: a street crossing run by scripts
  examples/crossing.h               its values, triggers and commands
  examples/scripts/                 its scripts, a file each
  examples/assets/                  its sheets, font and palette images
  docs/                             the guide
```

`value.h`, `expression.h`, `trigger.h`, `command.h`, `entry.h` and `script.h` include no engine header
at all, so a script and everything it is written in can be declared anywhere. `script_processor.h`
includes the engine's configuration and timing headers, which is where a `wait(500ms)` becomes ticks.
