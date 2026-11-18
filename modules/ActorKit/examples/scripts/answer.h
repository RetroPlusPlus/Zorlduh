// The dog's second script: once it is awake, the bell sends it trotting over, and after a wait it goes
// back to its kennel. It steers its walker through its own Heading, reading where it is from ActorKit::X.
#pragma once

#include "../farm.h"

namespace farm {

inline const ScriptKit::Script answer{
    .name = "answer",
    .on = Becomes(BellRung),
    .when = Awake,
    .body = {
        loopWhile(ActorKit::X < kBellX - 16, {set(Heading, retropp::Dir::Right), wait(1)}),
        set(Heading, retropp::Dir::None),
        wait(2s),
        loopWhile(ActorKit::X > kKennelX, {set(Heading, retropp::Dir::Left), wait(1)}),
        set(Heading, retropp::Dir::None),
    },
};

}  // namespace farm
