// A hint every eight seconds. It keeps quiet while the officer is after you, holds while any message
// is up, so it never talks over one, and runs until someone crosses properly and `crossed` stops it.
#pragma once

#include "../crossing.h"

namespace crossing {

inline const ScriptKit::Script reminder{
    .name = "reminder",
    .on = Begin,
    .body = {loop({wait(8s), waitWhile(Wanted), say("Press the button\nto cross."), waitWhile(MessageShowing)})},
};

}  // namespace crossing
