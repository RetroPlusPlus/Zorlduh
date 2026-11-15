// Script — a rule over a program: what sets it off, when it applies, and the body it runs.
//
//     const ScriptKit::Script greet{
//         .name = "greet",
//         .on   = Interact,
//         .when = !Greeted,
//         .body = {
//             say("Welcome."), waitWhile(MessageShowing),
//             set(Greeted, true),
//         },
//     };
//
// A script is data. A script processor built over it runs it (script_processor.h).
#pragma once

#include <string_view>
#include <vector>

#include "ScriptKit/entry.h"
#include "ScriptKit/expression.h"
#include "ScriptKit/trigger.h"
#include "ScriptKit/value.h"

namespace ScriptKit {

// A script. The fields before `body` are the rule; `body` is the program. Written with designated
// initializers, in the order the fields are declared.
//
// A script names the values it reads and sets where it uses them, in `when` and in its body; nothing
// lists them a second time. A script processor works out both lists when it is built, and reports
// them: ScriptProcessor::reads and ScriptProcessor::sets.
//
// A script is referred to by address wherever it is named — a processor's list, `interrupts`, `call`,
// `stop`, `restart` — so it is declared once, as a constant, and outlives every processor that names
// it. `name` is a view: the text it views outlives the script, as a string literal does.
struct Script {
    std::string_view name;                   // in every error about the script, and in a readout
    Trigger on{};                            // what sets it off; left unset, only the game starts it
    int from = 0;                            // a side or a source, in the game's own enum; ScriptKit stores it
    Expr<bool> when = true;                  // holds for the trigger to start it
    int priority = 0;                        // higher is stepped first each tick
    std::vector<const Script*> interrupts;   // held still while this one runs
    Body body;
};

}  // namespace ScriptKit
