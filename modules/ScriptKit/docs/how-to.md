# How-to

Recipes for common things a script does. Each is a few lines over the types in
[scripts.md](scripts.md), [script-processor.md](script-processor.md) and [commands.md](commands.md).

## Contents

- [Start two scripts with one trigger](#start-two-scripts-with-one-trigger)
- [Wait for something to finish](#wait-for-something-to-finish)
- [Run a script once](#run-a-script-once)
- [Hold another script still, and let it carry on](#hold-another-script-still-and-let-it-carry-on)
- [Start a script over](#start-a-script-over)
- [Run something alongside, for a while](#run-something-alongside-for-a-while)
- [Share steps between scripts](#share-steps-between-scripts)
- [Build many scripts from one function](#build-many-scripts-from-one-function)
- [Keep a script in a file of its own](#keep-a-script-in-a-file-of-its-own)
- [Make two things on different processors work together](#make-two-things-on-different-processors-work-together)
- [Bind a value the game works out](#bind-a-value-the-game-works-out)
- [Make setting a value do more than store it](#make-setting-a-value-do-more-than-store-it)
- [Show what a processor is doing](#show-what-a-processor-is-doing)
- [Test a script with no game](#test-a-script-with-no-game)

## Start two scripts with one trigger

Give both the same `on`. Every script whose trigger fires and whose `when` holds starts; `priority`
says which is stepped first.

```cpp
const ScriptKit::Script ringBell{.name = "ring bell", .on = Interact, .priority = 1, .body = { … }};
const ScriptKit::Script answer{.name = "answer", .on = Interact, .body = { say("Coming!") }};
```

On the tick after the press, `ring bell`'s requests come out first, then `answer`'s.

## Wait for something to finish

A command only asks. To wait for what it asked for, wait on a value that says it is still going:

```cpp
say("Welcome."), waitWhile(MessageShowing),
walk(Direction::Up, 2), waitWhile(Walking),
```

Bind the value to a getter that reads the game's state:

```cpp
{MessageShowing, [](const State& state) { return !state.messages.empty(); }},
{Walking,        [](const State& state) { return state.host.walk.has_value(); }},
```

The script reads the value on the tick after it asks, once the game has done what was asked, so the
wait does not fall through.

## Run a script once

Put the value the script sets in its own `when`:

```cpp
const ScriptKit::Script unwrap{
    .name = "unwrap",
    .on = Interact,
    .when = !Unwrapped,
    .body = { set(Unwrapped, true), say("A scarf!"), set(HasScarf, true) },
};
```

The game's state remembers that the parcel is unwrapped, as it must anyway to draw it. Once
`Unwrapped` is true, the trigger no longer starts the script.

## Hold another script still, and let it carry on

List it in `interrupts`:

```cpp
const ScriptKit::Script nap{.name = "nap", .on = Begin, .body = { loop({ … }) }};
const ScriptKit::Script answer{
    .name = "answer",
    .on = Interact,
    .interrupts = {&nap},
    .body = { say("Yes?"), waitWhile(MessageShowing) },
};
```

While `answer` runs, `nap` is not stepped: its waits stop counting and it stays on the instruction it
was on. When `answer` ends, `nap` carries on from there.

## Start a script over

```cpp
set(Storm, false),
restart(nap),
```

`restart` puts a script back at its first entry. Used by a script that interrupts the other, the other
starts from the top once the first one ends. `ScriptProcessor::restart` does the same from the game's
own code.

## Run something alongside, for a while

`restart` also starts a script that is off, and `stop` ends it. A script with no trigger, started and
stopped by another, runs alongside for as long as the other wants:

```cpp
const ScriptKit::Script flash{
    .name = "flash",
    .body = { loop({ set(Lit, true), wait(100ms), set(Lit, false), wait(100ms) }) },
};

// In another script on the same processor:
restart(flash),
// … however long the rest takes …
stop(flash),
```

## Share steps between scripts

Put the shared steps in a script with no trigger and `call` it:

```cpp
const ScriptKit::Script celebrate{.name = "celebrate", .body = { say("Well done!"), waitWhile(MessageShowing) }};

const ScriptKit::Script winRace{.name = "win race", .on = CrossLine, .body = { set(Wins, Wins + 1), call(celebrate) }};
const ScriptKit::Script findGem{.name = "find gem", .on = Touch,     .body = { set(Gems, Gems + 1), call(celebrate) }};
```

The callee is in the processor's list with the scripts that call it.

## Build many scripts from one function

A script is a value, so a function returns one:

```cpp
ScriptKit::Script blinker(std::string_view name, ScriptKit::Value<bool> lamp, std::chrono::milliseconds every) {
    return {
        .name = name,
        .on = Begin,
        .body = { loop({ set(lamp, true), wait(every), set(lamp, false), wait(every) }) },
    };
}

const std::array<ScriptKit::Script, 3> blinkers{
    blinker("slow", Lamps[0], 500ms), blinker("medium", Lamps[1], 250ms), blinker("fast", Lamps[2], 100ms),
};

ScriptKit::ScriptProcessor lights{{&blinkers[0], &blinkers[1], &blinkers[2]}, config};
```

Each script gets its own name and its own value. Keep the scripts in an array or in named constants,
so each has an address that lasts: `call`, `stop` and `restart` refuse a temporary.

## Keep a script in a file of its own

Declare the values, triggers and commands in one header as `inline` constants, and each script in a
header of its own that includes it:

```cpp
// town.h
namespace town {
using ScriptKit::loop;
using ScriptKit::wait;
using namespace std::chrono_literals;

inline const ScriptKit::Value<bool> LidOpen;
inline constexpr ScriptKit::Trigger Interact{On::Interact};
inline const ScriptKit::Command Say{"say"};
inline ScriptKit::Entry say(std::string_view text) { return Say(text); }
}
```

```cpp
// scripts/greeter.h
#include "../town.h"
#include "nap.h"             // a script this one names

namespace town {
inline const ScriptKit::Script greeter{
    .name = "greeter",
    .on = Interact,
    .interrupts = {&nap},
    .body = { say("Hello!") },
};
}
```

A script file includes the files of the scripts it names. `inline` makes each constant one object
however many files include it. The getting-started program is laid out this way
([`examples/scripts/`](../examples/scripts/)).

## Make two things on different processors work together

A script reaches only scripts on its own processor. Two processors work together through a value: one
sets it, the other reads it.

```cpp
// On the ambulance's processor:
set(AmbulanceOut, true),
// … the whole run …
set(AmbulanceOut, false),

// On every car's:
loop({ waitWhile(AmbulanceOut), advance(car), wait(1) }),
```

The cars see the value one tick after it is set, when the game has written it.

## Bind a value the game works out

Give the binding a getter. The script reads it like any other value; it has no setter, so a `set` of it
writes nothing:

```cpp
{Heat,      [](const State& state) { return state.stove.heat; }},
{OwnerNear, [](const State& state) { return distance(state.owner, state.cat) < 32; }},
```

```cpp
loopWhile(Heat < 100, { stoke(), wait(1) }),
waitWhile(!OwnerNear),
```

## Make setting a value do more than store it

Give the binding a getter and a setter:

```cpp
{LidOpen, [](const State& state) { return state.lid.open; },
          [](State& state, bool open) {
              state.lid.open = open;
              state.lid.swing = 0;         // the swing animation starts over
          }},
```

A script says `set(LidOpen, true)`. What opening involves stays in the game's code.

## Show what a processor is doing

`running`, `suspended`, `waiting`, `position` and `length` say where a script is:

```cpp
for (const ScriptKit::Script* script : scripts) {
    const std::string_view state = !processor.running(*script)   ? "off"
                                   : processor.suspended(*script) ? "held"
                                   : processor.waiting(*script)   ? "waiting"
                                                                   : "running";
    // script->name, state, processor.position(*script) of processor.length(*script)
}
```

`reads(script)` and `sets(script)` say which values a script reads and sets.

## Test a script with no game

A processor needs an engine configuration for its ticks, a `Values` for what the script reads, and
nothing else. Hand it values built by hand and read what it asks for:

```cpp
retropp::EngineConfig config;
config.timing = retropp::TimingProfile{.tickPeriodNs = retropp::TickPeriodNs::Hz60};

ScriptKit::ScriptProcessor processor{{&unwrap}, config};
EXPECT_EQ(processor.start(Interact, ScriptKit::Values{{Unwrapped, true}}), 0);    // already unwrapped
EXPECT_EQ(processor.start(Interact, ScriptKit::Values{{Unwrapped, false}}), 1);

processor.advance(ScriptKit::Values{{Unwrapped, false}});
const std::span<const ScriptKit::Request> requests = processor.requests();
ASSERT_EQ(requests.size(), 3u);
EXPECT_TRUE(requests[0].change<bool>()->to);                  // set(Unwrapped, true)
EXPECT_EQ(requests[1].command(), &Say);
EXPECT_EQ(requests[1].arguments().text(0), "A scarf!");
```

To play a script forward, write each change it asks for back into the `Values` before the next
advance, as the game's tick would:

```cpp
for (const ScriptKit::Request& request : processor.requests()) {
    if (const auto* change = request.change<bool>()) {
        values.set(change->value, change->to);
    }
}
```

The module's own tests drive the getting-started program's scripts this way, a tick at a time.
