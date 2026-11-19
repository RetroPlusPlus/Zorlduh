// A hen's first script: when the camera comes into the field, the hen runs the way it flees for half a
// second. Each hen is handed its own way.
#pragma once

#include "../farm.h"

namespace farm {

inline ScriptKit::Script scatter(retropp::Dir flees) {
    return {
        .name = "scatter",
        .on = Enter,
        .body = {set(Heading, flees), wait(500ms), set(Heading, retropp::Dir::None)},
    };
}

}  // namespace farm
