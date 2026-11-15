// The press that carries on once the ambulance or the patrol car has taken the officer away: whichever
// script was asking for it stops, and everyone is put back, the officer at his post again. Before the
// officer is aboard, and while either of the two is still out, its criteria fail and the press does
// nothing.
#pragma once

#include "../crossing.h"
#include "arrest.h"
#include "arrest_on_arrival.h"
#include "officer_down.h"

namespace crossing {

inline const ScriptKit::Script carryOn{
    .name = "carry on",
    .on = Confirm,
    .when = OfficerAboard && !AmbulanceOut && !PatrolOut,
    .priority = 3,
    .body = {
        stop(officerDown), stop(arrest), stop(arrestOnArrival),
        sendBack(),
        set(OfficerAboard, false), set(Arrested, false), set(Wanted, false), set(OfficerDown, false),
    },
};

}  // namespace crossing
