// What both arrests do, so both call it: shout, close in a step a tick until he has you, say so, and,
// once that has been read, call for backup. Once the patrol car has taken him away it asks, over and
// over, for the press that `carry on` answers. It has no trigger of its own.
#pragma once

#include "../crossing.h"
#include "backup.h"

namespace crossing {

inline const ScriptKit::Script apprehend{
    .name = "apprehend",
    .body = {
        set(Wanted, true),
        say("HEY! STOP!", Bubble::Shout),
        loopWhile(!Caught, {stepToward(PlayerX, PlayerY), wait(1)}),
        set(Arrested, true),
        say("You're under\narrest."), waitWhile(MessageShowing),
        call(backup),
        loop({say("Press to continue."), waitWhile(MessageShowing), wait(2s)}),
    },
};

}  // namespace crossing
