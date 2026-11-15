// The officer on watch: a look each way, forever. An arrest holds it still, and it carries on from
// where it was when the arrest is over.
#pragma once

#include "../crossing.h"

namespace crossing {

inline const ScriptKit::Script watch{
    .name = "watch",
    .on = Begin,
    .body = {loop({face(retropp::Dir::Left), wait(1s), face(retropp::Dir::Right), wait(1s)})},
};

}  // namespace crossing
