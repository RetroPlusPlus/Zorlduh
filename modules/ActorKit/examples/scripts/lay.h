// A hen's second script: once the camera has come into the field, every four seconds the hen lays an
// egg where it stands, three in all, counted in its own Laid. The hen wanders between eggs, so they lie
// apart.
#pragma once

#include "../farm.h"

namespace farm {

inline ScriptKit::Script lay(const ActorKit::Actor& egg) {
    return {
        .name = "lay",
        .on = Enter,
        .body = {
            loopWhile(Laid < 3, {wait(4s), ActorKit::spawn(egg, {0, 6}), set(Laid, Laid + 1)}),
        },
    };
}

}  // namespace farm
