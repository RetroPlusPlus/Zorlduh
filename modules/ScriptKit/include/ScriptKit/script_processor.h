// ScriptProcessor — the running state of a set of scripts, stepped once per tick.
//
//     ScriptKit::ScriptProcessor host{{&greet, &tidy, &yawn}, config};   // in the game's state
//
//     host.start(Interact, values);        // the game says a trigger happened
//     host.advance(values);                // one tick: every running script steps a little
//     for (const ScriptKit::Request& request : host.requests()) { … }   // what the scripts asked for
//
// A processor reads the values it is handed and hands back requests. It writes nothing but its own
// state, so the game's tick stays the only writer of the game's state. applyScripts and stepScripts
// (bindings.h) make the three lines above for every processor a game has.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "retropp/engine_config.h"
#include "retropp/timing.h"

#include "ScriptKit/command.h"
#include "ScriptKit/entry.h"
#include "ScriptKit/expression.h"
#include "ScriptKit/script.h"
#include "ScriptKit/trigger.h"
#include "ScriptKit/value.h"

namespace ScriptKit {

namespace detail {

enum class InstructionKind : std::uint8_t { Ask, Set, Wait, WaitWhile, JumpUnless, Jump, Call, Stop, Restart };

// One step of a compiled body. Which fields mean something follows from `kind`.
struct Instruction {
    InstructionKind kind = InstructionKind::Jump;
    bool reads = false;                  // it evaluates an expression that reads a value
    std::uint32_t target = 0;            // JumpUnless, Jump: an instruction; Call, Stop, Restart: a script
    std::uint64_t ticks = 0;             // Wait
    NodePtr expression;                  // JumpUnless, WaitWhile: the condition; Set: the new value
    const Command* command = nullptr;    // Ask
    std::uint32_t firstParameter = 0;    // Ask: where its parameters start in the program's list
    std::uint32_t parameterCount = 0;    // Ask
    ValueId value = 0;                   // Set
    ChangeWriter writer = nullptr;       // Set
};

// A body as the processor runs it: a flat list with jumps, and every command's parameters.
struct Program {
    std::vector<Instruction> instructions;
    std::vector<Parameter> parameters;
};

// Everything a processor works out once from its scripts. It never changes afterward, so copies of a
// processor share it.
struct Compiled {
    std::vector<const Script*> scripts;
    std::vector<Program> programs;                     // one per script, in list order
    std::vector<NodePtr> criteria;                     // each script's `when`
    std::vector<std::uint32_t> order;                  // the scripts from highest priority to lowest
    std::vector<std::vector<std::uint32_t>> heldBy;    // for each script, the scripts that interrupt it
    std::vector<std::vector<ValueId>> reads;           // for each script, every value it or a script it calls reads
    std::vector<std::vector<ValueId>> sets;            // for each script, every value it or a script it calls sets
    std::vector<ValueId> allReads;                     // every value any script reads, each once
};

inline constexpr std::uint32_t kNoScript = 0xFFFFFFFFu;

[[nodiscard]] inline std::uint32_t indexOf(const std::vector<const Script*>& scripts, const Script* script) noexcept {
    for (std::size_t n = 0; n < scripts.size(); ++n) {
        if (scripts[n] == script) {
            return static_cast<std::uint32_t>(n);
        }
    }
    return kNoScript;
}

[[noreturn]] inline void refuse(const Script& script, const std::string& why) {
    throw std::logic_error("ScriptKit: script '" + std::string(script.name) + "' " + why);
}

// Adds `id` to `ids` unless it is there.
inline void note(std::vector<ValueId>& ids, ValueId id) {
    if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
        ids.push_back(id);
    }
}

// The script a body names, as its place in the processor's list.
[[nodiscard]] inline std::uint32_t named(const Script& script, const std::vector<const Script*>& scripts,
                                         const Script* other, std::string_view how) {
    const std::uint32_t index = indexOf(scripts, other);
    if (index == kNoScript) {
        refuse(script, std::string(how) + " '" + std::string(other != nullptr ? other->name : std::string_view{}) +
                           "', which is not one of the processor's scripts");
    }
    return index;
}

// Appends `body` to `program` as instructions, and every value the body reads and sets to `reads` and
// `sets`.
inline void compileBody(const Body& body, const Script& script, const std::vector<const Script*>& scripts,
                        const retropp::TimingProfile& timing, Program& program, std::vector<ValueId>& reads,
                        std::vector<ValueId>& sets) {
    const auto here = [&program] { return static_cast<std::uint32_t>(program.instructions.size()); };
    for (const Entry& entry : body) {
        const EntryData& data = EntryAccess::data(entry);
        switch (data.kind) {
        case EntryKind::Ask: {
            Instruction instruction{.kind = InstructionKind::Ask,
                                    .command = data.command,
                                    .firstParameter = static_cast<std::uint32_t>(program.parameters.size()),
                                    .parameterCount = static_cast<std::uint32_t>(data.parameters.size())};
            for (const Parameter& parameter : data.parameters) {
                if (parameter.expression) {
                    collectValues(*parameter.expression, reads);
                    instruction.reads = instruction.reads || readsValues(*parameter.expression);
                }
                program.parameters.push_back(parameter);
            }
            program.instructions.push_back(std::move(instruction));
            break;
        }
        case EntryKind::WaitTicks:
            program.instructions.push_back({.kind = InstructionKind::Wait, .ticks = data.ticks});
            break;
        case EntryKind::WaitTime:
            program.instructions.push_back(
                {.kind = InstructionKind::Wait, .ticks = timing.ticksForDuration(data.time)});
            break;
        case EntryKind::WaitWhile:
            collectValues(*data.expression, reads);
            program.instructions.push_back({.kind = InstructionKind::WaitWhile,
                                            .reads = readsValues(*data.expression),
                                            .expression = data.expression});
            break;
        case EntryKind::Call: {
            const std::uint32_t callee = named(script, scripts, data.script, "calls");
            program.instructions.push_back({.kind = InstructionKind::Call, .target = callee});
            break;
        }
        case EntryKind::Set:
            note(sets, data.value);
            collectValues(*data.expression, reads);
            program.instructions.push_back({.kind = InstructionKind::Set,
                                            .reads = readsValues(*data.expression),
                                            .expression = data.expression,
                                            .value = data.value,
                                            .writer = data.writer});
            break;
        case EntryKind::Stop:
            program.instructions.push_back(
                {.kind = InstructionKind::Stop, .target = named(script, scripts, data.script, "stops")});
            break;
        case EntryKind::Restart:
            program.instructions.push_back(
                {.kind = InstructionKind::Restart, .target = named(script, scripts, data.script, "restarts")});
            break;
        case EntryKind::Loop: {
            const std::uint32_t top = here();
            compileBody(data.body, script, scripts, timing, program, reads, sets);
            program.instructions.push_back({.kind = InstructionKind::Jump, .target = top});
            break;
        }
        case EntryKind::LoopWhile: {
            collectValues(*data.expression, reads);
            const std::uint32_t top = here();
            program.instructions.push_back({.kind = InstructionKind::JumpUnless,
                                            .reads = readsValues(*data.expression),
                                            .expression = data.expression});
            compileBody(data.body, script, scripts, timing, program, reads, sets);
            program.instructions.push_back({.kind = InstructionKind::Jump, .target = top});
            program.instructions[top].target = here();
            break;
        }
        case EntryKind::When: {
            // Each arm: a test that skips the arm, the arm, and a jump past everything after it.
            std::vector<std::uint32_t> leaving;
            for (std::size_t n = 0; n < data.branches.size(); ++n) {
                const Branch& branch = data.branches[n];
                collectValues(*branch.condition, reads);
                const std::uint32_t test = here();
                program.instructions.push_back({.kind = InstructionKind::JumpUnless,
                                                .reads = readsValues(*branch.condition),
                                                .expression = branch.condition});
                compileBody(branch.body, script, scripts, timing, program, reads, sets);
                const bool more = n + 1 < data.branches.size() || !data.body.empty();
                if (more) {
                    leaving.push_back(here());
                    program.instructions.push_back({.kind = InstructionKind::Jump});
                }
                program.instructions[test].target = here();
            }
            compileBody(data.body, script, scripts, timing, program, reads, sets);
            for (const std::uint32_t jump : leaving) {
                program.instructions[jump].target = here();
            }
            break;
        }
        }
    }
}

// Whether following `edges` from `from` comes back to `start`.
[[nodiscard]] inline bool reaches(const std::vector<std::vector<std::uint32_t>>& edges, std::uint32_t from,
                                  std::uint32_t start, std::vector<bool>& seen) {
    for (const std::uint32_t next : edges[from]) {
        if (next == start) {
            return true;
        }
        if (!seen[next]) {
            seen[next] = true;
            if (reaches(edges, next, start, seen)) {
                return true;
            }
        }
    }
    return false;
}

struct ProcessorAccess;

}  // namespace detail

// The running state of a set of scripts — usually one actor's. Plain data: it is kept in the game's
// state, and a copy runs on by itself from where the original was.
//
// Each script is in one of three states. It is **off** until a trigger or the game starts it and
// after its body ends. It is **running** from then on. A running script is **held** while any other
// running script lists it in `interrupts`; a held script is not stepped, and carries on from exactly
// where it was when the last script holding it ends.
//
// **One tick** — one `advance` — steps every running script that is not held, from the highest
// `priority` to the lowest, scripts of one priority in the order of the processor's list. A script
// runs instructions from where it left off until one of these stops it for the tick:
//
//   - a `wait`, which lets go a number of ticks later;
//   - a `waitWhile` whose condition holds;
//   - the end of its body, which ends the script;
//   - the budget: `budget` instructions per script per tick, so a loop that never waits goes round a
//     little each tick;
//   - reaching an instruction that reads a value after asking for something on the same tick. What a
//     script asks for is written by the game before the next tick's values are gathered, so the script
//     reads on the next tick and sees what it asked for: `set(Count, Count + 1)` followed by
//     `when(Count >= 3, …)` tests the new count, and `say(…), waitWhile(MessageShowing)` holds until
//     the message it asked for is gone.
//
// What each line of a body costs: a command, a `set`, a `call`, a `stop`, a `restart`, a `wait` and a
// `waitWhile` are one instruction each; a `when` is one per arm tested, and one more to leave an arm
// that has others after it; a `loop` is one per trip, a `loopWhile` two.
class ScriptProcessor {
public:
    // Turns every script's body into instructions, checks it, and works out what each script reads
    // and sets. Every time in a `wait` becomes ticks here, through `config.timing.ticksForDuration`.
    // Throws std::logic_error, naming the script, for: two scripts of one name; a `call`, `stop`,
    // `restart` or `interrupts` naming a script that is not in `scripts`; a script that calls itself,
    // directly or through other scripts; scripts that interrupt each other in a ring. A null pointer
    // in `scripts` throws std::logic_error too.
    ScriptProcessor(std::vector<const Script*> scripts, const retropp::EngineConfig& config) {
        auto compiled = std::make_shared<detail::Compiled>();
        compiled->scripts = std::move(scripts);
        const std::vector<const Script*>& list = compiled->scripts;
        const std::size_t count = list.size();

        for (std::size_t n = 0; n < count; ++n) {
            if (list[n] == nullptr) {
                throw std::logic_error("ScriptKit::ScriptProcessor: a null script");
            }
            for (std::size_t earlier = 0; earlier < n; ++earlier) {
                if (list[earlier]->name == list[n]->name) {
                    throw std::logic_error("ScriptKit: two scripts are named '" + std::string(list[n]->name) + "'");
                }
            }
        }

        compiled->programs.resize(count);
        compiled->heldBy.resize(count);
        compiled->reads.resize(count);
        compiled->sets.resize(count);
        std::vector<std::vector<std::uint32_t>> calls(count);
        std::vector<std::vector<std::uint32_t>> holds(count);
        for (std::size_t n = 0; n < count; ++n) {
            const Script& script = *list[n];
            const detail::NodePtr& criteria = detail::ExprAccess::node(script.when);
            detail::collectValues(*criteria, compiled->reads[n]);
            compiled->criteria.push_back(criteria);
            detail::compileBody(script.body, script, list, config.timing, compiled->programs[n], compiled->reads[n],
                                compiled->sets[n]);
            for (const detail::Instruction& instruction : compiled->programs[n].instructions) {
                if (instruction.kind == detail::InstructionKind::Call) {
                    calls[n].push_back(instruction.target);
                }
            }
            for (const Script* other : script.interrupts) {
                const std::uint32_t held = detail::named(script, list, other, "interrupts");
                holds[n].push_back(held);
                compiled->heldBy[held].push_back(static_cast<std::uint32_t>(n));
            }
        }
        for (std::size_t n = 0; n < count; ++n) {
            const auto self = static_cast<std::uint32_t>(n);
            std::vector<bool> seen(count, false);
            if (detail::reaches(calls, self, self, seen)) {
                detail::refuse(*list[n], "calls itself");
            }
            seen.assign(count, false);
            if (detail::reaches(holds, self, self, seen)) {
                detail::refuse(*list[n], "interrupts itself, directly or through the scripts it interrupts");
            }
        }

        // A script reads and sets what the scripts it calls do, too. No script calls itself, so
        // following the calls ends.
        std::vector<bool> finished(count, false);
        const auto gather = [&](const auto& self, std::uint32_t index) -> void {
            if (finished[index]) {
                return;
            }
            finished[index] = true;
            for (const std::uint32_t callee : calls[index]) {
                self(self, callee);
                for (const ValueId id : compiled->reads[callee]) {
                    detail::note(compiled->reads[index], id);
                }
                for (const ValueId id : compiled->sets[callee]) {
                    detail::note(compiled->sets[index], id);
                }
            }
        };
        for (std::size_t n = 0; n < count; ++n) {
            gather(gather, static_cast<std::uint32_t>(n));
            for (const ValueId id : compiled->reads[n]) {
                detail::note(compiled->allReads, id);
            }
        }

        compiled->order.resize(count);
        for (std::size_t n = 0; n < count; ++n) {
            compiled->order[n] = static_cast<std::uint32_t>(n);
        }
        std::stable_sort(compiled->order.begin(), compiled->order.end(),
                         [&list](std::uint32_t a, std::uint32_t b) { return list[a]->priority > list[b]->priority; });

        states_.resize(count);
        compiled_ = std::move(compiled);
    }

    // The instructions each script may run in one tick. Below 1 is read as 1.
    int budget = 32;

    // Starts every script whose `on` equals `trigger` and whose `when` holds against `values`, unless
    // it is already running. Returns how many started. A default Trigger starts nothing. Throws
    // std::out_of_range, naming the id, when `values` lacks a value a matching script's `when` reads.
    int start(Trigger trigger, const Values& values) {
        if (!trigger.set()) {
            return 0;
        }
        int started = 0;
        for (std::size_t n = 0; n < states_.size(); ++n) {
            const Script& script = *compiled_->scripts[n];
            if (states_[n].running || !(script.on == trigger)) {
                continue;
            }
            if (detail::evaluate(*compiled_->criteria[n], values) == 0) {
                continue;
            }
            begin(static_cast<std::uint32_t>(n));
            ++started;
        }
        return started;
    }

    // Starts `script` whatever its `on` and `when` say, unless it is already running.
    void start(const Script& script) {
        const std::uint32_t index = require(script);
        if (!states_[index].running) {
            begin(index);
        }
    }

    // Ends `script`, wherever it is. The scripts it was holding carry on.
    void stop(const Script& script) { end(require(script)); }

    // Puts `script` back at its first instruction, running; a script that was off starts. A script
    // another running script interrupts stays held.
    void restart(const Script& script) { begin(require(script)); }

    // The three by-name calls above throw std::logic_error when `script` is not one of this
    // processor's.

    // One tick. Steps every running script that is not held, as the class comment sets out, and
    // records everything asked for, in the order asked, in a buffer this processor owns and reuses;
    // what the advance before asked for is dropped first.
    //
    // It reads `values`, the scripts and its own state, and writes only its own state and that buffer.
    // So it may run on any thread, and two processors may advance at the same moment; nothing else
    // touches a processor while it advances. Once the buffer has grown to what the scripts ask for in
    // a tick, an advance allocates nothing.
    //
    // Throws std::out_of_range, naming the id, when `values` lacks a value a script reads; handed every
    // value `reads()` names, it does not throw.
    void advance(const Values& values) {
        requestCount_ = 0;
        for (const std::uint32_t index : compiled_->order) {
            step(index, values);
        }
    }

    // What the last advance asked for, in order. The view holds until the next advance or
    // clearRequests. A request's text argument views the script's own text.
    [[nodiscard]] std::span<const Request> requests() const noexcept {
        return std::span<const Request>(requests_.data(), requestCount_);
    }

    // Drops what the last advance asked for, once it has been applied.
    void clearRequests() noexcept { requestCount_ = 0; }

    // Whether no script is running: an advance would do nothing.
    [[nodiscard]] bool idle() const noexcept {
        for (const State& state : states_) {
            if (state.running) {
                return false;
            }
        }
        return true;
    }

    // Every value any of the scripts reads, each once: what to hand to `start` and `advance`.
    [[nodiscard]] std::span<const ValueId> reads() const noexcept { return compiled_->allReads; }

    // Every value `script` reads — in its `when`, its conditions, its expressions and its commands'
    // arguments — and every value it sets, each once: its own first, then those of the scripts it
    // calls. Both throw std::out_of_range when `script` is not one of this processor's.
    [[nodiscard]] std::span<const ValueId> reads(const Script& script) const { return compiled_->reads[known(script)]; }
    [[nodiscard]] std::span<const ValueId> sets(const Script& script) const { return compiled_->sets[known(script)]; }

    // Where a script is, for a readout and for tests. Asked about a script that is not this
    // processor's, the first three are false and the last three throw std::out_of_range.

    // Started and not yet ended, held or not.
    [[nodiscard]] bool running(const Script& script) const noexcept {
        const std::uint32_t index = detail::indexOf(compiled_->scripts, &script);
        return index != detail::kNoScript && states_[index].running;
    }

    // Running, and held by another running script that interrupts it.
    [[nodiscard]] bool suspended(const Script& script) const noexcept {
        const std::uint32_t index = detail::indexOf(compiled_->scripts, &script);
        return index != detail::kNoScript && states_[index].running && held(index);
    }

    // Running, and holding when it was last stepped: at a `waitWhile` whose condition held, or in a
    // `wait` with more than one tick still to go. A script that passes through `wait(1)` every tick
    // is not waiting.
    [[nodiscard]] bool waiting(const Script& script) const noexcept {
        const std::uint32_t index = detail::indexOf(compiled_->scripts, &script);
        return index != detail::kNoScript && states_[index].running && states_[index].waiting;
    }

    // The instruction of its own body the script is on, from 0; while it is inside a `call`, that
    // call. 0 when it is off.
    [[nodiscard]] std::size_t position(const Script& script) const {
        const State& state = states_[known(script)];
        return state.running ? state.frames.front().at : 0;
    }

    // How many instructions the script's body is.
    [[nodiscard]] std::size_t length(const Script& script) const {
        return compiled_->programs[known(script)].instructions.size();
    }

    // How deep in calls the script is: 1 in its own body, 2 inside a `call`, 0 when it is off.
    [[nodiscard]] std::size_t depth(const Script& script) const {
        const State& state = states_[known(script)];
        return state.running ? state.frames.size() : 0;
    }

private:
    friend struct detail::ProcessorAccess;

    // A place in a body: frames[0] is the script's own, each one after it a `call`.
    struct Frame {
        std::uint32_t script = 0;   // whose body
        std::uint32_t at = 0;       // the instruction
    };

    struct State {
        bool running = false;
        bool waiting = false;         // its last step left it holding: see waiting()
        std::uint64_t waitLeft = 0;   // the ticks before the `wait` it is on lets go
        std::vector<Frame> frames;
    };

    [[nodiscard]] std::uint32_t require(const Script& script) const {
        const std::uint32_t index = detail::indexOf(compiled_->scripts, &script);
        if (index == detail::kNoScript) {
            throw std::logic_error("ScriptKit::ScriptProcessor: '" + std::string(script.name) +
                                   "' is not one of this processor's scripts");
        }
        return index;
    }

    [[nodiscard]] std::uint32_t known(const Script& script) const {
        const std::uint32_t index = detail::indexOf(compiled_->scripts, &script);
        if (index == detail::kNoScript) {
            throw std::out_of_range("ScriptKit::ScriptProcessor: '" + std::string(script.name) +
                                    "' is not one of this processor's scripts");
        }
        return index;
    }

    void begin(std::uint32_t index) {
        State& state = states_[index];
        state.running = true;
        state.waiting = false;
        state.waitLeft = 0;
        state.frames.assign(1, Frame{.script = index, .at = 0});
    }

    void end(std::uint32_t index) noexcept {
        State& state = states_[index];
        state.running = false;
        state.waiting = false;
        state.waitLeft = 0;
        state.frames.clear();
    }

    // Whether a running script interrupts this one.
    [[nodiscard]] bool held(std::uint32_t index) const noexcept {
        for (const std::uint32_t holder : compiled_->heldBy[index]) {
            if (states_[holder].running) {
                return true;
            }
        }
        return false;
    }

    // Leaves every body the script has reached the end of: a finished call returns to the instruction
    // after it, and the end of the script's own body ends the script. False when the script ended.
    [[nodiscard]] bool settle(std::uint32_t index) noexcept {
        State& state = states_[index];
        while (state.frames.back().at >= compiled_->programs[state.frames.back().script].instructions.size()) {
            state.frames.pop_back();
            if (state.frames.empty()) {
                end(index);
                return false;
            }
            ++state.frames.back().at;
        }
        return true;
    }

    [[nodiscard]] Request& nextRequest() {
        if (requestCount_ == requests_.size()) {
            requests_.emplace_back();
        }
        return requests_[requestCount_++];
    }

    void step(std::uint32_t index, const Values& values) {
        State& state = states_[index];
        if (!state.running || held(index)) {
            return;
        }
        if (state.waitLeft > 0) {
            if (--state.waitLeft > 0) {
                state.waiting = state.waitLeft > 1;
                return;
            }
            ++state.frames.back().at;
        }
        state.waiting = false;

        bool asked = false;
        for (int left = budget < 1 ? 1 : budget; left > 0; --left) {
            if (!settle(index)) {
                return;
            }
            const detail::Program& program = compiled_->programs[state.frames.back().script];
            const detail::Instruction& instruction = program.instructions[state.frames.back().at];
            if (asked && instruction.reads) {
                return;
            }
            switch (instruction.kind) {
            case detail::InstructionKind::Ask: {
                Request& request = nextRequest();
                detail::RequestAccess::ask(request, *instruction.command);
                for (std::uint32_t n = 0; n < instruction.parameterCount; ++n) {
                    const detail::Parameter& parameter = program.parameters[instruction.firstParameter + n];
                    detail::RequestAccess::add(
                        request, detail::Argument{.kind = parameter.kind,
                                                  .text = parameter.text,
                                                  .number = parameter.expression
                                                                ? detail::evaluate(*parameter.expression, values)
                                                                : parameter.number,
                                                  .type = parameter.type});
                }
                asked = true;
                ++state.frames.back().at;
                break;
            }
            case detail::InstructionKind::Set: {
                const std::int64_t to = detail::evaluate(*instruction.expression, values);
                instruction.writer(nextRequest(), instruction.value, to);
                asked = true;
                ++state.frames.back().at;
                break;
            }
            case detail::InstructionKind::Wait:
                if (instruction.ticks > 0) {
                    state.waitLeft = instruction.ticks;
                    state.waiting = instruction.ticks > 1;
                    return;
                }
                ++state.frames.back().at;
                break;
            case detail::InstructionKind::WaitWhile:
                if (detail::evaluate(*instruction.expression, values) != 0) {
                    state.waiting = true;
                    return;
                }
                ++state.frames.back().at;
                break;
            case detail::InstructionKind::JumpUnless:
                if (detail::evaluate(*instruction.expression, values) != 0) {
                    ++state.frames.back().at;
                } else {
                    state.frames.back().at = instruction.target;
                }
                break;
            case detail::InstructionKind::Jump:
                state.frames.back().at = instruction.target;
                break;
            case detail::InstructionKind::Call:
                state.frames.push_back(Frame{.script = instruction.target, .at = 0});
                break;
            case detail::InstructionKind::Stop:
                if (instruction.target == index) {
                    end(index);
                    return;
                }
                end(instruction.target);
                ++state.frames.back().at;
                break;
            case detail::InstructionKind::Restart:
                begin(instruction.target);
                if (instruction.target == index) {
                    return;
                }
                ++state.frames.back().at;
                break;
            }
        }
        // Out of budget: a script whose last instruction this was ends now, not a tick later.
        (void)settle(index);
    }

    std::shared_ptr<const detail::Compiled> compiled_;
    std::vector<State> states_;
    std::vector<Request> requests_;   // kept from tick to tick; the first requestCount_ are this tick's
    std::size_t requestCount_ = 0;
    Values gathered_;                 // where stepScripts gathers this processor's values
};

namespace detail {

// Reaches the buffer stepScripts gathers a processor's values into.
struct ProcessorAccess {
    [[nodiscard]] static Values& gathered(ScriptProcessor& processor) noexcept { return processor.gathered_; }
};

}  // namespace detail

}  // namespace ScriptKit
