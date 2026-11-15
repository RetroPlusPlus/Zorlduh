// Bindings and Handlers — the game's side of its scripts — and the two calls of the tick,
// applyScripts and stepScripts.
//
//     const ScriptKit::Bindings<State> bindings{
//         {Ready,  &State::ready},                                    // read and written
//         {PlayerX, [](const State& state) { return state.player.x; }}, // read only
//     };
//     const ScriptKit::Handlers<State> handlers{
//         {Say, [](State& state, const ScriptKit::Arguments& arguments) { state.say(arguments.text(0)); }},
//     };
//
//     // The tick.
//     ScriptKit::applyScripts({&state.host, &state.lamp}, bindings, handlers, state);   // first
//     // … the game's own logic: input, movement, starting scripts …
//     ScriptKit::stepScripts({&state.host, &state.lamp}, bindings, state);              // last
//
// Bindings say how each value is read from the game's state and written to it; Handlers say what the
// game does for each command. Both are templated on the game's state type and built once.
#pragma once

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ScriptKit/command.h"
#include "ScriptKit/script_processor.h"
#include "ScriptKit/value.h"

namespace ScriptKit {

template <class State>
class Bindings;

// How one value is read from a State and, when it can be, written to it. Written as one element of a
// Bindings list, in one of three forms:
//
//     {Score, &State::score}                        a member of the value's exact type: read and written
//     {Near,  [](const State& s) { return … ; }}    a getter: read only
//     {Lamp,  getter, [](State& s, Color c) { … }}  a getter and a setter
template <class State>
class Binding {
public:
    template <ValueType T>
    Binding(Value<T> value, T State::* member)
        : id_(value.id()),
          read_([member](const State& state) { return detail::encode(state.*member); }),
          write_([member](State& state, std::int64_t raw) { state.*member = detail::decode<T>(raw); }) {}

    template <ValueType T, class Get>
        requires(!std::is_member_pointer_v<Get>) && std::is_invocable_r_v<T, Get, const State&>
    Binding(Value<T> value, Get get)
        : id_(value.id()),
          read_([get = std::move(get)](const State& state) { return detail::encode(static_cast<T>(get(state))); }) {}

    template <ValueType T, class Get, class Set>
        requires std::is_invocable_r_v<T, Get, const State&> && std::invocable<Set, State&, T>
    Binding(Value<T> value, Get get, Set set)
        : id_(value.id()),
          read_([get = std::move(get)](const State& state) { return detail::encode(static_cast<T>(get(state))); }),
          write_([set = std::move(set)](State& state, std::int64_t raw) { set(state, detail::decode<T>(raw)); }) {}

private:
    friend class Bindings<State>;

    ValueId id_;
    std::function<std::int64_t(const State&)> read_;
    std::function<void(State&, std::int64_t)> write_;   // empty when the value is read only
};

// How a game reads and writes each value its scripts use. Built once, from a list of Binding; the
// game names its state type once, as the template argument. A value bound twice keeps its first
// binding.
template <class State>
class Bindings {
public:
    Bindings(std::initializer_list<Binding<State>> bindings) : bindings_(bindings) {
        std::stable_sort(bindings_.begin(), bindings_.end(),
                         [](const Binding<State>& a, const Binding<State>& b) { return a.id_ < b.id_; });
    }

    // The values `ids` name, read from `state` now. Throws std::out_of_range naming an id that has no
    // binding.
    [[nodiscard]] Values gather(const State& state, std::span<const ValueId> ids) const {
        Values values;
        gather(values, state, ids);
        return values;
    }

    // The same, into `values`, which is emptied first and keeps its storage: gathering into one Values
    // tick after tick allocates nothing.
    void gather(Values& values, const State& state, std::span<const ValueId> ids) const {
        values.clear();
        for (const ValueId id : ids) {
            const Binding<State>* binding = find(id);
            if (binding == nullptr) {
                throw std::out_of_range("ScriptKit::Bindings: no binding for the value with id " + std::to_string(id));
            }
            values.setRaw(id, binding->read_(state));
        }
    }

    // Writes the Change `request` holds into `state` through the value's binding, and returns true.
    // Returns false, writing nothing, when `request` is a command or the value is bound read only.
    // Throws std::out_of_range naming the id when the value has no binding.
    bool apply(State& state, const Request& request) const {
        if (!detail::RequestAccess::isChange(request)) {
            return false;
        }
        const ValueId id = detail::RequestAccess::changed(request);
        const Binding<State>* binding = find(id);
        if (binding == nullptr) {
            throw std::out_of_range("ScriptKit::Bindings: no binding for the value with id " + std::to_string(id));
        }
        if (!binding->write_) {
            return false;
        }
        binding->write_(state, detail::RequestAccess::changedTo(request));
        return true;
    }

private:
    [[nodiscard]] const Binding<State>* find(ValueId id) const noexcept {
        const auto found = std::lower_bound(bindings_.begin(), bindings_.end(), id,
                                            [](const Binding<State>& binding, ValueId wanted) { return binding.id_ < wanted; });
        return found != bindings_.end() && found->id_ == id ? &*found : nullptr;
    }

    std::vector<Binding<State>> bindings_;   // by id
};

template <class State>
class Handlers;

// What the game does for one command: {Say, [](State& state, const Arguments& arguments) { … }}.
template <class State>
class Handler {
public:
    template <class Run>
        requires std::invocable<Run, State&, const Arguments&>
    Handler(const Command& command, Run run) : id_(command.id()), run_(std::move(run)) {}

private:
    friend class Handlers<State>;

    CommandId id_;
    std::function<void(State&, const Arguments&)> run_;
};

// What a game does for each command: one function per command, keyed by the command. A command
// listed twice keeps its first function.
template <class State>
class Handlers {
public:
    Handlers(std::initializer_list<Handler<State>> handlers) : handlers_(handlers) {
        std::stable_sort(handlers_.begin(), handlers_.end(),
                         [](const Handler<State>& a, const Handler<State>& b) { return a.id_ < b.id_; });
    }

    // Runs the function of the command `request` asks for, with its arguments. Does nothing when
    // `request` is a Change. Throws std::out_of_range naming the command when it has no function.
    void apply(State& state, const Request& request) const {
        const Command* command = request.command();
        if (command == nullptr) {
            return;
        }
        const auto found = std::lower_bound(handlers_.begin(), handlers_.end(), command->id(),
                                            [](const Handler<State>& handler, CommandId wanted) { return handler.id_ < wanted; });
        if (found == handlers_.end() || found->id_ != command->id()) {
            throw std::out_of_range("ScriptKit::Handlers: no handler for command '" + std::string(command->name()) + "'");
        }
        found->run_(state, request.arguments());
    }

private:
    std::vector<Handler<State>> handlers_;   // by command id
};

// The first call of the tick. Writes what each processor's last advance asked for — processor by
// processor in the order given, request by request in the order asked: a Change through `bindings`,
// a command through `handlers` — then drops it, so nothing is applied twice. This is the one place a
// script's effects are written, on the thread that runs the game's tick.
template <class State>
void applyScripts(std::span<ScriptProcessor* const> processors, const Bindings<State>& bindings,
                  const Handlers<State>& handlers, State& state) {
    for (ScriptProcessor* processor : processors) {
        for (const Request& request : processor->requests()) {
            if (request.command() != nullptr) {
                handlers.apply(state, request);
            } else {
                bindings.apply(state, request);
            }
        }
        processor->clearRequests();
    }
}

// The last call of the tick. For every processor that is not idle, gathers the values its scripts
// read through `bindings`; then advances each of them. Every processor's values are gathered before
// any processor advances, so none sees what another did this tick, and the advances may run in any
// order — or at once — with one result.
template <class State>
void stepScripts(std::span<ScriptProcessor* const> processors, const Bindings<State>& bindings, const State& state) {
    for (ScriptProcessor* processor : processors) {
        if (!processor->idle()) {
            bindings.gather(detail::ProcessorAccess::gathered(*processor), state, processor->reads());
        }
    }
    for (ScriptProcessor* processor : processors) {
        if (!processor->idle()) {
            processor->advance(detail::ProcessorAccess::gathered(*processor));
        }
    }
}

// Both calls take the processors as a braced list of addresses too:
// applyScripts({&host, &lamp}, bindings, handlers, state).
template <class State>
void applyScripts(std::initializer_list<ScriptProcessor*> processors, const Bindings<State>& bindings,
                  const Handlers<State>& handlers, State& state) {
    applyScripts(std::span<ScriptProcessor* const>(processors.begin(), processors.size()), bindings, handlers, state);
}

template <class State>
void stepScripts(std::initializer_list<ScriptProcessor*> processors, const Bindings<State>& bindings,
                 const State& state) {
    stepScripts(std::span<ScriptProcessor* const>(processors.begin(), processors.size()), bindings, state);
}

}  // namespace ScriptKit
