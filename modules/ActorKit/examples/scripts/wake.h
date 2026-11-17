// The dog's first script: it wakes when the hero comes within 24 pixels.
#pragma once

#include "../farm.h"

namespace farm {

inline const ScriptKit::Script wake{
    .name = "wake",
    .on = Near(24),
    .body = {set(Awake, true)},
};

}  // namespace farm
