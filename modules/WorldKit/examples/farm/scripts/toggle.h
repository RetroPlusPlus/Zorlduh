// A switch any actor can list: each Interact flips the value it is handed. The gate lists
// toggle(GateOpen), so pressing Interact at it opens it and pressing again shuts it.
#pragma once

#include "../farm.h"

namespace farm {

inline ScriptKit::Script toggle(ScriptKit::Value<bool> value) {
    return {
        .name = "toggle",
        .on = Interact,
        .body = {set(value, !value)},
    };
}

}  // namespace farm
