// Reaching the other side with the walk sign lit: no more reminders, and a word of thanks. Without the
// walk sign its criteria fail, and the officer's `arrest on arrival` answers the same trigger.
#pragma once

#include "../crossing.h"
#include "reminder.h"

namespace crossing {

inline const ScriptKit::Script crossed{
    .name = "crossed",
    .on = ReachFarSide,
    .when = WalkLit,
    .body = {stop(reminder), say("Safely across."), waitWhile(MessageShowing)},
};

}  // namespace crossing
