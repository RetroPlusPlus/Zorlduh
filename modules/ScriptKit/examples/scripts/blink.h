// One blink of the lit walk sign: dark for a quarter of a second, lit for a quarter. It has no trigger;
// the walk phase calls it.
#pragma once

#include "../crossing.h"

namespace crossing {

inline const ScriptKit::Script blink{
    .name = "blink",
    .body = {set(WalkDim, true), wait(250ms), set(WalkDim, false), wait(250ms)},
};

}  // namespace crossing
