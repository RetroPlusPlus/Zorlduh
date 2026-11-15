// Backup: a patrol car answering an arrest. It sets off with its lights going, from whichever side of
// the screen is nearer to you, and every car holds while it is out. It drives straight at you, wherever
// you are, and into you. Once its driver has had his say, the officer who stopped you walks to it and
// gets in, and they speed off, three times as fast as they came. It has no trigger: `apprehend` calls
// it.
#pragma once

#include "../crossing.h"
#include "siren.h"

namespace crossing {

inline const ScriptKit::Script backup{
    .name = "backup",
    .body = {
        set(PatrolOut, true),
        restart(siren),
        loopWhile(!PatrolAtPlayer, {drivePatrol(), wait(1)}),
        say("Whoops! We gotta\nget outta here!"), waitWhile(MessageShowing),
        loopWhile(!OfficerAtPatrol, {stepToward(PatrolDoorX, PatrolDoorY), wait(1)}),
        set(OfficerAboard, true),
        wait(500ms),
        loopWhile(!PatrolGone, {drivePatrol(6), wait(1)}),
        stop(siren),
        set(PatrolOut, false),
    },
};

}  // namespace crossing
