// The ambulance's run. It sets off with its lights going, and every car holds while it is out. It
// drives up to the officer; its two medics get out, walk to him, lift him and carry him back; and it
// drives off the far edge of the screen. It has no trigger: `officer down` calls it.
#pragma once

#include "../crossing.h"
#include "siren.h"

namespace crossing {

inline const ScriptKit::Script ambulance{
    .name = "ambulance",
    .body = {
        set(AmbulanceOut, true),
        restart(siren),
        loopWhile(!AmbulanceAtOfficer, {driveAmbulance(), wait(1)}),
        set(MedicsOut, true),
        loopWhile(!MedicsAtOfficer, {walkMedics(Toward::Officer), wait(2)}),
        wait(500ms),
        set(OfficerCarried, true),
        loopWhile(!MedicsAtAmbulance, {walkMedics(Toward::Ambulance), wait(2)}),
        set(OfficerAboard, true), set(OfficerCarried, false), set(MedicsOut, false),
        wait(500ms),
        loopWhile(!AmbulanceGone, {driveAmbulance(), wait(1)}),
        stop(siren),
        set(AmbulanceOut, false),
    },
};

}  // namespace crossing
