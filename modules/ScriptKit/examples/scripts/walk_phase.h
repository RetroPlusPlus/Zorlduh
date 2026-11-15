// The button's first script. It holds the signal's cycle still, takes the signal to red if it is not
// red yet, lights the walk sign for four seconds, blinks it three times, forgets the presses, and
// starts the cycle over. Its priority puts it ahead of `pressed`, which the same press starts.
#pragma once

#include "../crossing.h"
#include "blink.h"
#include "traffic.h"

namespace crossing {

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

}  // namespace crossing
