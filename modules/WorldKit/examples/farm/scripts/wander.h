// A hen's third script: from a second after the camera comes into the field, the hen wanders — it walks
// for most of a second, stands for about a second, turns clockwise and walks again, round and round. Each
// hen is handed the way it sets off first. It steers its walker through its own Heading, and the walker
// stops it at the fence and at anything solid in its way.
#pragma once

#include <utility>

#include "../farm.h"

namespace farm {

// The direction a quarter turn clockwise from `way`.
constexpr retropp::Dir clockwise(retropp::Dir way) {
    switch (way) {
        case retropp::Dir::Up: return retropp::Dir::Right;
        case retropp::Dir::Right: return retropp::Dir::Down;
        case retropp::Dir::Down: return retropp::Dir::Left;
        case retropp::Dir::Left: return retropp::Dir::Up;
        case retropp::Dir::None:
        default: return retropp::Dir::None;
    }
}

inline ScriptKit::Script wander(retropp::Dir first) {
    // One round: a leg each way, starting with `first`.
    ScriptKit::Body round;
    retropp::Dir way = first;
    for (int leg = 0; leg < 4; ++leg) {
        round.push_back(set(Heading, way));
        round.push_back(wait(700ms));
        round.push_back(set(Heading, retropp::Dir::None));
        round.push_back(wait(900ms));
        way = clockwise(way);
    }
    return {
        .name = "wander",
        .on = Enter,
        .body = {wait(1s), ScriptKit::loop(std::move(round))},
    };
}

}  // namespace farm
