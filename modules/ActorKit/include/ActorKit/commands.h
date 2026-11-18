// What a script asks of ActorKit — spawn and remove — and the game's handlers for its own commands.
//
//     ScriptKit::Script lay(const ActorKit::Actor& egg) {
//         return {.name = "lay", .on = Enter, .body = {wait(4s), ActorKit::spawn(egg, {4, 8})}};
//     }
//     const ScriptKit::Script pickUp{.name = "pick up", .on = Touch, .body = {ActorKit::remove()}};
//
//     const ActorKit::Handlers<State> handlers{
//         {Cluck, [](State& state, ActorKit::Id asker, const ScriptKit::Arguments& arguments) { … }},
//     };
//
// spawn and remove are answered by ActorKit itself. Every other command a script asks for goes to the
// game's handler for it, which is handed the id of the actor whose script asked.
#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "retropp/geometry.h"

#include "ScriptKit/command.h"
#include "ScriptKit/entry.h"

#include "ActorKit/actor.h"

namespace ActorKit {

namespace detail {

inline const ScriptKit::Command SpawnCommand{"ActorKit spawn"};
inline const ScriptKit::Command RemoveCommand{"ActorKit remove"};

// Every actor value a script's spawn names, kept for the life of the program; a spawn entry carries its
// place in this table.
inline std::vector<std::shared_ptr<const Actor>>& spawnTable() {
    static std::vector<std::shared_ptr<const Actor>> table;
    return table;
}

}  // namespace detail

// In a body: spawns a copy of `actor` in the area of the actor whose script runs it, at that actor's
// position plus `offset`. The new actor takes part from the next tick. The value is copied when the
// entry is written, so a value made on the spot is kept.
[[nodiscard]] inline ScriptKit::Entry spawn(const Actor& actor, retropp::Vec2i offset = {}) {
    std::vector<std::shared_ptr<const Actor>>& table = detail::spawnTable();
    table.push_back(std::make_shared<const Actor>(actor));
    return detail::SpawnCommand(static_cast<int>(table.size() - 1), offset.x, offset.y);
}

// In a body: removes the actor whose script runs it, once the tick's requests are all written.
[[nodiscard]] inline ScriptKit::Entry remove() { return detail::RemoveCommand(); }

template <class State>
class Handlers;

// What the game does for one of its commands, handed the asking actor's id:
// {Cluck, [](State& state, ActorKit::Id asker, const ScriptKit::Arguments& arguments) { … }}.
template <class State>
class Handler {
public:
    template <class Run>
        requires std::invocable<Run, State&, Id, const ScriptKit::Arguments&>
    Handler(const ScriptKit::Command& command, Run run) : command_(&command), run_(std::move(run)) {}

private:
    friend class Handlers<State>;

    const ScriptKit::Command* command_;
    std::function<void(State&, Id, const ScriptKit::Arguments&)> run_;
};

// What the game does for each of its commands: one function per command. A command listed twice keeps
// its first function. Built once; the commands outlive it.
template <class State>
class Handlers {
public:
    Handlers(std::initializer_list<Handler<State>> handlers) : handlers_(handlers) {}

    // Runs the function of the command `request` asks for, with the asker's id and the arguments. A
    // request that is a change is left for the bindings. Throws std::out_of_range naming the command for
    // a command missing from the list.
    void apply(State& state, Id asker, const ScriptKit::Request& request) const {
        const ScriptKit::Command* command = request.command();
        if (command == nullptr) {
            return;
        }
        const auto found = std::find_if(handlers_.begin(), handlers_.end(), [&](const Handler<State>& handler) {
            return handler.command_->id() == command->id();
        });
        if (found == handlers_.end()) {
            throw std::out_of_range("ActorKit::Handlers: no handler for command '" + std::string(command->name()) +
                                    "'");
        }
        found->run_(state, asker, request.arguments());
    }

private:
    std::vector<Handler<State>> handlers_;
};

}  // namespace ActorKit
