// Arriving on the other side without the walk sign: you beat the cars, and the officer is waiting.
#pragma once

#include "../crossing.h"
#include "apprehend.h"
#include "watch.h"

namespace crossing {

inline const ScriptKit::Script arrestOnArrival{
    .name = "arrest on arrival",
    .on = ReachFarSide,
    .when = !WalkLit && !Wanted,
    .interrupts = {&watch},
    .body = {call(apprehend)},
};

}  // namespace crossing
