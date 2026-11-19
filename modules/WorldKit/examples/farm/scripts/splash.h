// The pond's script: from the moment the hero steps in until it steps out, every kStepTicks the pond looks
// at where the hero stands, and if the hero has moved since the last ripple, the pond's sprite ripples out
// from the hero's feet. The hero is the player, so PlayerX and PlayerY are its position; less
// the pond's own X and Y, the point is in the pond's art pixels. SteppedX and SteppedY, the pond's own, hold
// where the hero stood at the last ripple, so a hero standing still makes one ripple only.
#pragma once

#include "../farm.h"

namespace farm {

inline const ScriptKit::Script splash{
    .name = "splash",
    .on = Touch,
    .body = {
        loopWhile(ActorKit::Touched,
                  {
                      when(ActorKit::PlayerX != SteppedX || ActorKit::PlayerY != SteppedY,
                           {
                               ripple(ActorKit::PlayerX + kFeetX - ActorKit::X,
                                      ActorKit::PlayerY + kFeetY - ActorKit::Y),
                               set(SteppedX, ActorKit::PlayerX),
                               set(SteppedY, ActorKit::PlayerY),
                           }),
                      wait(kStepTicks),
                  }),
    },
};

}  // namespace farm
