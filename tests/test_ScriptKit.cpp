// The ScriptKit module: typed values and the expressions over them, with what does not build; what a
// script processor works out and refuses when it is built; starting by trigger; one tick of commands,
// waits by count and by time, waitWhile, branches, loops and the budget, set, call, priority,
// interrupts, stop and restart; a script reading on the tick after it asked; bindings, handlers and the
// two calls of the tick; a processor copied; an advance on a thread of its own. The last tests drive
// the scripts of the module's example program, a street crossing — a signal that cycles, a walk
// button, six cars, an officer, his backup, an ambulance — tick by tick with values the test sets.
// Everything else is made up here; no renderer and no platform is used.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "retropp/engine_config.h"
#include "retropp/input_actions.h"
#include "retropp/timing.h"

#include "ScriptKit/bindings.h"
#include "ScriptKit/command.h"
#include "ScriptKit/entry.h"
#include "ScriptKit/expression.h"
#include "ScriptKit/script.h"
#include "ScriptKit/script_processor.h"
#include "ScriptKit/trigger.h"
#include "ScriptKit/value.h"

// The example program's scripts: the last tests drive them.
#include "../modules/ScriptKit/examples/crossing.h"
#include "../modules/ScriptKit/examples/scripts/ambulance.h"
#include "../modules/ScriptKit/examples/scripts/apprehend.h"
#include "../modules/ScriptKit/examples/scripts/arrest.h"
#include "../modules/ScriptKit/examples/scripts/arrest_on_arrival.h"
#include "../modules/ScriptKit/examples/scripts/backup.h"
#include "../modules/ScriptKit/examples/scripts/blink.h"
#include "../modules/ScriptKit/examples/scripts/carry_on.h"
#include "../modules/ScriptKit/examples/scripts/crossed.h"
#include "../modules/ScriptKit/examples/scripts/drive.h"
#include "../modules/ScriptKit/examples/scripts/officer_down.h"
#include "../modules/ScriptKit/examples/scripts/pressed.h"
#include "../modules/ScriptKit/examples/scripts/reminder.h"
#include "../modules/ScriptKit/examples/scripts/siren.h"
#include "../modules/ScriptKit/examples/scripts/struck.h"
#include "../modules/ScriptKit/examples/scripts/traffic.h"
#include "../modules/ScriptKit/examples/scripts/walk_phase.h"
#include "../modules/ScriptKit/examples/scripts/watch.h"

namespace {

using namespace std::chrono_literals;
using ScriptKit::Arguments;
using ScriptKit::Bindings;
using ScriptKit::Body;
using ScriptKit::Command;
using ScriptKit::Entry;
using ScriptKit::Expr;
using ScriptKit::Handlers;
using ScriptKit::Request;
using ScriptKit::Script;
using ScriptKit::ScriptProcessor;
using ScriptKit::Trigger;
using ScriptKit::Value;
using ScriptKit::ValueId;
using ScriptKit::Values;
using ScriptKit::loop;
using ScriptKit::wait;

using Lines = std::vector<std::string>;

// An engine configuration ticking every `periodNs` nanoseconds: 60 Hz unless a test says otherwise.
retropp::EngineConfig configAt(std::int64_t periodNs = 16'666'667) {
    retropp::EngineConfig config;
    config.timing = retropp::TimingProfile{.tickPeriodNs = static_cast<retropp::TickPeriodNs>(periodNs)};
    return config;
}
const retropp::EngineConfig kConfig = configAt();

enum class Tone : std::uint8_t { Plain, Loud };
enum class Way : std::uint8_t { North, South };
enum class Light : std::uint8_t { Green, Yellow, Red };
enum class On : std::uint8_t { Poke, Bump, Near, Becomes };

const Value<bool> Flag, Other, Busy;
const Value<int> Count, Speed;
const Value<Light> Lamp;

constexpr Trigger Poke{On::Poke};
constexpr Trigger Bump{On::Bump};

const Command Say{"say"}, Move{"move"}, Mark{"mark"};
Entry say(std::string_view text, Tone tone = Tone::Plain) { return Say(text, tone); }

// The street crossing is the example program's: its values, triggers and commands in crossing.h, and
// its scripts, a file each. These build its three processors as the example does, and the crossing as
// it stands before anything happens.
ScriptProcessor crossingSignal() {
    return {{&crossing::traffic, &crossing::reminder, &crossing::blink, &crossing::walkPhase, &crossing::pressed,
             &crossing::crossed, &crossing::struck},
            kConfig};
}
ScriptProcessor crossingCars() {
    return {{&crossing::driving[0], &crossing::driving[1], &crossing::driving[2], &crossing::driving[3],
             &crossing::driving[4], &crossing::driving[5]},
            kConfig};
}
ScriptProcessor crossingOfficer() {
    return {{&crossing::watch, &crossing::apprehend, &crossing::backup, &crossing::arrest,
             &crossing::arrestOnArrival, &crossing::ambulance, &crossing::siren, &crossing::officerDown,
             &crossing::carryOn},
            kConfig};
}

// A green signal, nobody in the street, nothing said, no patrol car and no ambulance.
Values calm() {
    Values values{{crossing::Signal, crossing::Lamp::Green},
                  {crossing::WalkLit, false},
                  {crossing::WalkDim, false},
                  {crossing::MessageShowing, false},
                  {crossing::Struck, false},
                  {crossing::Wanted, false},
                  {crossing::Arrested, false},
                  {crossing::Caught, false},
                  {crossing::OfficerDown, false},
                  {crossing::OfficerAboard, false},
                  {crossing::PatrolOut, false},
                  {crossing::PatrolAtPlayer, false},
                  {crossing::PatrolGone, false},
                  {crossing::OfficerAtPatrol, false},
                  {crossing::PatrolDoorX, 40},
                  {crossing::PatrolDoorY, 100},
                  {crossing::AmbulanceOut, false},
                  {crossing::AmbulanceAtOfficer, false},
                  {crossing::AmbulanceGone, false},
                  {crossing::SirenRed, false},
                  {crossing::MedicsOut, false},
                  {crossing::MedicsAtOfficer, false},
                  {crossing::MedicsAtAmbulance, false},
                  {crossing::OfficerCarried, false},
                  {crossing::InStreet, false},
                  {crossing::Presses, 0},
                  {crossing::PlayerX, 80},
                  {crossing::PlayerY, 100}};
    for (const Value<bool>& atLine : crossing::AtLine) {
        values.set(atLine, false);
    }
    return values;
}

// The name a test value is written as in a described request.
std::string nameOf(ValueId id) {
    const std::array<std::pair<ValueId, std::string_view>, 35> names{{
        {Flag.id(), "flag"},
        {Other.id(), "other"},
        {Busy.id(), "busy"},
        {Count.id(), "count"},
        {Speed.id(), "speed"},
        {Lamp.id(), "lamp"},
        {crossing::Signal.id(), "signal"},
        {crossing::WalkLit.id(), "walk lit"},
        {crossing::WalkDim.id(), "walk dim"},
        {crossing::MessageShowing.id(), "message showing"},
        {crossing::Struck.id(), "struck"},
        {crossing::Wanted.id(), "wanted"},
        {crossing::Arrested.id(), "arrested"},
        {crossing::Caught.id(), "caught"},
        {crossing::OfficerDown.id(), "officer down"},
        {crossing::OfficerAboard.id(), "officer aboard"},
        {crossing::PatrolOut.id(), "patrol out"},
        {crossing::PatrolAtPlayer.id(), "patrol at player"},
        {crossing::PatrolGone.id(), "patrol gone"},
        {crossing::OfficerAtPatrol.id(), "officer at patrol"},
        {crossing::PatrolDoorX.id(), "patrol door x"},
        {crossing::PatrolDoorY.id(), "patrol door y"},
        {crossing::AmbulanceOut.id(), "ambulance out"},
        {crossing::AmbulanceAtOfficer.id(), "ambulance at officer"},
        {crossing::AmbulanceGone.id(), "ambulance gone"},
        {crossing::InStreet.id(), "in street"},
        {crossing::SirenRed.id(), "siren red"},
        {crossing::MedicsOut.id(), "medics out"},
        {crossing::MedicsAtOfficer.id(), "medics at officer"},
        {crossing::MedicsAtAmbulance.id(), "medics at ambulance"},
        {crossing::OfficerCarried.id(), "officer carried"},
        {crossing::AtLine[0].id(), "at line"},
        {crossing::Presses.id(), "presses"},
        {crossing::PlayerX.id(), "player x"},
        {crossing::PlayerY.id(), "player y"},
    }};
    for (const auto& [known, name] : names) {
        if (known == id) {
            return std::string(name);
        }
    }
    return "value " + std::to_string(id);
}

// A request as one line: "say hello", "move south 5", "set count 8".
std::string describe(const Request& request) {
    if (const Command* command = request.command()) {
        const Arguments& arguments = request.arguments();
        std::string line(command->name());
        if (command == &Say) {
            line += " " + std::string(arguments.text(0));
        } else if (command == &Move) {
            line += arguments.word<Way>(0) == Way::North ? " north " : " south ";
            line += std::to_string(arguments.number(1));
        } else if (command == &crossing::Say) {
            line += " " + std::string(arguments.text(0));
            if (arguments.word<crossing::Bubble>(1) == crossing::Bubble::Shout) {
                line += " (shout)";
            }
        } else if (command == &crossing::Face) {
            line += arguments.word<retropp::Dir>(0) == retropp::Dir::Left ? " left" : " right";
        } else if (command == &crossing::Advance) {
            line += " " + std::to_string(arguments.number(0));
        } else if (command == &crossing::DrivePatrol) {
            line += " " + std::to_string(arguments.number(0));
        } else if (command == &crossing::WalkMedics) {
            line += arguments.word<crossing::Toward>(0) == crossing::Toward::Officer ? " officer" : " ambulance";
        } else if (command == &crossing::StepToward) {
            line += " " + std::to_string(arguments.number(0)) + " " + std::to_string(arguments.number(1));
        }
        return line;
    }
    if (const auto* change = request.change<bool>()) {
        return "set " + nameOf(change->value.id()) + (change->to ? " true" : " false");
    }
    if (const auto* change = request.change<int>()) {
        return "set " + nameOf(change->value.id()) + " " + std::to_string(change->to);
    }
    if (const auto* change = request.change<Light>()) {
        return "set " + nameOf(change->value.id()) + " " + std::to_string(static_cast<int>(change->to));
    }
    if (const auto* change = request.change<crossing::Lamp>()) {
        switch (change->to) {
        case crossing::Lamp::Green:
            return "set signal green";
        case crossing::Lamp::Yellow:
            return "set signal yellow";
        case crossing::Lamp::Red:
            return "set signal red";
        }
    }
    return "?";
}

// What the processor's last advance asked for.
Lines asked(const ScriptProcessor& processor) {
    Lines lines;
    for (const Request& request : processor.requests()) {
        lines.push_back(describe(request));
    }
    return lines;
}

// One tick: advances the processor and returns what it asked for.
Lines tick(ScriptProcessor& processor, const Values& values = {}) {
    processor.advance(values);
    return asked(processor);
}

// Writes every Change the processor's last advance asked for into `values`, as a game's tick would
// before the next one.
void feed(Values& values, const ScriptProcessor& processor) {
    for (const Request& request : processor.requests()) {
        if (const auto* change = request.change<bool>()) {
            values.set(change->value, change->to);
        } else if (const auto* change = request.change<int>()) {
            values.set(change->value, change->to);
        } else if (const auto* change = request.change<Light>()) {
            values.set(change->value, change->to);
        } else if (const auto* change = request.change<crossing::Lamp>()) {
            values.set(change->value, change->to);
        }
    }
}

// One tick that feeds its changes back: what a game's tick does between two advances.
Lines fedTick(ScriptProcessor& processor, Values& values) {
    processor.advance(values);
    feed(values, processor);
    return asked(processor);
}

// `lines` without the siren's, which flashes all the way through the ambulance's run.
Lines quiet(Lines lines) {
    std::erase_if(lines, [](const std::string& line) { return line.starts_with("set siren"); });
    return lines;
}

// The next thing the processor asks for other than the siren, fed back, within `limit` ticks.
Lines nextAsked(ScriptProcessor& processor, Values& values, int limit = 200) {
    for (int n = 0; n < limit; ++n) {
        Lines lines = quiet(fedTick(processor, values));
        if (!lines.empty()) {
            return lines;
        }
    }
    return {};
}

// The first tick, counting from 1, on which the processor asks for anything; 0 when it asks for
// nothing within `limit` ticks.
int firstAsk(ScriptProcessor& processor, const Values& values = {}, int limit = 1000) {
    for (int n = 1; n <= limit; ++n) {
        if (!tick(processor, values).empty()) {
            return n;
        }
    }
    return 0;
}

// What building a processor over `scripts` throws, or an empty string when it builds.
std::string refusal(std::vector<const Script*> scripts) {
    try {
        const ScriptProcessor processor{std::move(scripts), kConfig};
    } catch (const std::logic_error& error) {
        return error.what();
    }
    return {};
}

bool contains(const std::string& text, std::string_view part) { return text.find(part) != std::string::npos; }

// What builds and what does not.
template <class A, class B>
concept CanSet = requires(A a, B b) { ScriptKit::set(a, b); };
template <class A>
concept CanWhen = requires(A a) { ScriptKit::when(a, Body{}); };
template <class A, class B>
concept CanAdd = requires(A a, B b) { a + b; };
template <class A, class B>
concept CanAnd = requires(A a, B b) { a && b; };
template <class A, class B>
concept CanCompare = requires(A a, B b) { a == b; };
template <class A>
concept CanAsk = requires(const Command& command, A a) { command(a); };
template <class A>
concept CanWait = requires(A a) { ScriptKit::wait(a); };

struct World {
    bool flag = false;
    int count = 0;
    Light lamp = Light::Green;
    int x = 0;
    bool busy = false;
    int busyWrites = 0;
    mutable int speedReads = 0;
    Lines log;
};

const Bindings<World> kBindings{
    {Flag, &World::flag},
    {Count, &World::count},
    {Lamp, &World::lamp},
    {Speed, [](const World& world) {
         ++world.speedReads;
         return world.x * 2;
     }},
    {Busy, [](const World& world) { return world.busy; },
     [](World& world, bool busy) {
         world.busy = busy;
         ++world.busyWrites;
     }},
};

const Handlers<World> kHandlers{
    {Say, [](World& world, const Arguments& arguments) {
         world.log.push_back("say " + std::string(arguments.text(0)) +
                             (arguments.word<Tone>(1) == Tone::Loud ? "!" : ""));
     }},
    {Move, [](World& world, const Arguments& arguments) {
         world.log.push_back(std::string("move ") + (arguments.word<Way>(0) == Way::North ? "north " : "south ") +
                             std::to_string(arguments.number(1)));
     }},
};

// Scripts that name themselves or each other, declared before they are defined.
extern const Script quitter;
const Script quitter{.name = "quitter", .body = {say("one"), stop(quitter), say("never")}};
extern const Script again;
const Script again{.name = "again", .body = {say("again"), restart(again), say("never")}};
extern const Script echo;
const Script echo{.name = "echo", .body = {call(echo)}};
extern const Script ringB;
const Script ringA{.name = "ring a", .interrupts = {&ringB}};
const Script ringB{.name = "ring b", .interrupts = {&ringA}};

TEST(ScriptKit, EachValueHasItsOwnIdAndAValuesHoldsWhatEachIsWorth) {
    EXPECT_NE(Flag.id(), 0u);
    EXPECT_NE(Flag.id(), Other.id());
    EXPECT_NE(Count.id(), Speed.id());
    const Value<int> copy = Count;
    EXPECT_EQ(copy.id(), Count.id());

    Values values{{Flag, true}, {Count, 3}, {Lamp, Light::Red}};
    EXPECT_TRUE(values.get(Flag));
    EXPECT_EQ(values.get(Count), 3);
    EXPECT_EQ(values.get(Lamp), Light::Red);
    EXPECT_TRUE(values.has(Count.id()));
    EXPECT_FALSE(values.has(Speed.id()));

    values.set(Count, 9);
    EXPECT_EQ(values.get(Count), 9);
    EXPECT_EQ(values.size(), 3u);

    try {
        (void)values.get(Speed);
        FAIL() << "a value never set was read";
    } catch (const std::out_of_range& error) {
        EXPECT_TRUE(contains(error.what(), std::to_string(Speed.id())));
    }
}

TEST(ScriptKit, IntExpressionsAddSubtractMultiplyDivideClampAndWrap) {
    const Values values{{Count, 7}, {Speed, 2}};
    EXPECT_EQ(evaluate(Count + 2, values), 9);
    EXPECT_EQ(evaluate(2 + Count, values), 9);
    EXPECT_EQ(evaluate(Count - Speed, values), 5);
    EXPECT_EQ(evaluate(Count * Speed, values), 14);
    EXPECT_EQ(evaluate(Count / Speed, values), 3);
    EXPECT_EQ(evaluate(Count / 0, values), 0);
    EXPECT_EQ(evaluate(-Count, values), -7);
    EXPECT_EQ(evaluate((Count + 1) * 2, values), 16);
    EXPECT_EQ(evaluate(clamp(Count, 0, 5), values), 5);
    EXPECT_EQ(evaluate(clamp(Count, 10, 20), values), 10);
    EXPECT_EQ(evaluate(clamp(Count, 0, 10), values), 7);
    EXPECT_EQ(evaluate(min(Count, Speed), values), 2);
    EXPECT_EQ(evaluate(max(Count, 3), values), 7);

    const Values most{{Count, INT_MAX}};
    EXPECT_EQ(evaluate(Count + 1, most), INT_MIN);
}

TEST(ScriptKit, BoolExpressionsNegateJoinAndCompare) {
    const Values values{{Flag, true}, {Other, false}, {Count, 7}, {Speed, 2}, {Lamp, Light::Yellow}};
    EXPECT_FALSE(evaluate(!Flag, values));
    EXPECT_FALSE(evaluate(Flag && Other, values));
    EXPECT_TRUE(evaluate(Flag || Other, values));
    EXPECT_TRUE(evaluate(Count < 8, values));
    EXPECT_TRUE(evaluate(Count <= 7, values));
    EXPECT_FALSE(evaluate(Count > 7, values));
    EXPECT_FALSE(evaluate(Count >= 8, values));
    EXPECT_TRUE(evaluate(Count == 7, values));
    EXPECT_FALSE(evaluate(Count != 7, values));
    EXPECT_FALSE(evaluate(Flag == Other, values));
    EXPECT_TRUE(evaluate(Flag != Other, values));
    EXPECT_TRUE(evaluate(Flag == true, values));
    EXPECT_FALSE(evaluate(Lamp == Light::Red, values));
    EXPECT_TRUE(evaluate(Lamp != Light::Red, values));
    EXPECT_TRUE(evaluate(Lamp == Light::Yellow && !Other, values));

    // Every value an expression reads, each once, in the order first read.
    const std::vector<ValueId> read = valuesOf((Count + Speed) * Count > 3 && Flag);
    EXPECT_EQ(read, (std::vector<ValueId>{Count.id(), Speed.id(), Flag.id()}));
    EXPECT_TRUE(valuesOf(Expr<int>(4)).empty());
}

TEST(ScriptKit, OnlyMatchingTypesBuild) {
    EXPECT_TRUE((CanSet<Value<bool>, bool>));
    EXPECT_TRUE((CanSet<Value<int>, Expr<int>>));
    EXPECT_TRUE((CanSet<Value<Light>, Light>));
    EXPECT_FALSE((CanSet<Value<bool>, int>));
    EXPECT_FALSE((CanSet<Value<int>, Value<bool>>));
    EXPECT_FALSE((CanSet<Value<Light>, int>));

    EXPECT_TRUE(CanWhen<Value<bool>>);
    EXPECT_TRUE(CanWhen<Expr<bool>>);
    EXPECT_FALSE(CanWhen<Value<int>>);
    EXPECT_FALSE(CanWhen<int>);

    EXPECT_TRUE((CanAdd<Value<int>, int>));
    EXPECT_TRUE((CanAdd<int, Expr<int>>));
    EXPECT_FALSE((CanAdd<Value<bool>, int>));
    EXPECT_FALSE((CanAdd<Value<Light>, int>));

    EXPECT_TRUE((CanAnd<Value<bool>, Expr<bool>>));
    EXPECT_FALSE((CanAnd<Value<int>, Value<bool>>));

    EXPECT_TRUE((CanCompare<Value<Light>, Light>));
    EXPECT_FALSE((CanCompare<Value<Light>, Way>));
    EXPECT_FALSE((CanCompare<Value<int>, Value<bool>>));

    EXPECT_TRUE(CanAsk<int>);
    EXPECT_TRUE(CanAsk<std::string_view>);
    EXPECT_TRUE(CanAsk<Way>);
    EXPECT_TRUE(CanAsk<Expr<bool>>);
    EXPECT_FALSE(CanAsk<double>);
    EXPECT_FALSE(CanAsk<std::string>);
    EXPECT_FALSE(CanAsk<Expr<Light>>);
}

TEST(ScriptKit, AProcessorKnowsWhatEachScriptReadsAndSets) {
    const Script inner{.name = "inner", .body = {waitWhile(Busy), set(Speed, 3)}};
    const Script outer{.name = "outer", .when = Flag,
                       .body = {Move(Way::North, Count + 1), set(Count, Count + Speed), call(inner),
                                when(Other, {set(Lamp, Light::Red)})}};
    const Script plain{.name = "plain", .body = {say("nothing"), wait(3)}};
    const ScriptProcessor processor{{&outer, &inner, &plain}, kConfig};
    const auto ids = [](std::span<const ValueId> span) { return std::vector<ValueId>(span.begin(), span.end()); };

    // What a script reads, in its `when`, its conditions, its expressions and its commands' arguments,
    // and what it sets: its own first, then those of the scripts it calls, each once.
    EXPECT_EQ(ids(processor.reads(outer)), (std::vector<ValueId>{Flag.id(), Count.id(), Speed.id(), Other.id(), Busy.id()}));
    EXPECT_EQ(ids(processor.sets(outer)), (std::vector<ValueId>{Count.id(), Lamp.id(), Speed.id()}));
    EXPECT_EQ(ids(processor.reads(inner)), std::vector<ValueId>{Busy.id()});
    EXPECT_EQ(ids(processor.sets(inner)), std::vector<ValueId>{Speed.id()});
    EXPECT_TRUE(processor.reads(plain).empty());
    EXPECT_TRUE(processor.sets(plain).empty());

    // What to hand the processor: everything any of its scripts reads.
    EXPECT_EQ(ids(processor.reads()), (std::vector<ValueId>{Flag.id(), Count.id(), Speed.id(), Other.id(), Busy.id()}));

    const Script stranger{.name = "stranger"};
    EXPECT_FALSE(processor.running(stranger));
    EXPECT_FALSE(processor.suspended(stranger));
    EXPECT_FALSE(processor.waiting(stranger));
    EXPECT_THROW((void)processor.reads(stranger), std::out_of_range);
    EXPECT_THROW((void)processor.sets(stranger), std::out_of_range);
    EXPECT_THROW((void)processor.position(stranger), std::out_of_range);
    EXPECT_THROW((void)processor.length(stranger), std::out_of_range);
    EXPECT_THROW((void)processor.depth(stranger), std::out_of_range);
}

TEST(ScriptKit, AProcessorRefusesScriptsItCannotRunTogether) {
    const Script outsider{.name = "outsider"};
    const Script stopper{.name = "stopper", .body = {stop(outsider)}};
    EXPECT_TRUE(contains(refusal({&stopper}), "'stopper'"));
    EXPECT_TRUE(contains(refusal({&stopper}), "'outsider'"));
    EXPECT_EQ(refusal({&stopper, &outsider}), "");

    const Script restarter{.name = "restarter", .body = {restart(outsider)}};
    EXPECT_TRUE(contains(refusal({&restarter}), "restarts 'outsider'"));
    const Script caller{.name = "caller", .body = {call(outsider)}};
    EXPECT_TRUE(contains(refusal({&caller}), "calls 'outsider'"));
    const Script holder{.name = "holder", .interrupts = {&outsider}};
    EXPECT_TRUE(contains(refusal({&holder}), "interrupts 'outsider'"));

    const Script twin{.name = "outsider"};
    EXPECT_TRUE(contains(refusal({&outsider, &twin}), "two scripts are named 'outsider'"));

    EXPECT_TRUE(contains(refusal({&echo}), "'echo' calls itself"));
    EXPECT_TRUE(contains(refusal({&ringA, &ringB}), "'ring a' interrupts itself"));
    EXPECT_TRUE(contains(refusal({&outsider, nullptr}), "a null script"));
}

TEST(ScriptKit, ATriggerStartsEveryMatchingScriptWhoseCriteriaHold) {
    const Script a{.name = "a", .on = Poke, .body = {wait(5)}};
    const Script b{.name = "b", .on = Poke, .when = Flag, .body = {wait(5)}};
    const Script c{.name = "c", .on = Bump, .body = {wait(5)}};
    const Script d{.name = "d", .on = Trigger{On::Near, 48}, .body = {wait(5)}};
    const Script e{.name = "e", .body = {wait(5)}};
    ScriptProcessor processor{{&a, &b, &c, &d, &e}, kConfig};
    EXPECT_TRUE(processor.idle());

    EXPECT_EQ(processor.start(Poke, Values{{Flag, false}}), 1);
    EXPECT_TRUE(processor.running(a));
    EXPECT_FALSE(processor.running(b));
    EXPECT_FALSE(processor.running(c));
    EXPECT_FALSE(processor.idle());

    // A running script is not started again; the one whose criteria now hold is.
    EXPECT_EQ(processor.start(Poke, Values{{Flag, true}}), 1);
    EXPECT_TRUE(processor.running(b));

    EXPECT_EQ(processor.start(Trigger{On::Near, 32}, {}), 0);
    EXPECT_FALSE(processor.running(d));
    EXPECT_EQ(processor.start(Trigger{On::Near, 48}, {}), 1);
    EXPECT_TRUE(processor.running(d));

    // A default trigger is no trigger: it starts nothing, a script without one included.
    EXPECT_EQ(processor.start(Trigger{}, {}), 0);
    EXPECT_FALSE(processor.running(e));
    processor.start(e);
    EXPECT_TRUE(processor.running(e));

    EXPECT_FALSE((Trigger{On::Becomes, Flag} == Trigger{On::Becomes, Other}));
    EXPECT_TRUE((Trigger{On::Becomes, Flag} == Trigger{On::Becomes, Flag}));
    EXPECT_FALSE(Trigger{}.set());
    EXPECT_TRUE(Poke.set());

    ScriptProcessor fresh{{&b}, kConfig};
    EXPECT_THROW((void)fresh.start(Poke, Values{}), std::out_of_range);
    const Script stranger{.name = "stranger"};
    EXPECT_THROW(fresh.start(stranger), std::logic_error);
}

TEST(ScriptKit, OneTickRunsCommandsInOrderWithTheirArguments) {
    const Script script{.name = "three",
                        .body = {Move(Way::South, Count + 1), say("hello", Tone::Loud), Mark(7, true)}};
    ScriptProcessor processor{{&script}, kConfig};
    processor.start(script);
    processor.advance(Values{{Count, 4}});

    const std::span<const Request> requests = processor.requests();
    ASSERT_EQ(requests.size(), 3u);
    EXPECT_EQ(requests[0].command(), &Move);
    ASSERT_EQ(requests[0].arguments().size(), 2u);
    EXPECT_EQ(requests[0].arguments().word<Way>(0), Way::South);
    EXPECT_EQ(requests[0].arguments().number(1), 5);
    EXPECT_EQ(requests[1].command(), &Say);
    EXPECT_EQ(requests[1].command()->name(), "say");
    EXPECT_EQ(requests[1].arguments().text(0), "hello");
    EXPECT_EQ(requests[1].arguments().word<Tone>(1), Tone::Loud);
    EXPECT_EQ(requests[2].command()->id(), Mark.id());
    EXPECT_EQ(requests[2].arguments().number(0), 7);
    EXPECT_TRUE(requests[2].arguments().truth(1));
    EXPECT_EQ(requests[0].change<int>(), nullptr);

    // The body ended, so the script is off.
    EXPECT_FALSE(processor.running(script));
    EXPECT_TRUE(processor.idle());
    EXPECT_TRUE(tick(processor).empty());
}

TEST(ScriptKit, AWaitOfTicksHoldsThatManyTicks) {
    const Script three{.name = "three", .body = {wait(3), say("x")}};
    ScriptProcessor processor{{&three}, kConfig};
    processor.start(three);
    EXPECT_TRUE(tick(processor).empty());
    EXPECT_TRUE(processor.waiting(three));
    EXPECT_TRUE(tick(processor).empty());
    EXPECT_TRUE(tick(processor).empty());
    EXPECT_EQ(tick(processor), Lines{"say x"});

    // A wait of nothing, or of less, moves straight on.
    const Script none{.name = "none", .body = {say("a"), wait(0), say("b"), wait(-4), say("c")}};
    ScriptProcessor quick{{&none}, kConfig};
    quick.start(none);
    EXPECT_EQ(tick(quick), (Lines{"say a", "say b", "say c"}));
}

TEST(ScriptKit, AWaitOfTimeBecomesTicksThroughTheConfigTheProcessorIsBuiltWith) {
    const Script half{.name = "half", .body = {wait(500ms), say("x")}};
    ScriptProcessor sixty{{&half}, kConfig};
    sixty.start(half);
    EXPECT_EQ(firstAsk(sixty), 31);   // held 30 ticks at 60 Hz

    ScriptProcessor fifty{{&half}, configAt(20'000'000)};
    fifty.start(half);
    EXPECT_EQ(firstAsk(fifty), 26);   // held 25 ticks at 50 Hz

    // What the engine's own conversion returns is a count of ticks.
    const Script converted{.name = "converted", .body = {wait(kConfig.timing.ticksForDuration(500ms)), say("x")}};
    ScriptProcessor same{{&converted}, kConfig};
    same.start(converted);
    EXPECT_EQ(firstAsk(same), 31);

    // A time that comes to no ticks is a wait of nothing.
    const Script brief{.name = "brief", .body = {wait(1ms), say("x")}};
    ScriptProcessor at{{&brief}, kConfig};
    at.start(brief);
    EXPECT_EQ(firstAsk(at), 1);

    EXPECT_TRUE(CanWait<int>);
    EXPECT_TRUE(CanWait<std::uint64_t>);
    EXPECT_TRUE(CanWait<std::chrono::milliseconds>);
    EXPECT_TRUE(CanWait<std::chrono::seconds>);
    EXPECT_FALSE(CanWait<double>);
    EXPECT_FALSE(CanWait<const char*>);
    EXPECT_FALSE(CanWait<bool>);
    EXPECT_FALSE(CanWait<std::chrono::duration<double>>);
}

TEST(ScriptKit, WaitWhileHoldsWhileItsConditionHolds) {
    const Script script{.name = "holds", .body = {say("go"), waitWhile(Busy), say("done")}};
    ScriptProcessor processor{{&script}, kConfig};
    processor.start(script);

    EXPECT_EQ(tick(processor, Values{{Busy, false}}), Lines{"say go"});   // asked, so it reads next tick
    EXPECT_TRUE(tick(processor, Values{{Busy, true}}).empty());
    EXPECT_TRUE(processor.waiting(script));
    EXPECT_EQ(processor.position(script), 1u);
    EXPECT_TRUE(tick(processor, Values{{Busy, true}}).empty());
    EXPECT_EQ(tick(processor, Values{{Busy, false}}), Lines{"say done"});
    EXPECT_FALSE(processor.running(script));
}

TEST(ScriptKit, AScriptThatAskedReadsOnTheNextTick) {
    // The count it tests is the one it asked for, once the game has written it.
    const Script counts{.name = "counts",
                        .body = {set(Count, Count + 1),
                                 when(Count >= 1, {say("one")}).otherwise({say("none")})}};
    ScriptProcessor processor{{&counts}, kConfig};
    processor.start(counts);
    Values values{{Count, 0}};
    EXPECT_EQ(fedTick(processor, values), Lines{"set count 1"});
    EXPECT_TRUE(processor.running(counts));
    EXPECT_FALSE(processor.waiting(counts));
    EXPECT_EQ(fedTick(processor, values), Lines{"say one"});

    // Entries that read nothing follow in the same tick.
    const Script plain{.name = "plain",
                       .body = {say("a"), set(Flag, true), say("b"), set(Count, 2)}};
    ScriptProcessor quick{{&plain}, kConfig};
    quick.start(plain);
    EXPECT_EQ(tick(quick), (Lines{"say a", "set flag true", "say b", "set count 2"}));
}

TEST(ScriptKit, WhenRunsTheFirstArmThatHoldsOnTheTickItIsReached) {
    const Script script{.name = "sign",
                        .body = {when(Count < 0, {say("negative")})
                                     .orWhen(Count == 0, {say("zero")})
                                     .otherwise({say("positive")}),
                                 say("end")}};
    ScriptProcessor processor{{&script}, kConfig};
    processor.start(script);
    EXPECT_EQ(tick(processor, Values{{Count, -1}}), (Lines{"say negative", "say end"}));
    processor.start(script);
    EXPECT_EQ(tick(processor, Values{{Count, 0}}), (Lines{"say zero", "say end"}));
    processor.start(script);
    EXPECT_EQ(tick(processor, Values{{Count, 5}}), (Lines{"say positive", "say end"}));

    // A `when` with one arm and nothing after it.
    const Script lone{.name = "lone", .body = {when(Flag, {say("yes")})}};
    ScriptProcessor single{{&lone}, kConfig};
    single.start(lone);
    EXPECT_TRUE(tick(single, Values{{Flag, false}}).empty());
    EXPECT_FALSE(single.running(lone));

    // The value is read when the script gets there, not when it starts.
    const Script late{.name = "late",
                      .body = {wait(2), when(Flag, {say("yes")}).otherwise({say("no")})}};
    ScriptProcessor waits{{&late}, kConfig};
    waits.start(late);
    EXPECT_TRUE(tick(waits, Values{{Flag, false}}).empty());
    EXPECT_TRUE(tick(waits, Values{{Flag, false}}).empty());
    EXPECT_EQ(tick(waits, Values{{Flag, true}}), Lines{"say yes"});
}

TEST(ScriptKit, ALoopGoesRoundOncePerItsWaitsOrAsFarAsTheBudgetEachTick) {
    const Script paced{.name = "paced", .body = {loop({say("tick"), wait(2)})}};
    ScriptProcessor processor{{&paced}, kConfig};
    processor.start(paced);
    EXPECT_EQ(tick(processor), Lines{"say tick"});
    EXPECT_TRUE(tick(processor).empty());
    EXPECT_EQ(tick(processor), Lines{"say tick"});
    EXPECT_TRUE(tick(processor).empty());
    EXPECT_EQ(tick(processor), Lines{"say tick"});

    // A loop that never waits: a command and the jump back are two instructions a trip.
    const Script spins{.name = "spins", .body = {loop({Mark()})}};
    ScriptProcessor spinner{{&spins}, kConfig};
    EXPECT_EQ(spinner.length(spins), 2u);
    spinner.start(spins);
    EXPECT_EQ(tick(spinner).size(), 16u);   // the default budget of 32
    EXPECT_EQ(spinner.position(spins), 0u);

    spinner.budget = 5;
    EXPECT_EQ(tick(spinner).size(), 3u);
    EXPECT_EQ(spinner.position(spins), 1u);   // stopped on the jump back
    EXPECT_EQ(tick(spinner).size(), 2u);
    EXPECT_EQ(spinner.position(spins), 0u);

    spinner.budget = 0;   // read as 1
    EXPECT_EQ(tick(spinner).size(), 1u);
    EXPECT_EQ(spinner.position(spins), 1u);
}

TEST(ScriptKit, LoopWhileEndsOnTheTickItsConditionFails) {
    const Script script{.name = "counts",
                        .body = {loopWhile(Count < 3, {Mark(), wait(1)}), wait(5), say("out")}};
    ScriptProcessor processor{{&script}, kConfig};
    EXPECT_EQ(processor.length(script), 6u);   // test, mark, wait, jump back; wait; say
    processor.start(script);
    EXPECT_EQ(tick(processor, Values{{Count, 0}}), Lines{"mark"});
    EXPECT_EQ(processor.position(script), 2u);
    EXPECT_EQ(tick(processor, Values{{Count, 1}}), Lines{"mark"});
    EXPECT_EQ(tick(processor, Values{{Count, 2}}), Lines{"mark"});
    EXPECT_TRUE(tick(processor, Values{{Count, 3}}).empty());
    EXPECT_EQ(processor.position(script), 4u);   // past the loop, on the wait after it
}

TEST(ScriptKit, SetAsksForAChangeOfTheValuesOwnType) {
    const Script script{.name = "sets",
                        .body = {set(Count, Count + 5), set(Flag, true), set(Lamp, Light::Red)}};
    ScriptProcessor processor{{&script}, kConfig};
    processor.start(script);
    processor.advance(Values{{Count, 3}});

    const std::span<const Request> requests = processor.requests();
    ASSERT_EQ(requests.size(), 3u);
    ASSERT_NE(requests[0].change<int>(), nullptr);
    EXPECT_EQ(requests[0].change<int>()->value.id(), Count.id());
    EXPECT_EQ(requests[0].change<int>()->to, 8);
    EXPECT_EQ(requests[0].change<bool>(), nullptr);
    EXPECT_EQ(requests[0].command(), nullptr);
    EXPECT_EQ(requests[0].arguments().size(), 0u);

    ASSERT_NE(requests[1].change<bool>(), nullptr);
    EXPECT_EQ(requests[1].change<bool>()->value.id(), Flag.id());
    EXPECT_TRUE(requests[1].change<bool>()->to);
    EXPECT_EQ(requests[1].change<int>(), nullptr);

    ASSERT_NE(requests[2].change<Light>(), nullptr);
    EXPECT_EQ(requests[2].change<Light>()->to, Light::Red);
    EXPECT_EQ(requests[2].change<Way>(), nullptr);
}

TEST(ScriptKit, ACallRunsTheCalleeThenCarriesOn) {
    const Script chime{.name = "chime", .body = {say("ding"), wait(2), say("dong")}};
    const Script caller{.name = "caller", .body = {say("before"), call(chime), say("after")}};
    ScriptProcessor processor{{&caller, &chime}, kConfig};
    EXPECT_EQ(processor.depth(caller), 0u);
    processor.start(caller);
    EXPECT_EQ(processor.depth(caller), 1u);

    EXPECT_EQ(tick(processor), (Lines{"say before", "say ding"}));
    EXPECT_EQ(processor.depth(caller), 2u);
    EXPECT_EQ(processor.position(caller), 1u);   // the call it is inside
    EXPECT_FALSE(processor.running(chime));      // called, not started

    // The callee's wait holds the caller.
    EXPECT_TRUE(tick(processor).empty());
    EXPECT_EQ(tick(processor), (Lines{"say dong", "say after"}));
    EXPECT_FALSE(processor.running(caller));
    EXPECT_EQ(processor.depth(caller), 0u);
}

TEST(ScriptKit, HigherPriorityIsSteppedFirstAndEqualsInListOrder) {
    const Script low{.name = "low", .body = {say("low")}};
    const Script high{.name = "high", .priority = 5, .body = {say("high")}};
    const Script alsoLow{.name = "also low", .body = {say("also low")}};
    ScriptProcessor processor{{&low, &high, &alsoLow}, kConfig};
    processor.start(low);
    processor.start(high);
    processor.start(alsoLow);
    EXPECT_EQ(tick(processor), (Lines{"say high", "say low", "say also low"}));
}

TEST(ScriptKit, AnInterruptedScriptIsHeldAndCarriesOnFromWhereItWas) {
    const Script a{.name = "a", .body = {say("a1"), wait(1), say("a2"), wait(1), say("a3")}};
    const Script b{.name = "b", .interrupts = {&a}, .body = {say("b1"), wait(1), say("b2")}};
    const Script c{.name = "c", .interrupts = {&a}, .body = {wait(3)}};
    ScriptProcessor processor{{&a, &b, &c}, kConfig};

    processor.start(a);
    EXPECT_EQ(tick(processor), Lines{"say a1"});
    EXPECT_EQ(processor.position(a), 1u);

    processor.start(b);
    EXPECT_TRUE(processor.suspended(a));
    EXPECT_FALSE(processor.suspended(b));
    EXPECT_EQ(tick(processor), Lines{"say b1"});
    EXPECT_EQ(processor.position(a), 1u);
    EXPECT_EQ(tick(processor), Lines{"say b2"});
    EXPECT_FALSE(processor.running(b));
    EXPECT_FALSE(processor.suspended(a));
    EXPECT_EQ(tick(processor), Lines{"say a2"});   // its wait counted only the ticks it was stepped
    EXPECT_EQ(processor.position(a), 3u);

    // Held by two scripts, it carries on when both have ended.
    processor.start(b);
    processor.start(c);
    EXPECT_EQ(tick(processor), Lines{"say b1"});
    EXPECT_EQ(tick(processor), Lines{"say b2"});
    EXPECT_FALSE(processor.running(b));
    EXPECT_TRUE(processor.suspended(a));
    EXPECT_TRUE(tick(processor).empty());
    EXPECT_TRUE(tick(processor).empty());
    EXPECT_FALSE(processor.running(c));
    EXPECT_FALSE(processor.suspended(a));
    EXPECT_EQ(tick(processor), Lines{"say a3"});

    // A script started while one that interrupts it is running starts held.
    ScriptProcessor other{{&a, &b, &c}, kConfig};
    other.start(b);
    other.start(a);
    EXPECT_TRUE(other.suspended(a));
    EXPECT_EQ(tick(other), Lines{"say b1"});
    EXPECT_EQ(tick(other), Lines{"say b2"});
    EXPECT_EQ(tick(other), Lines{"say a1"});
}

TEST(ScriptKit, StopEndsAScriptAndRestartPutsItBackAtItsStart) {
    const Script target{.name = "target", .body = {say("one"), wait(1), say("two"), wait(1), say("three")}};
    const Script stopper{.name = "stopper", .body = {stop(target), say("stopped it")}};
    const Script restarter{.name = "restarter", .body = {restart(target)}};
    ScriptProcessor processor{{&stopper, &restarter, &target}, kConfig};

    // Stopped by a script.
    processor.start(target);
    EXPECT_EQ(tick(processor), Lines{"say one"});
    processor.start(stopper);
    EXPECT_EQ(tick(processor), Lines{"say stopped it"});
    EXPECT_FALSE(processor.running(target));
    EXPECT_EQ(processor.position(target), 0u);
    EXPECT_TRUE(processor.idle());

    // Stopped by the game.
    processor.start(target);
    EXPECT_EQ(tick(processor), Lines{"say one"});
    processor.stop(target);
    EXPECT_FALSE(processor.running(target));
    EXPECT_TRUE(tick(processor).empty());

    // Restarted by a script: back at its first instruction, the same tick.
    processor.start(target);
    EXPECT_EQ(tick(processor), Lines{"say one"});
    EXPECT_EQ(tick(processor), Lines{"say two"});
    EXPECT_EQ(processor.position(target), 3u);
    processor.start(restarter);
    EXPECT_EQ(tick(processor), Lines{"say one"});
    EXPECT_EQ(processor.position(target), 1u);

    // Restarted by the game.
    processor.restart(target);
    EXPECT_TRUE(processor.running(target));
    EXPECT_EQ(processor.position(target), 0u);
    EXPECT_FALSE(processor.waiting(target));
    EXPECT_EQ(tick(processor), Lines{"say one"});

    // Restart starts a script that was off.
    processor.stop(target);
    processor.restart(target);
    EXPECT_TRUE(processor.running(target));
}

TEST(ScriptKit, AStoppedHeldScriptStaysOffAndARestartedOneStaysHeld) {
    const Script a{.name = "a", .body = {say("a1"), wait(1), say("a2"), wait(10)}};
    const Script b{.name = "b", .interrupts = {&a}, .body = {wait(2)}};
    ScriptProcessor processor{{&a, &b}, kConfig};

    processor.start(a);
    EXPECT_EQ(tick(processor), Lines{"say a1"});
    processor.start(b);
    processor.stop(a);
    EXPECT_TRUE(tick(processor).empty());
    EXPECT_TRUE(tick(processor).empty());
    EXPECT_TRUE(tick(processor).empty());
    EXPECT_FALSE(processor.running(b));
    EXPECT_FALSE(processor.running(a));
    EXPECT_TRUE(tick(processor).empty());

    // Restarted while held: at its start, still held, and off from the top once the holder ends.
    processor.start(a);
    EXPECT_EQ(tick(processor), Lines{"say a1"});
    EXPECT_EQ(tick(processor), Lines{"say a2"});
    EXPECT_EQ(processor.position(a), 3u);
    processor.start(b);
    processor.restart(a);
    EXPECT_TRUE(processor.running(a));
    EXPECT_TRUE(processor.suspended(a));
    EXPECT_EQ(processor.position(a), 0u);
    EXPECT_TRUE(tick(processor).empty());
    EXPECT_TRUE(tick(processor).empty());
    EXPECT_TRUE(tick(processor).empty());
    EXPECT_FALSE(processor.running(b));
    EXPECT_EQ(tick(processor), Lines{"say a1"});
}

TEST(ScriptKit, AScriptThatStopsOrRestartsItselfIsDoneForTheTick) {
    ScriptProcessor processor{{&quitter, &again}, kConfig};
    processor.start(quitter);
    EXPECT_EQ(tick(processor), Lines{"say one"});
    EXPECT_FALSE(processor.running(quitter));
    EXPECT_TRUE(tick(processor).empty());

    processor.start(again);
    EXPECT_EQ(tick(processor), Lines{"say again"});
    EXPECT_TRUE(processor.running(again));
    EXPECT_EQ(processor.position(again), 0u);
    EXPECT_EQ(tick(processor), Lines{"say again"});
}

TEST(ScriptKit, BindingsReadAGamesStateAndWriteChangesIntoIt) {
    World world{.flag = true, .count = 4, .lamp = Light::Red, .x = 21};
    const std::array<ValueId, 5> ids{Flag.id(), Count.id(), Lamp.id(), Speed.id(), Busy.id()};
    const Values values = kBindings.gather(world, ids);
    EXPECT_TRUE(values.get(Flag));
    EXPECT_EQ(values.get(Count), 4);
    EXPECT_EQ(values.get(Lamp), Light::Red);
    EXPECT_EQ(values.get(Speed), 42);
    EXPECT_FALSE(values.get(Busy));

    const std::array<ValueId, 1> unbound{Other.id()};
    try {
        (void)kBindings.gather(world, unbound);
        FAIL() << "an unbound value was gathered";
    } catch (const std::out_of_range& error) {
        EXPECT_TRUE(contains(error.what(), std::to_string(Other.id())));
    }

    const Script script{.name = "writes",
                        .body = {set(Count, 9), set(Speed, 1), set(Busy, true), say("hi"), set(Other, true)}};
    ScriptProcessor processor{{&script}, kConfig};
    processor.start(script);
    processor.advance({});
    const std::span<const Request> requests = processor.requests();
    ASSERT_EQ(requests.size(), 5u);

    EXPECT_TRUE(kBindings.apply(world, requests[0]));    // a member: written
    EXPECT_EQ(world.count, 9);
    EXPECT_FALSE(kBindings.apply(world, requests[1]));   // a getter alone: read only
    EXPECT_EQ(world.x, 21);
    EXPECT_TRUE(kBindings.apply(world, requests[2]));    // a getter and a setter
    EXPECT_TRUE(world.busy);
    EXPECT_EQ(world.busyWrites, 1);
    EXPECT_FALSE(kBindings.apply(world, requests[3]));   // a command, not a Change
    EXPECT_THROW((void)kBindings.apply(world, requests[4]), std::out_of_range);
}

TEST(ScriptKit, HandlersRunACommandsFunctionAndTheTwoCallsCarryATick) {
    const Script first{.name = "first",
                       .body = {Move(Way::North, Count + 1), say("first"), set(Count, 5)}};
    const Script second{.name = "second", .body = {say("second", Tone::Loud), set(Flag, true)}};
    const Script reads{.name = "reads", .body = {waitWhile(Speed > 100)}};
    ScriptProcessor one{{&first}, kConfig};
    ScriptProcessor two{{&second}, kConfig};
    ScriptProcessor three{{&reads}, kConfig};
    World world{.count = 2};

    one.start(first);
    two.start(second);
    ScriptKit::stepScripts({&one, &two, &three}, kBindings, world);
    EXPECT_TRUE(world.log.empty());       // stepping writes nothing
    EXPECT_EQ(world.count, 2);
    EXPECT_EQ(world.speedReads, 0);       // an idle processor is not gathered for
    EXPECT_EQ(one.requests().size(), 3u);

    ScriptKit::applyScripts({&one, &two, &three}, kBindings, kHandlers, world);
    EXPECT_EQ(world.log, (Lines{"move north 3", "say first", "say second!"}));
    EXPECT_EQ(world.count, 5);
    EXPECT_TRUE(world.flag);
    EXPECT_TRUE(one.requests().empty());

    // Applied once: a second apply with no step between writes nothing.
    world.count = 0;
    ScriptKit::applyScripts({&one, &two, &three}, kBindings, kHandlers, world);
    EXPECT_EQ(world.log.size(), 3u);
    EXPECT_EQ(world.count, 0);

    // The processors are applied in the order given.
    one.start(first);
    two.start(second);
    ScriptKit::stepScripts({&one, &two}, kBindings, world);
    world.log.clear();
    ScriptKit::applyScripts({&two, &one}, kBindings, kHandlers, world);
    EXPECT_EQ(world.log, (Lines{"say second!", "move north 1", "say first"}));

    // A processor with a script running is gathered for.
    three.start(reads);
    ScriptKit::stepScripts({&three}, kBindings, world);
    EXPECT_EQ(world.speedReads, 1);

    // A command with no handler.
    const Script marks{.name = "marks", .body = {Mark()}};
    ScriptProcessor four{{&marks}, kConfig};
    four.start(marks);
    ScriptKit::stepScripts({&four}, kBindings, world);
    try {
        ScriptKit::applyScripts({&four}, kBindings, kHandlers, world);
        FAIL() << "a command with no handler was applied";
    } catch (const std::out_of_range& error) {
        EXPECT_TRUE(contains(error.what(), "'mark'"));
    }
}

TEST(ScriptKit, AnArgumentReadAsAnotherKindThrowsNamingTheCommandAndTheArgument) {
    const Script script{.name = "asks",
                        .body = {Mark(Count > 2, 5), Say("x"), Move(Way::North, 3)}};
    ScriptProcessor processor{{&script}, kConfig};
    processor.start(script);
    processor.advance(Values{{Count, 3}});
    const std::span<const Request> requests = processor.requests();
    ASSERT_EQ(requests.size(), 3u);

    EXPECT_TRUE(requests[0].arguments().truth(0));   // the expression, worked out
    EXPECT_THROW((void)requests[0].arguments().number(0), std::logic_error);

    const Arguments& said = requests[1].arguments();
    try {
        (void)said.number(0);
        FAIL() << "text was read as a number";
    } catch (const std::logic_error& error) {
        EXPECT_TRUE(contains(error.what(), "'say'"));
        EXPECT_TRUE(contains(error.what(), "argument 0"));
    }
    EXPECT_THROW((void)said.truth(0), std::logic_error);
    EXPECT_THROW((void)said.word<Tone>(0), std::logic_error);
    EXPECT_THROW((void)said.text(1), std::out_of_range);

    const Arguments& moved = requests[2].arguments();
    EXPECT_EQ(moved.word<Way>(0), Way::North);
    EXPECT_THROW((void)moved.word<Tone>(0), std::logic_error);   // a word of another enum
    EXPECT_THROW((void)moved.text(1), std::logic_error);
    EXPECT_EQ(moved.number(1), 3);
}

TEST(ScriptKit, ACopiedProcessorRunsOnByItself) {
    const Script script{.name = "counts", .body = {say("one"), wait(1), say("two"), wait(1), say("three")}};
    ScriptProcessor original{{&script}, kConfig};
    original.start(script);
    EXPECT_EQ(tick(original), Lines{"say one"});

    ScriptProcessor copy = original;
    EXPECT_EQ(tick(original), Lines{"say two"});
    EXPECT_EQ(tick(original), Lines{"say three"});
    EXPECT_FALSE(original.running(script));

    EXPECT_TRUE(copy.running(script));
    EXPECT_EQ(copy.position(script), 1u);
    EXPECT_EQ(asked(copy), Lines{"say one"});   // what the original had asked for when it was copied
    EXPECT_EQ(tick(copy), Lines{"say two"});
}

TEST(ScriptKit, WhatAScriptAsksForIsWrittenAtTheNextApply) {
    const Script setter{.name = "setter", .body = {set(Flag, true)}};
    const Script reader{.name = "reader",
                        .body = {loop({when(Flag, {say("on")}).otherwise({say("off")}), wait(1)})}};
    ScriptProcessor writes{{&setter}, kConfig};
    ScriptProcessor reads{{&reader}, kConfig};
    World world;

    // Tick 1: a script started between the two calls asks on this tick's step.
    ScriptKit::applyScripts({&writes, &reads}, kBindings, kHandlers, world);
    reads.start(reader);
    ScriptKit::stepScripts({&writes, &reads}, kBindings, world);
    EXPECT_TRUE(world.log.empty());   // and nothing of it is written yet

    // Tick 2: what it asked for is written at the top. The setter asks; the reader, stepped on the
    // same tick, still reads the old value.
    ScriptKit::applyScripts({&writes, &reads}, kBindings, kHandlers, world);
    EXPECT_EQ(world.log, Lines{"say off"});
    writes.start(setter);
    ScriptKit::stepScripts({&writes, &reads}, kBindings, world);
    EXPECT_FALSE(world.flag);

    // Tick 3: the change is written, and the reader reads it.
    ScriptKit::applyScripts({&writes, &reads}, kBindings, kHandlers, world);
    EXPECT_TRUE(world.flag);
    EXPECT_EQ(world.log, (Lines{"say off", "say off"}));
    ScriptKit::stepScripts({&writes, &reads}, kBindings, world);

    ScriptKit::applyScripts({&writes, &reads}, kBindings, kHandlers, world);
    EXPECT_EQ(world.log, (Lines{"say off", "say off", "say on"}));
}

TEST(ScriptKit, AnAdvanceThrowsOnlyForAValueItWasNotHanded) {
    static constexpr std::string_view kLine = "the very same text";
    const Script script{.name = "reads",
                        .body = {say(kLine), loop({waitWhile(Count > 0), when(Flag, {Mark()}), wait(1)})}};
    ScriptProcessor processor{{&script}, kConfig};
    processor.start(script);

    World world{.count = 0};
    const Values handed = kBindings.gather(world, processor.reads());
    EXPECT_NO_THROW(processor.advance(handed));

    // A request's text is the script's own, not a copy.
    ASSERT_EQ(processor.requests().size(), 1u);
    EXPECT_EQ(processor.requests()[0].arguments().text(0).data(), kLine.data());

    processor.clearRequests();
    EXPECT_TRUE(processor.requests().empty());

    try {
        processor.advance(Values{{Flag, true}});
        FAIL() << "an advance read a value it was not handed";
    } catch (const std::out_of_range& error) {
        EXPECT_TRUE(contains(error.what(), std::to_string(Count.id())));
    }
}

TEST(ScriptKit, TheCrossingsSignalCyclesAndRemindsByItself) {
    ScriptProcessor signal = crossingSignal();
    Values values = calm();
    EXPECT_EQ(signal.start(crossing::Begin, values), 2);
    EXPECT_TRUE(signal.running(crossing::traffic));
    EXPECT_TRUE(signal.running(crossing::reminder));

    // Five seconds green, one yellow, three red at 60 ticks a second; the reminder after eight.
    std::vector<std::pair<int, std::string>> seen;
    for (int n = 1; n <= 541; ++n) {
        for (const std::string& line : fedTick(signal, values)) {
            seen.emplace_back(n, line);
        }
    }
    const std::vector<std::pair<int, std::string>> expected{
        {1, "set signal green"},
        {301, "set signal yellow"},
        {361, "set signal red"},
        {481, "say Press the button\nto cross."},
        {541, "set signal green"},
    };
    EXPECT_EQ(seen, expected);
}

TEST(ScriptKit, TheCrossingsReminderKeepsQuietWhileTheOfficerIsAfterYou) {
    ScriptProcessor signal = crossingSignal();
    Values values = calm();
    signal.start(crossing::reminder);
    values.set(crossing::Wanted, true);
    for (int n = 1; n <= 600; ++n) {
        EXPECT_TRUE(fedTick(signal, values).empty()) << n;   // its eight seconds pass, and it holds
    }
    EXPECT_TRUE(signal.waiting(crossing::reminder));
    values.set(crossing::Wanted, false);
    EXPECT_EQ(fedTick(signal, values), Lines{"say Press the button\nto cross."});
}

TEST(ScriptKit, TheCrossingsButtonInterruptsTheCycleForAWalkPhase) {
    ScriptProcessor signal = crossingSignal();
    Values values = calm();
    signal.start(crossing::Begin, values);
    for (int n = 1; n <= 10; ++n) {
        fedTick(signal, values);
    }
    EXPECT_EQ(signal.position(crossing::traffic), 1u);   // in its five seconds of green

    // The press starts both of the button's scripts; the walk phase, the higher priority, goes first.
    EXPECT_EQ(signal.start(crossing::Interact, values), 2);
    EXPECT_TRUE(signal.suspended(crossing::traffic));
    EXPECT_EQ(fedTick(signal, values), (Lines{"set signal yellow", "say Wait for the\nwalk sign."}));
    EXPECT_EQ(fedTick(signal, values), Lines{"set presses 1"});
    EXPECT_EQ(values.get(crossing::Presses), 1);

    // Yellow for a second, red for a second, then the walk sign for four.
    std::vector<std::pair<int, std::string>> seen;
    for (int n = 13; n <= 521; ++n) {
        for (const std::string& line : fedTick(signal, values)) {
            seen.emplace_back(n, line);
        }
        if (n == 380) {
            EXPECT_EQ(signal.depth(crossing::walkPhase), 2u);   // inside the first blink
            EXPECT_EQ(signal.position(crossing::traffic), 1u);  // the cycle, held where it was
        }
    }
    const std::vector<std::pair<int, std::string>> expected{
        {71, "set signal red"},
        {131, "set walk lit true"},
        {371, "set walk dim true"},
        {386, "set walk dim false"},
        {401, "set walk dim true"},
        {416, "set walk dim false"},
        {431, "set walk dim true"},
        {446, "set walk dim false"},
        {461, "set walk lit false"},
        {481, "say Press the button\nto cross."},
        {521, "set presses 0"},
        {521, "set signal green"},   // the cycle, restarted, from the top
    };
    EXPECT_EQ(seen, expected);
    EXPECT_FALSE(signal.running(crossing::walkPhase));
    EXPECT_FALSE(signal.suspended(crossing::traffic));
    EXPECT_EQ(signal.position(crossing::traffic), 1u);

    // Pressed on a red, the walk sign lights at once.
    ScriptProcessor onRed = crossingSignal();
    Values red = calm();
    red.set(crossing::Signal, crossing::Lamp::Red);
    onRed.start(crossing::walkPhase);
    EXPECT_EQ(fedTick(onRed, red), Lines{"set walk lit true"});
}

TEST(ScriptKit, TheCrossingsButtonAnswersByTheSignAndThePressesBefore) {
    ScriptProcessor signal = crossingSignal();
    Values values = calm();
    values.set(crossing::Presses, 2);
    signal.start(crossing::pressed);
    EXPECT_EQ(fedTick(signal, values), Lines{"say PRESSING HARDER\nWON'T HELP. (shout)"});
    EXPECT_EQ(fedTick(signal, values), Lines{"set presses 3"});

    // It holds while its message shows, and a press meanwhile does not start it again.
    values.set(crossing::MessageShowing, true);
    EXPECT_TRUE(fedTick(signal, values).empty());
    EXPECT_TRUE(signal.waiting(crossing::pressed));
    EXPECT_EQ(signal.start(crossing::Interact, values), 1);   // the walk phase alone
    signal.stop(crossing::walkPhase);
    values.set(crossing::MessageShowing, false);
    EXPECT_TRUE(fedTick(signal, values).empty());
    EXPECT_FALSE(signal.running(crossing::pressed));

    values.set(crossing::WalkLit, true);
    signal.start(crossing::pressed);
    EXPECT_EQ(fedTick(signal, values), Lines{"say Walk now."});
}

TEST(ScriptKit, TheCrossingsCarsHoldAtTheirLineOnRed) {
    ScriptProcessor cars = crossingCars();
    Values values = calm();
    const Lines all{"advance 0", "advance 1", "advance 2", "advance 3", "advance 4", "advance 5"};
    EXPECT_EQ(cars.start(crossing::Begin, values), 6);
    EXPECT_EQ(tick(cars, values), all);
    EXPECT_EQ(tick(cars, values), all);

    // At its line on green, a car drives on; on red it holds there, and the others drive up.
    values.set(crossing::AtLine[0], true);
    EXPECT_EQ(tick(cars, values), all);
    values.set(crossing::Signal, crossing::Lamp::Red);
    const Lines others{"advance 1", "advance 2", "advance 3", "advance 4", "advance 5"};
    EXPECT_EQ(tick(cars, values), others);
    EXPECT_TRUE(cars.waiting(crossing::driving[0]));
    EXPECT_FALSE(cars.waiting(crossing::driving[1]));
    EXPECT_EQ(tick(cars, values), others);

    values.set(crossing::Signal, crossing::Lamp::Green);
    EXPECT_EQ(tick(cars, values), all);

    // An arrest in the street does not hold a car; the ambulance and the patrol car each hold them all,
    // whatever the signal.
    values.set(crossing::Arrested, true);
    EXPECT_EQ(tick(cars, values), all);
    values.set(crossing::AmbulanceOut, true);
    EXPECT_TRUE(tick(cars, values).empty());
    values.set(crossing::AmbulanceOut, false);
    EXPECT_EQ(tick(cars, values), all);
    values.set(crossing::PatrolOut, true);
    EXPECT_TRUE(tick(cars, values).empty());
    values.set(crossing::PatrolOut, false);
    EXPECT_EQ(tick(cars, values), all);
}

TEST(ScriptKit, TheCrossingsOfficerArrestsWhoeverStepsOffOnRedWithoutTheWalkSign) {
    ScriptProcessor officer = crossingOfficer();
    Values values = calm();
    EXPECT_EQ(officer.start(crossing::Begin, values), 1);
    EXPECT_EQ(fedTick(officer, values), Lines{"face left"});

    // Not on green — the cars deal with that — and not with the walk sign lit.
    EXPECT_EQ(officer.start(crossing::StepOffCurb, values), 0);
    values.set(crossing::Signal, crossing::Lamp::Red);
    values.set(crossing::WalkLit, true);
    EXPECT_EQ(officer.start(crossing::StepOffCurb, values), 0);
    values.set(crossing::WalkLit, false);
    EXPECT_EQ(officer.start(crossing::StepOffCurb, values), 1);
    EXPECT_TRUE(officer.running(crossing::arrest));
    EXPECT_TRUE(officer.suspended(crossing::watch));

    EXPECT_EQ(fedTick(officer, values), (Lines{"set wanted true", "say HEY! STOP! (shout)"}));
    EXPECT_EQ(officer.depth(crossing::arrest), 2u);
    EXPECT_EQ(officer.start(crossing::StepOffCurb, values), 0);   // already wanted

    // He closes in a step a tick, on wherever the player is.
    EXPECT_EQ(fedTick(officer, values), Lines{"step toward 80 100"});
    values.set(crossing::PlayerX, 90);
    EXPECT_EQ(fedTick(officer, values), Lines{"step toward 90 100"});

    // Caught: he says so. Nothing holds the traffic for that; it goes by the signal as ever.
    values.set(crossing::Caught, true);
    EXPECT_EQ(fedTick(officer, values), (Lines{"set arrested true", "say You're under\narrest."}));
    values.set(crossing::MessageShowing, true);
    EXPECT_TRUE(fedTick(officer, values).empty());
    EXPECT_FALSE(values.get(crossing::PatrolOut));

    // Once that has been read he calls for backup: a patrol car, which every other car holds for.
    values.set(crossing::MessageShowing, false);
    EXPECT_EQ(fedTick(officer, values), (Lines{"set patrol out true", "set siren red true"}));
    EXPECT_EQ(officer.depth(crossing::arrest), 3u);   // the arrest, apprehending, calling backup
    EXPECT_TRUE(officer.running(crossing::siren));

    // It drives at you until it reaches you; then the officer walks to its door, gets in, and they go.
    EXPECT_EQ(nextAsked(officer, values), Lines{"drive patrol 2"});
    values.set(crossing::PatrolAtPlayer, true);
    EXPECT_EQ(nextAsked(officer, values), Lines{"say Whoops! We gotta\nget outta here!"});

    // Nobody moves until that has been said.
    values.set(crossing::MessageShowing, true);
    for (int n = 0; n < 90; ++n) {
        EXPECT_TRUE(quiet(fedTick(officer, values)).empty()) << n;
    }
    values.set(crossing::MessageShowing, false);
    EXPECT_EQ(nextAsked(officer, values), Lines{"step toward 40 100"});
    EXPECT_EQ(officer.start(crossing::Confirm, values), 0);   // nothing to carry on from yet
    values.set(crossing::OfficerAtPatrol, true);
    EXPECT_EQ(nextAsked(officer, values), Lines{"set officer aboard true"});
    EXPECT_EQ(officer.start(crossing::Confirm, values), 0);   // the patrol car is still out
    EXPECT_EQ(nextAsked(officer, values), Lines{"drive patrol 6"});   // they speed away
    values.set(crossing::PatrolGone, true);
    EXPECT_EQ(nextAsked(officer, values), (Lines{"set patrol out false", "say Press to continue."}));
    EXPECT_FALSE(officer.running(crossing::siren));
    EXPECT_TRUE(officer.running(crossing::arrest));
    EXPECT_TRUE(values.get(crossing::Arrested));              // you are left as you were

    // The press: the arrest stops asking, and everyone is put back.
    EXPECT_EQ(officer.start(crossing::Confirm, values), 1);
    EXPECT_EQ(fedTick(officer, values), (Lines{"send back", "set officer aboard false", "set arrested false",
                                              "set wanted false", "set officer down false"}));
    EXPECT_FALSE(officer.running(crossing::arrest));

    // His watch carries on from where the arrest interrupted it.
    EXPECT_TRUE(officer.running(crossing::watch));
    EXPECT_FALSE(officer.suspended(crossing::watch));
    EXPECT_EQ(officer.position(crossing::watch), 1u);
}

TEST(ScriptKit, TheCrossingsOfficerArrestsOnArrivalWithoutTheWalkSign) {
    ScriptProcessor officer = crossingOfficer();
    Values values = calm();
    values.set(crossing::WalkLit, true);
    EXPECT_EQ(officer.start(crossing::ReachFarSide, values), 0);
    values.set(crossing::WalkLit, false);
    EXPECT_EQ(officer.start(crossing::ReachFarSide, values), 1);
    EXPECT_TRUE(officer.running(crossing::arrestOnArrival));
    EXPECT_EQ(fedTick(officer, values), (Lines{"set wanted true", "say HEY! STOP! (shout)"}));

    // Crossing with the walk sign lit is the one way across: the signal's script says so and stops
    // the reminder.
    ScriptProcessor signal = crossingSignal();
    Values lit = calm();
    signal.start(crossing::Begin, lit);
    EXPECT_EQ(signal.start(crossing::ReachFarSide, lit), 0);
    lit.set(crossing::WalkLit, true);
    EXPECT_EQ(signal.start(crossing::ReachFarSide, lit), 1);
    EXPECT_TRUE(signal.running(crossing::reminder));
    EXPECT_EQ(tick(signal, lit), (Lines{"set signal green", "say Safely across."}));
    EXPECT_FALSE(signal.running(crossing::reminder));
    EXPECT_TRUE(signal.running(crossing::crossed));   // until its message has been read
    EXPECT_TRUE(tick(signal, lit).empty());
    EXPECT_FALSE(signal.running(crossing::crossed));
}

TEST(ScriptKit, ACarThatReachesTheCrossingsOfficerBringsTheAmbulance) {
    ScriptProcessor officer = crossingOfficer();
    Values values = calm();
    officer.start(crossing::Begin, values);
    fedTick(officer, values);
    values.set(crossing::Signal, crossing::Lamp::Red);
    EXPECT_EQ(officer.start(crossing::StepOffCurb, values), 1);
    EXPECT_EQ(fedTick(officer, values), (Lines{"set wanted true", "say HEY! STOP! (shout)"}));
    EXPECT_EQ(fedTick(officer, values), Lines{"step toward 80 100"});

    // Down, with you in the road beside him: the arrest is over, mid-chase.
    values.set(crossing::InStreet, true);
    EXPECT_EQ(officer.start(crossing::OfficerHit, values), 1);
    values.set(crossing::Arrested, true);   // as when he has just caught you
    EXPECT_EQ(fedTick(officer, values),
              (Lines{"set officer down true", "set arrested false", "say OOF! (shout)"}));
    EXPECT_FALSE(officer.running(crossing::arrest));
    EXPECT_TRUE(officer.suspended(crossing::watch));
    EXPECT_FALSE(values.get(crossing::Arrested));   // he no longer has you: you can move

    // The ambulance stops the cars, so it holds off until they have reached you too — or you are out
    // of the street.
    EXPECT_TRUE(fedTick(officer, values).empty());
    EXPECT_TRUE(fedTick(officer, values).empty());
    EXPECT_TRUE(officer.waiting(crossing::officerDown));
    EXPECT_FALSE(officer.running(crossing::siren));
    values.set(crossing::Struck, true);
    EXPECT_EQ(fedTick(officer, values), (Lines{"set ambulance out true", "set siren red true"}));
    EXPECT_EQ(officer.depth(crossing::officerDown), 2u);          // inside the ambulance's run
    EXPECT_TRUE(officer.running(crossing::siren));                // which started its lights
    EXPECT_EQ(officer.start(crossing::OfficerHit, values), 0);    // he is hit once
    EXPECT_TRUE(values.get(crossing::Wanted));                    // and you are left as you were

    // It drives up to him, and its medics get out, walk to him, lift him and carry him back.
    EXPECT_EQ(nextAsked(officer, values), Lines{"drive ambulance"});
    values.set(crossing::AmbulanceAtOfficer, true);
    EXPECT_EQ(nextAsked(officer, values), Lines{"set medics out true"});
    EXPECT_EQ(nextAsked(officer, values), Lines{"walk medics officer"});
    values.set(crossing::MedicsAtOfficer, true);
    EXPECT_EQ(nextAsked(officer, values), Lines{"set officer carried true"});
    EXPECT_EQ(officer.start(crossing::Confirm, values), 0);       // nothing to carry on from yet
    EXPECT_EQ(nextAsked(officer, values), Lines{"walk medics ambulance"});
    values.set(crossing::MedicsAtAmbulance, true);
    EXPECT_EQ(nextAsked(officer, values),
              (Lines{"set officer aboard true", "set officer carried false", "set medics out false"}));
    EXPECT_EQ(officer.start(crossing::Confirm, values), 0);       // the ambulance is still out
    EXPECT_EQ(nextAsked(officer, values), Lines{"drive ambulance"});

    // Off the screen: its lights stop, the cars may go, and it asks for the press.
    values.set(crossing::AmbulanceGone, true);
    EXPECT_EQ(nextAsked(officer, values), (Lines{"set ambulance out false", "say Press to continue."}));
    EXPECT_FALSE(officer.running(crossing::siren));
    EXPECT_EQ(officer.depth(crossing::officerDown), 1u);
    EXPECT_TRUE(officer.running(crossing::officerDown));

    // The press: it stops asking, and everyone is put back.
    EXPECT_EQ(officer.start(crossing::Confirm, values), 1);
    EXPECT_EQ(fedTick(officer, values), (Lines{"send back", "set officer aboard false", "set arrested false",
                                              "set wanted false", "set officer down false"}));
    EXPECT_FALSE(officer.running(crossing::officerDown));
    EXPECT_FALSE(officer.running(crossing::carryOn));
    EXPECT_TRUE(officer.running(crossing::watch));
    EXPECT_FALSE(officer.suspended(crossing::watch));
    EXPECT_FALSE(values.get(crossing::Wanted));
}

TEST(ScriptKit, TheAmbulancesLightBarAlternatesEveryTenthOfASecond) {
    ScriptProcessor officer = crossingOfficer();
    Values values = calm();
    officer.restart(crossing::siren);   // as the ambulance's run does
    std::vector<std::pair<int, std::string>> seen;
    for (int n = 1; n <= 19; ++n) {
        for (const std::string& line : fedTick(officer, values)) {
            seen.emplace_back(n, line);
        }
    }
    const std::vector<std::pair<int, std::string>> expected{
        {1, "set siren red true"}, {7, "set siren red false"}, {13, "set siren red true"}, {19, "set siren red false"}};
    EXPECT_EQ(seen, expected);
    officer.stop(crossing::siren);
    EXPECT_TRUE(fedTick(officer, values).empty());
}

TEST(ScriptKit, BeingStruckEndsTheWalkPhaseAndStartsTheCycleOver) {
    ScriptProcessor signal = crossingSignal();
    Values values = calm();
    signal.start(crossing::Begin, values);
    fedTick(signal, values);
    signal.start(crossing::walkPhase);
    EXPECT_EQ(fedTick(signal, values), Lines{"set signal yellow"});
    EXPECT_TRUE(signal.suspended(crossing::traffic));

    EXPECT_EQ(signal.start(crossing::HitByCar, values), 1);
    EXPECT_EQ(fedTick(signal, values), (Lines{"set struck true", "set walk lit false", "set walk dim false",
                                              "say WHAM! (shout)"}));
    EXPECT_FALSE(signal.running(crossing::walkPhase));
    EXPECT_FALSE(signal.suspended(crossing::traffic));

    // While the officer is down too, or you are under arrest and it was the patrol car, you stay down
    // past the message, until the press carries on.
    values.set(crossing::OfficerDown, true);
    EXPECT_TRUE(fedTick(signal, values).empty());
    EXPECT_TRUE(signal.waiting(crossing::struck));
    values.set(crossing::OfficerDown, false);
    values.set(crossing::Arrested, true);
    EXPECT_TRUE(fedTick(signal, values).empty());
    values.set(crossing::Arrested, false);

    // Then everyone is put back, and the cycle starts from the top.
    EXPECT_EQ(fedTick(signal, values),
              (Lines{"send back", "set presses 0", "set struck false", "set signal green"}));
    EXPECT_FALSE(signal.running(crossing::struck));
    EXPECT_EQ(signal.position(crossing::traffic), 1u);
}

TEST(ScriptKit, AnAdvanceOnAThreadOfItsOwnGivesWhatAnAdvanceInTurnGives) {
    const Script made{.name = "made up", .on = Poke,
                      .body = {loop({Mark(Count * 2, Flag), when(Flag, {set(Count, Count + 1)}), wait(1)})}};
    std::array<ScriptProcessor, 4> threaded{crossingSignal(), crossingCars(),
                                            crossingOfficer(), ScriptProcessor{{&made}, kConfig}};
    std::array<ScriptProcessor, 4> inTurn = threaded;

    std::size_t total = 0;
    for (int n = 0; n < 600; ++n) {
        // What every processor is handed this tick: the same for both sets.
        Values values = calm();
        values.set(crossing::Signal, static_cast<crossing::Lamp>((n / 100) % 3));
        values.set(crossing::WalkLit, (n / 37) % 2 == 1);
        values.set(crossing::MessageShowing, (n / 11) % 2 == 1);
        values.set(crossing::Caught, n % 50 == 49);
        for (int car = 0; car < crossing::kCars; ++car) {
            values.set(crossing::AtLine[static_cast<std::size_t>(car)], (n / (7 + car)) % 2 == 1);
        }
        values.set(crossing::Presses, n % 5);
        values.set(crossing::PlayerX, n % 160);
        values.set(crossing::PlayerY, (n * 3) % 128);
        values.set(crossing::AmbulanceAtOfficer, (n / 23) % 2 == 1);
        values.set(crossing::AmbulanceGone, (n / 41) % 2 == 1);
        values.set(crossing::OfficerAboard, (n / 59) % 2 == 1);
        values.set(crossing::MedicsAtOfficer, (n / 19) % 2 == 1);
        values.set(crossing::MedicsAtAmbulance, (n / 29) % 2 == 1);
        values.set(crossing::PatrolAtPlayer, (n / 17) % 2 == 1);
        values.set(crossing::PatrolGone, (n / 47) % 2 == 1);
        values.set(crossing::OfficerAtPatrol, (n / 37) % 2 == 1);
        values.set(crossing::InStreet, (n / 31) % 2 == 1);
        values.set(crossing::Struck, (n / 43) % 2 == 1);
        values.set(Count, n);
        values.set(Flag, n % 2 == 1);

        for (std::array<ScriptProcessor, 4>* set : {&threaded, &inTurn}) {
            if (n == 0) {
                for (ScriptProcessor& processor : *set) {
                    processor.start(crossing::Begin, values);
                }
                (*set)[3].start(Poke, values);
            }
            if (n % 97 == 5) {
                (*set)[0].start(crossing::Interact, values);
            }
            if (n % 131 == 7) {
                (*set)[2].start(crossing::StepOffCurb, values);
            }
            if (n % 173 == 9) {
                (*set)[0].start(crossing::ReachFarSide, values);
                (*set)[2].start(crossing::ReachFarSide, values);
            }
            if (n % 211 == 11) {
                (*set)[0].start(crossing::HitByCar, values);
            }
            if (n % 257 == 13) {
                (*set)[2].start(crossing::OfficerHit, values);
            }
            if (n % 89 == 17) {
                (*set)[2].start(crossing::Confirm, values);
            }
        }

        std::array<std::thread, 4> threads;
        for (std::size_t p = 0; p < threaded.size(); ++p) {
            threads[p] = std::thread([&threaded, &values, p] { threaded[p].advance(values); });
        }
        for (std::thread& thread : threads) {
            thread.join();
        }
        for (std::size_t p = 0; p < inTurn.size(); ++p) {
            inTurn[p].advance(values);
            ASSERT_EQ(asked(threaded[p]), asked(inTurn[p])) << "processor " << p << ", tick " << n;
            total += inTurn[p].requests().size();
        }
    }
    EXPECT_GT(total, 1500u);   // the cars alone ask for most of that
}

}  // namespace
