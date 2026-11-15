// The button's second script, started by the same press as the walk phase: what the button says back.
// It counts the presses it has answered, and holds until its message has been read, so a press while
// it is still talking is not answered twice.
#pragma once

#include "../crossing.h"

namespace crossing {

inline const ScriptKit::Script pressed{
    .name = "pressed",
    .on = Interact,
    .body = {
        when(WalkLit, {say("Walk now.")})
            .orWhen(Presses >= 2, {say("PRESSING HARDER\nWON'T HELP.", Bubble::Shout)})
            .otherwise({say("Wait for the\nwalk sign.")}),
        set(Presses, Presses + 1),
        waitWhile(MessageShowing),
    },
};

}  // namespace crossing
