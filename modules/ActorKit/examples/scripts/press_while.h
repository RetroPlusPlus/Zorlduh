// A switch any actor can list: touched, it makes `pressed` true, holds while `held` does, and makes it
// false again. Written once as a function and handed the values each actor wants it to act on — the
// puddle splashes while the hero stands in it with pressWhile(Splashing, ActorKit::Touched).
#pragma once

#include "../farm.h"

namespace farm {

inline ScriptKit::Script pressWhile(ScriptKit::Value<bool> pressed, ScriptKit::Value<bool> held) {
    return {
        .name = "press",
        .on = Touch,
        .body = {set(pressed, true), waitWhile(held), set(pressed, false)},
    };
}

}  // namespace farm
