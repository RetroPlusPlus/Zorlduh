// Hit by a car. The walk phase ends, and you stay down while the message shows — and, when the officer
// went down too or it was the patrol car that hit you, until you carry on. Then everyone is put back and
// the signal's cycle starts over. Its priority is the highest on the signal's processor, so it is
// stepped before anything else there.
#pragma once

#include "../crossing.h"
#include "traffic.h"
#include "walk_phase.h"

namespace crossing {

inline const ScriptKit::Script struck{
    .name = "struck",
    .on = HitByCar,
    .priority = 2,
    .body = {
        set(Struck, true),
        stop(walkPhase), set(WalkLit, false), set(WalkDim, false),
        say("WHAM!", Bubble::Shout), waitWhile(MessageShowing),
        waitWhile(OfficerDown || Arrested),
        sendBack(),
        set(Presses, 0), set(Struck, false),
        restart(traffic),
    },
};

}  // namespace crossing
