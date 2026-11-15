// Stepping off the curb on a red without the walk sign: the officer's watch is held, and `apprehend`
// does the rest.
#pragma once

#include "../crossing.h"
#include "apprehend.h"
#include "watch.h"

namespace crossing {

inline const ScriptKit::Script arrest{
    .name = "arrest",
    .on = StepOffCurb,
    .when = Signal == Lamp::Red && !WalkLit && !Wanted,
    .interrupts = {&watch},
    .body = {call(apprehend)},
};

}  // namespace crossing
