// What the street crossing's scripts are written in: its values, its triggers and its commands. Each
// script is a file of its own in scripts/, and every one of them includes this.
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <string_view>

#include "retropp/input_actions.h"

#include "ScriptKit/command.h"
#include "ScriptKit/entry.h"
#include "ScriptKit/expression.h"
#include "ScriptKit/script.h"
#include "ScriptKit/trigger.h"
#include "ScriptKit/value.h"

namespace crossing {

// A braced body and a bare count give name lookup no ScriptKit type to follow, so these two are named;
// the literals are what let a script say wait(5s) and wait(250ms).
using ScriptKit::loop;
using ScriptKit::wait;
using namespace std::chrono_literals;

enum class Lamp : std::uint8_t { Green, Yellow, Red };
enum class Bubble : std::uint8_t { Plain, Shout };
enum class Toward : std::uint8_t { Officer, Ambulance };

// Six cars, numbered 0 to 5: three eastbound, then three westbound.
inline constexpr int kCarsPerLane = 3;
inline constexpr int kCars = 2 * kCarsPerLane;

// Values, each with its real type. Declared once; the compiler checks every script against them.
inline const ScriptKit::Value<Lamp> Signal;             // the traffic signal's lamp
inline const ScriptKit::Value<bool> WalkLit;            // the walk sign shows "walk"
inline const ScriptKit::Value<bool> WalkDim;            // the lit walk sign is in the dark half of a blink
inline const ScriptKit::Value<bool> MessageShowing;     // a message is on screen, or waiting to be
inline const ScriptKit::Value<bool> Struck;             // you are down, hit by a car
inline const ScriptKit::Value<bool> Wanted;             // the officer is after you
inline const ScriptKit::Value<bool> Arrested;           // the officer has you
inline const ScriptKit::Value<bool> Caught;             // the officer is touching you
inline const ScriptKit::Value<bool> OfficerDown;        // the officer is down, hit by a car, until you carry on
inline const ScriptKit::Value<bool> OfficerAboard;      // the officer is in the ambulance or the patrol car
inline const ScriptKit::Value<bool> PatrolOut;          // the patrol car is out
inline const ScriptKit::Value<bool> PatrolAtPlayer;     // the patrol car has reached you
inline const ScriptKit::Value<bool> PatrolGone;         // the patrol car has driven off the screen
inline const ScriptKit::Value<bool> OfficerAtPatrol;    // the officer is at the patrol car's door
inline const ScriptKit::Value<bool> AmbulanceOut;       // the ambulance is on the road
inline const ScriptKit::Value<bool> AmbulanceAtOfficer; // the ambulance has reached the officer
inline const ScriptKit::Value<bool> AmbulanceGone;      // the ambulance has driven off the screen
inline const ScriptKit::Value<bool> SirenRed;           // its light bar shows the red lamp; else the blue one
inline const ScriptKit::Value<bool> MedicsOut;          // its two medics are out on the street
inline const ScriptKit::Value<bool> MedicsAtOfficer;    // the medics have reached the officer
inline const ScriptKit::Value<bool> MedicsAtAmbulance;  // the medics are back at the ambulance
inline const ScriptKit::Value<bool> OfficerCarried;     // the medics have lifted the officer
inline const ScriptKit::Value<bool> InStreet;           // you are in the street
inline const ScriptKit::Value<int> Presses;             // presses of the button answered so far
inline const ScriptKit::Value<int> PlayerX;
inline const ScriptKit::Value<int> PlayerY;
inline const ScriptKit::Value<int> PatrolDoorX;         // where the officer gets into the patrol car
inline const ScriptKit::Value<int> PatrolDoorY;
inline const std::array<ScriptKit::Value<bool>, kCars> AtLine{};   // each car: it is at its lane's stop line

// Triggers, named by the program's own enum. The program's tick decides when each has happened.
enum class On : std::uint8_t { Begin, Interact, StepOffCurb, ReachFarSide, HitByCar, OfficerHit, Confirm };
inline constexpr ScriptKit::Trigger Begin{On::Begin};                 // the program starts
inline constexpr ScriptKit::Trigger Interact{On::Interact};           // the button is pressed
inline constexpr ScriptKit::Trigger StepOffCurb{On::StepOffCurb};     // you walk from a sidewalk into the street
inline constexpr ScriptKit::Trigger ReachFarSide{On::ReachFarSide};   // you reach the sidewalk across from the one you left
inline constexpr ScriptKit::Trigger HitByCar{On::HitByCar};           // a car drives into you
inline constexpr ScriptKit::Trigger OfficerHit{On::OfficerHit};       // a car drives into the officer
inline constexpr ScriptKit::Trigger Confirm{On::Confirm};             // the button is pressed, wherever you stand

// Commands: each is something a script can ask for. A command does nothing itself; the program's
// handlers do it when the request comes out of a script processor. The functions give each command's
// arguments their types and defaults.
inline const ScriptKit::Command Say{"say"};
inline const ScriptKit::Command Face{"face"};
inline const ScriptKit::Command Advance{"advance"};
inline const ScriptKit::Command StepToward{"step toward"};
inline const ScriptKit::Command SendBack{"send back"};
inline const ScriptKit::Command DriveAmbulance{"drive ambulance"};
inline const ScriptKit::Command WalkMedics{"walk medics"};
inline const ScriptKit::Command DrivePatrol{"drive patrol"};
inline ScriptKit::Entry say(std::string_view text, Bubble bubble = Bubble::Plain) { return Say(text, bubble); }
inline ScriptKit::Entry face(retropp::Dir direction) { return Face(direction); }
inline ScriptKit::Entry advance(int car) { return Advance(car); }
inline ScriptKit::Entry stepToward(ScriptKit::Expr<int> x, ScriptKit::Expr<int> y) { return StepToward(x, y); }
inline ScriptKit::Entry sendBack() { return SendBack(); }
inline ScriptKit::Entry driveAmbulance() { return DriveAmbulance(); }
inline ScriptKit::Entry walkMedics(Toward where) { return WalkMedics(where); }
inline ScriptKit::Entry drivePatrol(int pixels = 2) { return DrivePatrol(pixels); }

}  // namespace crossing
