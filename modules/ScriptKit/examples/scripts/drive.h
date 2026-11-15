// A car's script: drive, hold at the line while the signal is red, and hold wherever it is while the
// ambulance or the patrol car is out. A script is ordinary C++ data, so one function builds all six cars' scripts, each
// over its own car's value.
#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "../crossing.h"

namespace crossing {

inline ScriptKit::Script drive(std::string_view name, int car) {
    const ScriptKit::Value<bool> atLine = AtLine[static_cast<std::size_t>(car)];
    return {
        .name = name,
        .on = Begin,
        .body = {loop({
            waitWhile((atLine && Signal == Lamp::Red) || AmbulanceOut || PatrolOut),
            advance(car), wait(1),
        })},
    };
}

inline const std::array<ScriptKit::Script, kCars> driving{
    drive("east 1", 0), drive("east 2", 1), drive("east 3", 2),
    drive("west 1", 3), drive("west 2", 4), drive("west 3", 5),
};

}  // namespace crossing
