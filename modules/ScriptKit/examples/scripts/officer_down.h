// A car reaches the officer. Whatever arrest he was making is over, and if he had you, you are free to
// move again. The ambulance stops every car, so it holds off while you are still in the street and not
// yet hit; then it comes, its medics carry him aboard, and you are left where you are. After that it
// asks, over and over, for the press that `carry on` answers.
#pragma once

#include "../crossing.h"
#include "ambulance.h"
#include "arrest.h"
#include "arrest_on_arrival.h"
#include "watch.h"

namespace crossing {

inline const ScriptKit::Script officerDown{
    .name = "officer down",
    .on = OfficerHit,
    .priority = 2,
    .interrupts = {&watch},
    .body = {
        set(OfficerDown, true),
        stop(arrest), stop(arrestOnArrival),
        set(Arrested, false),
        say("OOF!", Bubble::Shout),
        waitWhile(InStreet && !Struck),
        call(ambulance),
        loop({say("Press to continue."), waitWhile(MessageShowing), wait(2s)}),
    },
};

}  // namespace crossing
