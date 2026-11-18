// An egg's script: the hero walking onto it picks it up — the farm counts one more egg, and the egg is
// gone. The count is a command, so every egg picked up on one tick is counted.
#pragma once

#include "../farm.h"

namespace farm {

inline const ScriptKit::Script pickUp{
    .name = "pick up",
    .on = Touch,
    .body = {collect(), ActorKit::remove()},
};

}  // namespace farm
