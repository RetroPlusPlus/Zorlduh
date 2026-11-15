# Getting started

This page builds a complete program on the module: a street crossing run by scripts. A signal cycles
green, yellow and red; three cars in each lane drive through and queue at their line on red; an officer
keeps watch from the far sidewalk. You walk with the arrow keys, WASD or a pad's directions, and press
the button on the pole with Space, Return, Z or a pad's A button when you stand next to it.

There is one way across: press the button, wait for the walk sign, and cross while it is lit. Everything
else goes badly, and every way it goes badly is a script.

The full source is [`examples/getting_started.cpp`](../examples/getting_started.cpp), with what the
scripts are written in at [`examples/crossing.h`](../examples/crossing.h) and the scripts themselves, a
file each, in [`examples/scripts/`](../examples/scripts/). It builds as the
`ScriptKit-example-getting-started` target whenever `SCRIPTKIT_BUILD_EXAMPLES` is on, so what you read
here is what compiles. Its art is in [`examples/assets/`](../examples/assets/).

## Contents

- [1. Build it](#1-build-it)
- [2. Play it](#2-play-it)
- [3. The art](#3-the-art)
- [4. What the scripts are written in](#4-what-the-scripts-are-written-in)
- [5. A first script: the signal](#5-a-first-script-the-signal)
- [6. The button: two scripts on one trigger](#6-the-button-two-scripts-on-one-trigger)
- [7. The cars: one function, six scripts](#7-the-cars-one-function-six-scripts)
- [8. The officer: watching, and arresting](#8-the-officer-watching-and-arresting)
- [9. Backup, and the press that carries on](#9-backup-and-the-press-that-carries-on)
- [10. Officer down: the ambulance](#10-officer-down-the-ambulance)
- [11. The state and the processors](#11-the-state-and-the-processors)
- [12. Bindings and handlers](#12-bindings-and-handlers)
- [13. The tick](#13-the-tick)
- [14. Drawing it, and the readout](#14-drawing-it-and-the-readout)
- [15. Where to go next](#15-where-to-go-next)

## 1. Build it

The module needs the engine as the CMake target `retropp::engine`; the engine's
`docs/guide/build-and-consume.md` covers building it. Add the module and turn its examples on:

```cmake
add_subdirectory(engine)
set(SCRIPTKIT_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(modules/ScriptKit)
```

```sh
cmake -S . -B build
cmake --build build --target ScriptKit-example-getting-started
```

The executable lands in the build tree under `modules/ScriptKit/`. Close the window to quit.

## 2. Play it

| What you do | What happens | The scripts behind it |
|---|---|---|
| Stand by the pole and press | The signal goes yellow, then red; the cars queue; the walk sign lights for four seconds, blinks three times, and the signal's cycle starts over | `walk phase`, `blink`, `pressed` |
| Cross while the walk sign is lit | "Safely across." | `crossed` |
| Step into the street while cars are moving | A car hits you, and you are put back where you started | `struck` |
| Step off the curb on a red without the walk sign | The officer shouts, runs you down and arrests you | `arrest`, `apprehend` |
| Get across without the walk sign | He arrests you on arrival | `arrest on arrival`, `apprehend` |
| Wait once arrested | His backup arrives: a patrol car, lights going, from whichever side is nearer. Every other car holds for it. It drives into you, he gets in, and they speed off | `backup`, `siren` |
| Lead him through moving traffic | A car knocks him down. Once the cars are done with you too, an ambulance comes and two medics carry him aboard | `officer down`, `ambulance`, `siren` |
| Press once he has been taken away | Everyone is put back | `carry on` |

Under the street, three lines say what each script processor is doing: the name of the script it is
stepping first, and a mark for each of that script's instructions with the one it is on lit — white
while the script runs, yellow while it waits.

## 3. The art

[`examples/assets/`](../examples/assets/) holds three sheets of 8×8 cells and twenty palette images.
Open them in any image viewer alongside this page.

| File | Size | Cells, in read order |
|---|---|---|
| `tiles.png` | 56×8 | sidewalk, road, road with a lane line, crosswalk, wall, the top of the message panel, a dark cell |
| `sprites.png` | 112×8 | you, a figure lying down, the officer, a car's rear and front, the pole, a lamp, the walk sign, the don't-walk sign, a readout mark, the ambulance's rear and front, a light bar, a medic |
| `font.png` | 128×24 | `A`–`Z`, `0`–`9`, `. , ! ? ' - :` |

Each sheet is an indexed PNG whose pixels are palette indices, not colors. The street's tiles draw all
four indices; on the other two sheets index 0 is transparent. A palette image is 4×1 pixels, one per
entry, and each thing is drawn through its own: the same lamp cell is green, yellow or red by its
palette, and the same two car cells are an eastbound car, a westbound car and the patrol car.

The light bar is one cell holding two lamps, one drawn in index 2 and the other in index 3.
`siren-red.png` lights the first and darkens the second; `siren-blue.png` does the reverse. Swapping
the palette is the flash.

## 4. What the scripts are written in

[`examples/crossing.h`](../examples/crossing.h) declares three kinds of thing, each once.

**Values**, each with its real type:

```cpp
enum class Lamp : std::uint8_t { Green, Yellow, Red };

inline const ScriptKit::Value<Lamp> Signal;             // the traffic signal's lamp
inline const ScriptKit::Value<bool> WalkLit;            // the walk sign shows "walk"
inline const ScriptKit::Value<bool> MessageShowing;     // a message is on screen, or waiting to be
inline const ScriptKit::Value<int> Presses;             // presses of the button answered so far
inline const std::array<ScriptKit::Value<bool>, kCars> AtLine{};   // each car: it is at its lane's stop line
```

A `Value` holds no data. It is a name with a type: what it is worth is in the program's state, and a
script processor is handed that each tick. The type is what lets the compiler check every script:
`Presses + 1` builds, `WalkLit + 1` does not, and `Signal` compares only against a `Lamp`.

**Triggers**, named by the program's own enum:

```cpp
enum class On : std::uint8_t { Begin, Interact, StepOffCurb, ReachFarSide, HitByCar, OfficerHit, Confirm };
inline constexpr ScriptKit::Trigger Begin{On::Begin};                 // the program starts
inline constexpr ScriptKit::Trigger Interact{On::Interact};           // the button is pressed
inline constexpr ScriptKit::Trigger StepOffCurb{On::StepOffCurb};     // you walk from a sidewalk into the street
```

A trigger is what sets a script off. ScriptKit only compares them; the program's tick decides when one
has happened ([section 13](#13-the-tick)).

**Commands**, each something a script can ask for:

```cpp
enum class Bubble : std::uint8_t { Plain, Shout };

inline const ScriptKit::Command Say{"say"};
inline const ScriptKit::Command Advance{"advance"};
inline ScriptKit::Entry say(std::string_view text, Bubble bubble = Bubble::Plain) { return Say(text, bubble); }
inline ScriptKit::Entry advance(int car) { return Advance(car); }
```

A command does nothing itself. Calling `Say("Hello!", Bubble::Shout)` in a body makes an entry that asks
for it; the program does it when the request comes out of a processor
([section 12](#12-bindings-and-handlers)). The small functions give each command's arguments their types
and defaults, so a body reads `say("Hello!")`.

The header ends its preamble with three lines every script file relies on:

```cpp
using ScriptKit::loop;
using ScriptKit::wait;
using namespace std::chrono_literals;
```

Most of a body's functions are found through their arguments: `set(Signal, …)` finds `ScriptKit::set`
because `Signal` is a ScriptKit type. `loop({…})` and `wait(1)` take a braced list and a bare number,
which name no namespace, so those two are named. The literals are what let a script say `wait(5s)`.

## 5. A first script: the signal

[`scripts/traffic.h`](../examples/scripts/traffic.h):

```cpp
inline const ScriptKit::Script traffic{
    .name = "traffic",
    .on = Begin,
    .body = {loop({
        set(Signal, Lamp::Green), wait(5s),
        set(Signal, Lamp::Yellow), wait(1s),
        set(Signal, Lamp::Red), wait(3s),
    })},
};
```

A script has a header and a body. The header is the rule: `on` says what sets it off. The body is the
program: a list of entries, run from the first to the last.

- **`set(Signal, Lamp::Green)`** asks the program to make `Signal` worth green. The script does not
  write it; the request comes out of the processor and the tick writes it.
- **`wait(5s)`** holds the script for five seconds. The processor turns the time into ticks when it is
  built, through the engine configuration it is built with: 300 at this program's 60 ticks a second.
- **`loop({…})`** goes round until something stops the script.

Writing the script runs nothing. It is a constant, and everything that names it — a processor's list,
another script — refers to it by address.

## 6. The button: two scripts on one trigger

Pressing the button fires `Interact`, and both of these start.

[`scripts/walk_phase.h`](../examples/scripts/walk_phase.h):

```cpp
inline const ScriptKit::Script walkPhase{
    .name = "walk phase",
    .on = Interact,
    .priority = 1,
    .interrupts = {&traffic},
    .body = {
        when(Signal != Lamp::Red, {
            set(Signal, Lamp::Yellow), wait(1s),
            set(Signal, Lamp::Red), wait(1s),
        }),
        set(WalkLit, true), wait(4s),
        call(blink), call(blink), call(blink),
        set(WalkLit, false), wait(1s),
        set(Presses, 0),
        restart(traffic),
    },
};
```

- **`interrupts = {&traffic}`** holds the signal's cycle still for as long as the walk phase runs. A held
  script is not stepped, so the cycle's five-second wait stops counting where it was.
- **`when(Signal != Lamp::Red, {…})`** is a branch: pressed on a red, the walk sign lights at once. The
  condition is an expression, built as data and worked out on the tick the script reaches it.
- **`call(blink)`** runs another script's body and comes back.
  [`blink`](../examples/scripts/blink.h) dims the lit sign for a quarter of a second and lights it for
  another. It has no trigger: it exists to be called.
- **`restart(traffic)`** puts the cycle back at its first entry. The walk phase ends on the next line,
  the cycle is no longer held, and it starts from green.
- **`priority = 1`** steps it before the other script the press starts, so the signal turns before the
  button answers.

[`scripts/pressed.h`](../examples/scripts/pressed.h):

```cpp
inline const ScriptKit::Script pressed{
    .name = "pressed",
    .on = Interact,
    .body = {
        when(WalkLit, {say("Walk now.")})
            .orWhen(Presses >= 2, {say("PRESSING HARDER\nWON'T HELP.", Bubble::Shout)})
            .otherwise({say("Wait for the\nwalk sign.")}),
        set(Presses, Presses + 1),
        waitWhile(MessageShowing),
    },
};
```

- **`when(…).orWhen(…).otherwise(…)`** runs the first arm whose condition holds.
- **`waitWhile(MessageShowing)`** holds the script while a message is up. That is how a script waits
  for something it asked for: `say` only asks, and the script then waits on a value that says the
  message is still there.
- A trigger does not start a script that is already running, so a press while `pressed` is still
  holding is not answered twice. The walk phase forgets the count when it ends, so the third impatient
  press is counted afresh at each crossing.

## 7. The cars: one function, six scripts

[`scripts/drive.h`](../examples/scripts/drive.h):

```cpp
inline ScriptKit::Script drive(std::string_view name, int car) {
    const ScriptKit::Value<bool> atLine = AtLine[static_cast<std::size_t>(car)];
    return {
        .name = name,
        .on = Begin,
        .body = {loop({
            waitWhile((atLine && Signal == Lamp::Red) || AmbulanceOut || PatrolOut),
            advance(car), wait(1),
        })},
    };
}

inline const std::array<ScriptKit::Script, kCars> driving{
    drive("east 1", 0), drive("east 2", 1), drive("east 3", 2),
    drive("west 1", 3), drive("west 2", 4), drive("west 3", 5),
};
```

A script is ordinary C++ data, so a function can build one. Each car gets a script of its own over its
own `AtLine` value, and all six run side by side on one processor.

Each trip of the loop: hold while the car is at its line on a red, or while the ambulance or the patrol
car is out; then ask for one step, and wait a tick. `advance(car)` passes the car's number as the
command's argument. A car past its line on a red drives on, and one short of it drives up to it.

## 8. The officer: watching, and arresting

[`scripts/watch.h`](../examples/scripts/watch.h) is his idle loop: a look each way, a second apart,
forever. Two scripts interrupt it.

[`scripts/arrest.h`](../examples/scripts/arrest.h) and
[`scripts/arrest_on_arrival.h`](../examples/scripts/arrest_on_arrival.h):

```cpp
inline const ScriptKit::Script arrest{
    .name = "arrest",
    .on = StepOffCurb,
    .when = Signal == Lamp::Red && !WalkLit && !Wanted,
    .interrupts = {&watch},
    .body = {call(apprehend)},
};

inline const ScriptKit::Script arrestOnArrival{
    .name = "arrest on arrival",
    .on = ReachFarSide,
    .when = !WalkLit && !Wanted,
    .interrupts = {&watch},
    .body = {call(apprehend)},
};
```

**`when`** on the header is the script's criteria: the trigger starts it only if the expression holds
at that moment. So stepping off the curb sets the officer off on a red without the walk sign, and at no
other time; the cars are left to deal with a green. Reaching the far side without the walk sign sets him
off whatever the signal shows.

Both do the same thing, so both call it.
[`scripts/apprehend.h`](../examples/scripts/apprehend.h):

```cpp
inline const ScriptKit::Script apprehend{
    .name = "apprehend",
    .body = {
        set(Wanted, true),
        say("HEY! STOP!", Bubble::Shout),
        loopWhile(!Caught, {stepToward(PlayerX, PlayerY), wait(1)}),
        set(Arrested, true),
        say("You're under\narrest."), waitWhile(MessageShowing),
        call(backup),
        loop({say("Press to continue."), waitWhile(MessageShowing), wait(2s)}),
    },
};
```

- **`loopWhile(!Caught, {…})`** tests its condition before each trip. `Caught` is a value the program
  works out — the officer is touching you — and the loop ends on the tick it is true.
- **`stepToward(PlayerX, PlayerY)`** hands two expressions to a command. They are evaluated on the tick
  the script asks, so he closes in on where you are now.
- **`set(Wanted, true)`** is what keeps a second arrest from starting, through the `!Wanted` in both
  headers, and what keeps the [reminder](../examples/scripts/reminder.h) quiet while he is after you.

## 9. Backup, and the press that carries on

[`scripts/backup.h`](../examples/scripts/backup.h):

```cpp
inline const ScriptKit::Script backup{
    .name = "backup",
    .body = {
        set(PatrolOut, true),
        restart(siren),
        loopWhile(!PatrolAtPlayer, {drivePatrol(), wait(1)}),
        say("Whoops! We gotta\nget outta here!"), waitWhile(MessageShowing),
        loopWhile(!OfficerAtPatrol, {stepToward(PatrolDoorX, PatrolDoorY), wait(1)}),
        set(OfficerAboard, true),
        wait(500ms),
        loopWhile(!PatrolGone, {drivePatrol(6), wait(1)}),
        stop(siren),
        set(PatrolOut, false),
    },
};
```

Three scripts deep — `arrest` calls `apprehend`, which calls `backup` — this is still one running
script, and the processor reports its depth as 3.

- **`set(PatrolOut, true)`** does two things, in two places. Every car's script holds on that value
  ([section 7](#7-the-cars-one-function-six-scripts)), which is how scripts on different processors
  work together: through values, never by reaching into each other. And the program's binding for it
  places the patrol car at the edge of the screen nearer to you
  ([section 12](#12-bindings-and-handlers)).
- **`restart(siren)`** starts a script that was off. [`siren`](../examples/scripts/siren.h) flips
  `SirenRed` every tenth of a second, side by side with this one, until `stop(siren)`.
- **`drivePatrol()`** and **`drivePatrol(6)`** are one command with a default: two pixels a step on the
  way in, six on the way out.

The patrol car drives into you, which fires `HitByCar` like any other car and starts
[`struck`](../examples/scripts/struck.h) on the signal's processor. Then `apprehend` asks for a press,
over and over, and this answers it. [`scripts/carry_on.h`](../examples/scripts/carry_on.h):

```cpp
inline const ScriptKit::Script carryOn{
    .name = "carry on",
    .on = Confirm,
    .when = OfficerAboard && !AmbulanceOut && !PatrolOut,
    .priority = 3,
    .body = {
        stop(officerDown), stop(arrest), stop(arrestOnArrival),
        sendBack(),
        set(OfficerAboard, false), set(Arrested, false), set(Wanted, false), set(OfficerDown, false),
    },
};
```

`Confirm` fires on every press of the button, wherever you stand. The criteria make it do nothing until
the officer has been driven away. **`stop`** ends a script wherever it is; stopping `arrest` ends the
`apprehend` it is inside.

## 10. Officer down: the ambulance

A car that drives into the officer fires `OfficerHit`.
[`scripts/officer_down.h`](../examples/scripts/officer_down.h):

```cpp
inline const ScriptKit::Script officerDown{
    .name = "officer down",
    .on = OfficerHit,
    .priority = 2,
    .interrupts = {&watch},
    .body = {
        set(OfficerDown, true),
        stop(arrest), stop(arrestOnArrival),
        set(Arrested, false),
        say("OOF!", Bubble::Shout),
        waitWhile(InStreet && !Struck),
        call(ambulance),
        loop({say("Press to continue."), waitWhile(MessageShowing), wait(2s)}),
    },
};
```

His arrest is over and you are free to move. The ambulance stops every car, so
`waitWhile(InStreet && !Struck)` holds it back while you are still in the street and not yet hit: the
cars get their chance at you first.

[`scripts/ambulance.h`](../examples/scripts/ambulance.h) is the longest body in the program, and reads
top to bottom as what happens: it sets off with its siren, drives until it reaches him, puts its two
medics out, walks them to him, has them lift him and carry him back, takes everyone aboard, drives off,
and lets the cars go.

```cpp
        set(MedicsOut, true),
        loopWhile(!MedicsAtOfficer, {walkMedics(Toward::Officer), wait(2)}),
        wait(500ms),
        set(OfficerCarried, true),
        loopWhile(!MedicsAtAmbulance, {walkMedics(Toward::Ambulance), wait(2)}),
        set(OfficerAboard, true), set(OfficerCarried, false), set(MedicsOut, false),
```

Every step of it is one of three things: ask for something, wait a time, or wait on a value that says
how far the thing asked for has got.

## 11. The state and the processors

```cpp
struct State {
    retropp::Vec2i player = kPlayerStart;
    retropp::Vec2i officerAt = kOfficerPost;
    Lamp lamp = Lamp::Green;
    bool walkLit = false;
    // … every other thing the screen shows …

    ScriptKit::ScriptProcessor signal{
        {&traffic, &reminder, &blink, &walkPhase, &pressed, &crossed, &struck}, engineConfig()};
    ScriptKit::ScriptProcessor cars{
        {&driving[0], &driving[1], &driving[2], &driving[3], &driving[4], &driving[5]}, engineConfig()};
    ScriptKit::ScriptProcessor officer{
        {&watch, &apprehend, &backup, &arrest, &arrestOnArrival, &ambulance, &siren, &crossing::officerDown,
         &carryOn},
        engineConfig()};
};
```

A script processor is the running state of a set of scripts: which are running, where each one is, what
each is waiting for. It is plain data and lives in the program's state beside everything else.

There is one per thing that acts. A script can `call`, `stop`, `restart` and `interrupt` only scripts on
its own processor, so the scripts that do that to each other share one: the walk phase and the cycle it
holds, the officer's arrest and his watch. A script a body names that is not in the list is refused
when the processor is built.

The processor is built with the engine configuration, which is where every time in a `wait` becomes
ticks.

## 12. Bindings and handlers

The program says, once, how each value is read from its state and written to it:

```cpp
const ScriptKit::Bindings<State> bindings{
    {Signal, &State::lamp},                                                         // a member: read and written
    {MessageShowing, [](const State& state) { return !state.messages.empty(); }},   // a getter: read only
    {Caught, [](const State& state) { return overlap(grown(box(state.officerAt), 1), box(state.player)); }},
    // Setting off, the patrol car comes in level with you, from whichever side of the screen is nearer.
    {PatrolOut, [](const State& state) { return state.patrolOut; },
     [](State& state, bool out) {
         state.patrolOut = out;
         if (out) {
             state.patrolHeading = state.player.x + kCell / 2 < kScreenWidth / 2 ? 1 : -1;
             state.patrolAt = {.x = state.patrolHeading > 0 ? -kCarLength : kScreenWidth, .y = state.player.y};
         }
     }},
    // …
};
```

A binding takes one of three forms: a member of the value's own type, a getter for a value that is
worked out, or a getter with a setter for a value whose writing does more than store it.

And what it does for each command:

```cpp
const ScriptKit::Handlers<State> handlers{
    {Say, [](State& state, const ScriptKit::Arguments& arguments) {
         const Bubble bubble = arguments.word<Bubble>(1);
         if (bubble == Bubble::Shout) {
             state.messages.clear();
         }
         state.messages.push_back({.text = arguments.text(0), .bubble = bubble});
     }},
    {Advance, [](State& state, const ScriptKit::Arguments& arguments) {
         const int car = arguments.number(0);
         // … move the car a step unless that would close on the car ahead; note who it hit …
     }},
    // …
};
```

A handler reads its arguments by kind: `text`, `number`, `truth`, or `word<E>` for an enumerator.
Everything the world does — a car keeping its distance from the one ahead, a shout cutting in ahead of
the message that was waiting — is in the handlers, in ordinary C++. A script says what should happen and
when; a handler says how.

## 13. The tick

```cpp
void tick(State& state, const retropp::InputState& input) {
    // First: everything the scripts asked for on the last tick is written.
    ScriptKit::applyScripts({&state.signal, &state.cars, &state.officer}, bindings, handlers, state);

    // The program's own logic: messages time out, you walk.
    // …

    // The triggers.
    const auto valuesOf = [&state](const ScriptKit::ScriptProcessor& processor) {
        return bindings.gather(state, processor.reads());
    };
    if (input.justPressed(Action::Press)) {
        state.officer.start(Confirm, valuesOf(state.officer));
        if (overlap(grown(kPole, 2), box(state.player))) {
            state.signal.start(Interact, valuesOf(state.signal));
        }
    }
    if (nowInStreet && !state.inStreet) {
        state.officer.start(StepOffCurb, valuesOf(state.officer));
    }
    // …

    // Last: every processor is handed the values its scripts read, and steps once.
    ScriptKit::stepScripts({&state.signal, &state.cars, &state.officer}, bindings, state);
}
```

Two calls bracket the tick.

- **`applyScripts`**, first, writes what each processor asked for on the last tick: a `set` through its
  binding, a command through its handler, in the order asked. It is the one place a script's effects
  land.
- **`stepScripts`**, last, gathers the values each processor's scripts read and steps each processor
  once.

Between them is the program's own logic, and the only time it touches a processor. **`start(trigger,
values)`** tells one that a trigger has happened: every script of that processor waiting on it whose
`when` holds, and which is not already running, starts. It is handed the values because `when` reads
them.

So a script's request made on one tick is written at the top of the next, and a script started on one
tick first speaks on the next.

## 14. Drawing it, and the readout

Every sprite is drawn from the state: the lamp's palette from `state.lamp`, the walk sign from
`state.walkLit`, the light bar's palette from `state.sirenRed`. What a script changed shows because
the state changed; a script itself draws no part of the screen.

The readout draws from the processors themselves:

```cpp
const ScriptKit::Script* stepped = nullptr;
for (const ScriptKit::Script* script : scripts) {
    if (processor.running(*script) && !processor.suspended(*script)) {
        stepped = script;
        break;
    }
}
// …
const std::size_t position = processor.position(*stepped);
const retropp::PaletteId lit = processor.waiting(*stepped) ? art.textShout : art.text;
for (std::size_t mark = 0; mark < processor.length(*stepped); ++mark) {
    // one mark per instruction, the one at `position` lit
}
```

`running`, `suspended`, `waiting`, `position`, `length` and `depth` say where any script is. Watch the
top line when you press the button: `TRAFFIC` gives way to `WALK PHASE`, whose lit mark walks along
its instructions, and `TRAFFIC` comes back with its mark at the start.

## 15. Where to go next

- The model: rules over programs, the stepped machine, the two ends of the tick —
  [concepts.md](concepts.md).
- Every value, expression and entry, and what does not build — [scripts.md](scripts.md).
- The processor, instruction by instruction, with a worked trace —
  [script-processor.md](script-processor.md).
- Commands, bindings, handlers and the two calls — [commands.md](commands.md).
- Recipes: waiting for something to finish, running a script once, testing a script with no game —
  [how-to.md](how-to.md).
