// The signal's cycle: green for five seconds, yellow for one, red for three, forever. It starts when the
// program does. The walk phase holds it still, and restarts it when the walk is over.
#pragma once

#include "../crossing.h"

namespace crossing {

inline const ScriptKit::Script traffic{
    .name = "traffic",
    .on = Begin,
    .body = {loop({
        set(Signal, Lamp::Green), wait(5s),
        set(Signal, Lamp::Yellow), wait(1s),
        set(Signal, Lamp::Red), wait(3s),
    })},
};

}  // namespace crossing
