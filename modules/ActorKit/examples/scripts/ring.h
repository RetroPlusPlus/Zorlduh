// The bell's script: rung from below, it rings for a second. Its look swings while BellRung holds.
#pragma once

#include "../farm.h"

namespace farm {

inline const ScriptKit::Script ring{
    .name = "ring",
    .on = Interact,
    .from = ActorKit::Below,
    .body = {
        set(BellRung, true), wait(1s), set(BellRung, false),
    },
};

}  // namespace farm
