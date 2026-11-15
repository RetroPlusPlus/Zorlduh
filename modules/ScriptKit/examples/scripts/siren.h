// The ambulance's light bar: red, blue, red, blue, a tenth of a second each. It has no trigger; the
// ambulance's run starts it when it sets off and stops it when it has gone.
#pragma once

#include "../crossing.h"

namespace crossing {

inline const ScriptKit::Script siren{
    .name = "siren",
    .body = {loop({set(SirenRed, true), wait(100ms), set(SirenRed, false), wait(100ms)})},
};

}  // namespace crossing
