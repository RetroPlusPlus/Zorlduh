// A complete program built on the ScriptKit module: a street crossing run by scripts. A signal cycles
// green, yellow and red; three cars in each lane drive through and queue at their line on red; an
// officer keeps watch from the far sidewalk. The arrow keys, WASD or a pad's directions walk you
// around, and Space, Return, Z or a pad's A button presses the button on the pole when you stand next
// to it.
//
// There is one way across: press the button, wait for the walk sign, and cross while it is lit. Step
// into the street while the cars are moving and one hits you, and you are put back where you started.
// Step off the curb on a red without the walk sign, or arrive on the other side without it, and the
// officer arrests you; then his backup arrives, a patrol car from whichever side is nearer, drives
// into you wherever you are standing, picks him up and leaves. The officer is no safer in the road
// than you are: a car that reaches him while he is after you knocks him down, and once the cars are
// done with you too an ambulance comes and two medics carry him aboard. Every car holds while the
// patrol car or the ambulance is out, and either one leaves you where you are until you press the
// button again to carry on.
//
// Twenty-two scripts on three script processors do all of that. Each script is a file of its own in
// scripts/, written in the values, triggers and commands of crossing.h; this file is the program
// around them: its state, how each value is read and written, what each command does, the tick, and
// the drawing. Under the street, three lines say what each processor is doing: the script it steps
// first, and a mark for each of that script's instructions with the one it is on lit.
//
// The art is assets/tiles.png, assets/sprites.png and assets/font.png, colored by the palette images in
// assets/palettes/. docs/getting-started.md walks through it part by part.

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "retropp/clock.h"
#include "retropp/draw_state.h"
#include "retropp/engine_config.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/input.h"
#include "retropp/input_actions.h"
#include "retropp/palette.h"
#include "retropp/renderer.h"
#include "retropp/run_loop.h"
#include "retropp/sdl_platform.h"
#include "retropp/timing.h"
#include "retropp/viewport.h"
#include "retropp/windowed_host.h"

#include "ScriptKit/bindings.h"
#include "ScriptKit/command.h"
#include "ScriptKit/script.h"
#include "ScriptKit/script_processor.h"

#include "crossing.h"
#include "scripts/ambulance.h"
#include "scripts/apprehend.h"
#include "scripts/arrest.h"
#include "scripts/arrest_on_arrival.h"
#include "scripts/backup.h"
#include "scripts/blink.h"
#include "scripts/carry_on.h"
#include "scripts/crossed.h"
#include "scripts/drive.h"
#include "scripts/officer_down.h"
#include "scripts/pressed.h"
#include "scripts/reminder.h"
#include "scripts/siren.h"
#include "scripts/struck.h"
#include "scripts/traffic.h"
#include "scripts/walk_phase.h"
#include "scripts/watch.h"

namespace {

using namespace crossing;

// ── The screen and the street, in pixels ────────────────────────────────────────────────────────

constexpr int kCell = 8;
constexpr int kColumns = 20;
constexpr int kRows = 19;
constexpr int kScreenWidth = kColumns * kCell;
constexpr int kScreenHeight = kRows * kCell;

constexpr int kStreetTop = 4 * kCell;         // the street is rows 4 to 9
constexpr int kStreetBottom = 10 * kCell;
constexpr int kWalkTop = 1 * kCell;           // you walk on rows 1 to 12
constexpr int kWalkBottom = 13 * kCell;
constexpr int kMessageY = 13 * kCell + 4;     // two lines of message in the panel on rows 13 to 15
constexpr int kReadoutY = 16 * kCell;         // a line for each processor on rows 16 to 18

constexpr retropp::Vec2i kPlayerStart{.x = 80, .y = 88};
constexpr retropp::Vec2i kOfficerPost{.x = 104, .y = 16};
constexpr retropp::IntRect kPole{.x = 64, .y = 80, .width = kCell, .height = kCell};
constexpr int kOfficerSpeed = 2;              // pixels a step; you walk one a tick

// The lanes. A car is 16×8. Each lane is a loop 216 pixels round: across the screen, then a stretch
// out of sight back to where it came in. A car's place is how far round its lane's loop it is, so
// both lanes share every number below; the eastbound lane runs left to right and the westbound one
// right to left.
enum class Lane : std::uint8_t { East, West };
constexpr int kCarLength = 16;
constexpr int kCarStep = 2;                    // pixels each time a car's script advances it
constexpr int kCarGap = 8;                     // the room a car leaves behind the one ahead
constexpr int kLoop = kScreenWidth + kCarLength + 40;
constexpr int kLine = 68;                      // where a car holds: its nose four pixels short of the crosswalk
constexpr std::array<int, 2> kLaneY{64, 40};   // by Lane
constexpr int kAmbulanceY = 52;                // the ambulance drives the line between the lanes

constexpr int kMessageTicks = 120;             // a message stays up two seconds

// The engine configuration: a 160×152 screen at 60 ticks a second. The script processors are built
// with it, so a `wait(5s)` in a script is 300 ticks here.
retropp::EngineConfig engineConfig() {
    return retropp::EngineConfig{
        .identity = {.organization = "Retro++", .application = "ScriptKit Getting Started"},
        .window = {.title = "ScriptKit — getting started"},
        .viewport = retropp::ViewportResolution{.width = kScreenWidth, .height = kScreenHeight},
        .timing = retropp::TimingProfile{.tickPeriodNs = retropp::TickPeriodNs::Hz60},
        .enhancements = {.windowScale = 4},
    };
}

enum class Action : std::uint8_t { Up, Down, Left, Right, Press };

// ── The program's state ─────────────────────────────────────────────────────────────────────────

struct Message {
    std::string_view text;
    Bubble bubble = Bubble::Plain;
    int ticksLeft = kMessageTicks;
};

struct State {
    retropp::Vec2i player = kPlayerStart;
    retropp::Vec2i officerAt = kOfficerPost;
    retropp::Dir officerFacing = retropp::Dir::Left;
    std::array<int, kCars> carAt{0, 72, 144, 36, 108, 180};   // how far round its lane's loop each car is
    std::array<int, kCars> carLap{};                          // how many times each has been round
    Lamp lamp = Lamp::Green;
    bool walkLit = false;
    bool walkDim = false;
    bool down = false;               // hit by a car, until the script lets you up
    bool wanted = false;
    bool arrested = false;
    bool officerFloored = false;     // the officer is hit by a car, until you carry on
    bool officerAboard = false;      // he is in the ambulance or the patrol car, and not drawn
    bool patrolOut = false;
    retropp::Vec2i patrolAt{};       // the patrol car's top-left corner
    int patrolHeading = 1;           // 1 while it drives east, -1 west
    bool ambulanceOut = false;
    int ambulanceX = -kCarLength;
    bool sirenRed = false;           // which lamp of its light bar is lit
    bool medicsOut = false;
    retropp::Vec2i medicsAt{};       // the point the two medics walk either side of
    bool officerCarried = false;     // they have lifted him, and he goes where they go
    int presses = 0;
    std::vector<Message> messages;   // the first is the one showing
    bool hit = false;                // a car drove into you since the last tick's triggers
    bool officerHit = false;         // a car drove into the officer since then
    bool inStreet = false;
    bool leftNearSide = true;        // which sidewalk you last stepped off
    bool begun = false;

    // One script processor per thing that acts, each built once over that thing's scripts.
    ScriptKit::ScriptProcessor signal{
        {&traffic, &reminder, &blink, &walkPhase, &pressed, &crossed, &struck}, engineConfig()};
    ScriptKit::ScriptProcessor cars{
        {&driving[0], &driving[1], &driving[2], &driving[3], &driving[4], &driving[5]}, engineConfig()};
    ScriptKit::ScriptProcessor officer{
        {&watch, &apprehend, &backup, &arrest, &arrestOnArrival, &ambulance, &siren, &crossing::officerDown,
         &carryOn},
        engineConfig()};
};

constexpr bool overlap(const retropp::IntRect& a, const retropp::IntRect& b) noexcept {
    return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}

constexpr retropp::IntRect box(retropp::Vec2i at) noexcept {
    return {.x = at.x, .y = at.y, .width = kCell, .height = kCell};
}

constexpr retropp::IntRect grown(const retropp::IntRect& rect, int by) noexcept {
    return {.x = rect.x - by, .y = rect.y - by, .width = rect.width + 2 * by, .height = rect.height + 2 * by};
}

constexpr Lane laneOf(int car) noexcept { return car < kCarsPerLane ? Lane::East : Lane::West; }

retropp::IntRect carBox(const State& state, int car) noexcept {
    const int at = state.carAt[static_cast<std::size_t>(car)];
    const Lane lane = laneOf(car);
    return {.x = lane == Lane::East ? at - kCarLength : kScreenWidth - at,
            .y = kLaneY[static_cast<std::size_t>(lane)],
            .width = kCarLength,
            .height = kCell};
}

// Whether a car is within a step of its lane's stop line.
bool atLine(const State& state, int car) noexcept {
    const int ahead = kLine - state.carAt[static_cast<std::size_t>(car)];
    return ahead >= 0 && ahead <= kCarStep;
}

// Whether a box touches any car.
bool onACar(const State& state, const retropp::IntRect& mine) noexcept {
    for (int car = 0; car < kCars; ++car) {
        if (overlap(mine, carBox(state, car))) {
            return true;
        }
    }
    return false;
}

retropp::IntRect patrolBox(const State& state) noexcept {
    return {.x = state.patrolAt.x, .y = state.patrolAt.y, .width = kCarLength, .height = kCell};
}

// Where the officer gets into the patrol car.
retropp::Vec2i patrolDoor(const State& state) noexcept {
    return {.x = state.patrolAt.x + kCell / 2, .y = state.patrolAt.y};
}

// Where the medics get out of the ambulance and back into it.
retropp::Vec2i ambulanceDoor(const State& state) noexcept {
    return {.x = state.ambulanceX + kCell / 2, .y = kAmbulanceY};
}

// Whether you are in the street: the middle of your box is on one of its rows.
bool inStreet(const State& state) noexcept {
    const int middle = state.player.y + kCell / 2;
    return middle >= kStreetTop && middle < kStreetBottom;
}

// ── The program's side of its scripts ───────────────────────────────────────────────────────────

// How each value is read from the state and, for the ones a script sets, written to it.
const ScriptKit::Bindings<State> bindings{
    {Signal, &State::lamp},
    {WalkLit, &State::walkLit},
    {WalkDim, &State::walkDim},
    {Struck, &State::down},
    {Wanted, &State::wanted},
    {Arrested, &State::arrested},
    {OfficerDown, &State::officerFloored},
    {OfficerAboard, &State::officerAboard},
    // Setting off, the ambulance comes in at the left edge of the screen.
    {AmbulanceOut, [](const State& state) { return state.ambulanceOut; },
     [](State& state, bool out) {
         state.ambulanceOut = out;
         if (out) {
             state.ambulanceX = -kCarLength;
         }
     }},
    {AmbulanceAtOfficer,
     [](const State& state) { return state.ambulanceX + kCarLength >= state.officerAt.x - kCarGap; }},
    {AmbulanceGone, [](const State& state) { return state.ambulanceX >= kScreenWidth; }},
    {SirenRed, &State::sirenRed},
    // Setting off, the patrol car comes in level with you, from whichever side of the screen is nearer.
    {PatrolOut, [](const State& state) { return state.patrolOut; },
     [](State& state, bool out) {
         state.patrolOut = out;
         if (out) {
             state.patrolHeading = state.player.x + kCell / 2 < kScreenWidth / 2 ? 1 : -1;
             state.patrolAt = {.x = state.patrolHeading > 0 ? -kCarLength : kScreenWidth, .y = state.player.y};
         }
     }},
    {PatrolAtPlayer, [](const State& state) { return overlap(patrolBox(state), box(state.player)); }},
    {PatrolGone,
     [](const State& state) { return state.patrolAt.x >= kScreenWidth || state.patrolAt.x <= -kCarLength; }},
    {OfficerAtPatrol, [](const State& state) { return state.officerAt == patrolDoor(state); }},
    {PatrolDoorX, [](const State& state) { return patrolDoor(state).x; }},
    {PatrolDoorY, [](const State& state) { return patrolDoor(state).y; }},
    // Getting out, the medics start at the ambulance's door.
    {MedicsOut, [](const State& state) { return state.medicsOut; },
     [](State& state, bool out) {
         state.medicsOut = out;
         if (out) {
             state.medicsAt = ambulanceDoor(state);
         }
     }},
    {MedicsAtOfficer, [](const State& state) { return state.medicsAt == state.officerAt; }},
    {MedicsAtAmbulance, [](const State& state) { return state.medicsAt == ambulanceDoor(state); }},
    {OfficerCarried, &State::officerCarried},
    {Presses, &State::presses},
    {MessageShowing, [](const State& state) { return !state.messages.empty(); }},
    {InStreet, [](const State& state) { return inStreet(state); }},
    {Caught, [](const State& state) { return overlap(grown(box(state.officerAt), 1), box(state.player)); }},
    {PlayerX, [](const State& state) { return state.player.x; }},
    {PlayerY, [](const State& state) { return state.player.y; }},
    {AtLine[0], [](const State& state) { return atLine(state, 0); }},
    {AtLine[1], [](const State& state) { return atLine(state, 1); }},
    {AtLine[2], [](const State& state) { return atLine(state, 2); }},
    {AtLine[3], [](const State& state) { return atLine(state, 3); }},
    {AtLine[4], [](const State& state) { return atLine(state, 4); }},
    {AtLine[5], [](const State& state) { return atLine(state, 5); }},
};

// What the program does for each command.
const ScriptKit::Handlers<State> handlers{
    // A message joins the line, and the tick shows each for two seconds. A shout cuts in: it shows at
    // once, in place of whatever was waiting.
    {Say, [](State& state, const ScriptKit::Arguments& arguments) {
         const Bubble bubble = arguments.word<Bubble>(1);
         if (bubble == Bubble::Shout) {
             state.messages.clear();
         }
         state.messages.push_back({.text = arguments.text(0), .bubble = bubble});
     }},
    {Face, [](State& state, const ScriptKit::Arguments& arguments) {
         state.officerFacing = arguments.word<retropp::Dir>(0);
     }},
    // A car moves a step round its lane's loop unless that would close on the car ahead, and hits
    // whoever it drives into: you, the officer, or both.
    {Advance, [](State& state, const ScriptKit::Arguments& arguments) {
         const int car = arguments.number(0);
         const int next = (state.carAt[static_cast<std::size_t>(car)] + kCarStep) % kLoop;
         for (int other = 0; other < kCars; ++other) {
             const int ahead = (state.carAt[static_cast<std::size_t>(other)] - next + kLoop) % kLoop;
             if (other != car && laneOf(other) == laneOf(car) && ahead < kCarLength + kCarGap) {
                 return;
             }
         }
         if (next < state.carAt[static_cast<std::size_t>(car)]) {
             ++state.carLap[static_cast<std::size_t>(car)];
         }
         state.carAt[static_cast<std::size_t>(car)] = next;
         if (overlap(carBox(state, car), box(state.player))) {
             state.hit = true;
         }
         if (!state.officerAboard && overlap(carBox(state, car), box(state.officerAt))) {
             state.officerHit = true;
         }
     }},
    // The officer moves a step toward a point, facing the way he goes.
    {StepToward, [](State& state, const ScriptKit::Arguments& arguments) {
         const auto toward = [](int from, int to) {
             return to > from ? std::min(from + kOfficerSpeed, to) : std::max(from - kOfficerSpeed, to);
         };
         const int x = toward(state.officerAt.x, arguments.number(0));
         if (x != state.officerAt.x) {
             state.officerFacing = x < state.officerAt.x ? retropp::Dir::Left : retropp::Dir::Right;
         }
         state.officerAt = {.x = x, .y = toward(state.officerAt.y, arguments.number(1))};
     }},
    // Everyone to their places, and whatever was being said is over.
    {SendBack, [](State& state, const ScriptKit::Arguments&) {
         state.player = kPlayerStart;
         state.officerAt = kOfficerPost;
         state.inStreet = false;
         state.messages.clear();
     }},
    {DriveAmbulance, [](State& state, const ScriptKit::Arguments&) { state.ambulanceX += kCarStep; }},
    // The patrol car moves as many pixels as it is told the way it is heading, and hits you if it
    // drives into you.
    {DrivePatrol, [](State& state, const ScriptKit::Arguments& arguments) {
         state.patrolAt.x += state.patrolHeading * arguments.number(0);
         if (overlap(patrolBox(state), box(state.player))) {
             state.hit = true;
         }
     }},
    // The medics take a step toward the officer or back to the ambulance; once they have lifted the
    // officer, he goes with them.
    {WalkMedics, [](State& state, const ScriptKit::Arguments& arguments) {
         const retropp::Vec2i to =
             arguments.word<Toward>(0) == Toward::Officer ? state.officerAt : ambulanceDoor(state);
         const auto toward = [](int from, int target) { return from < target ? from + 1 : from > target ? from - 1 : from; };
         state.medicsAt = {.x = toward(state.medicsAt.x, to.x), .y = toward(state.medicsAt.y, to.y)};
         if (state.officerCarried) {
             state.officerAt = state.medicsAt;
         }
     }},
};

// ── The tick ────────────────────────────────────────────────────────────────────────────────────

// Walks you a pixel on each held axis, stopping at the edges of the sidewalks, the pole and the cars.
void walk(State& state, const retropp::InputState& input) {
    if (state.down || state.arrested) {
        return;
    }
    const auto open = [&state](retropp::Vec2i at) {
        const retropp::IntRect mine = box(at);
        return at.x >= 0 && at.x <= kScreenWidth - kCell && at.y >= kWalkTop && at.y <= kWalkBottom - kCell &&
               !overlap(mine, kPole) && !onACar(state, mine);
    };
    const int dx = (input.isHeld(Action::Right) ? 1 : 0) - (input.isHeld(Action::Left) ? 1 : 0);
    const int dy = (input.isHeld(Action::Down) ? 1 : 0) - (input.isHeld(Action::Up) ? 1 : 0);
    if (dx != 0 && open({.x = state.player.x + dx, .y = state.player.y})) {
        state.player.x += dx;
    }
    if (dy != 0 && open({.x = state.player.x, .y = state.player.y + dy})) {
        state.player.y += dy;
    }
}

void tick(State& state, const retropp::InputState& input) {
    // First: everything the scripts asked for on the last tick is written. This is the only place a
    // script's effects land.
    ScriptKit::applyScripts({&state.signal, &state.cars, &state.officer}, bindings, handlers, state);

    // The program's own logic.
    if (!state.messages.empty() && --state.messages.front().ticksLeft <= 0) {
        state.messages.erase(state.messages.begin());
    }
    const bool wasNearSide = state.player.y + kCell / 2 >= kStreetBottom;
    walk(state, input);

    // The triggers, each a few lines here: the program says what happened, and every script waiting
    // on it whose criteria hold starts. `start` reads each script's `when`, so it is handed the values.
    const auto valuesOf = [&state](const ScriptKit::ScriptProcessor& processor) {
        return bindings.gather(state, processor.reads());
    };
    if (!state.begun) {
        state.begun = true;
        state.signal.start(Begin, valuesOf(state.signal));
        state.cars.start(Begin, valuesOf(state.cars));
        state.officer.start(Begin, valuesOf(state.officer));
    }
    if (input.justPressed(Action::Press)) {
        state.officer.start(Confirm, valuesOf(state.officer));
        if (overlap(grown(kPole, 2), box(state.player))) {
            state.signal.start(Interact, valuesOf(state.signal));
        }
    }
    const bool nowInStreet = inStreet(state);
    if (nowInStreet && !state.inStreet) {
        state.leftNearSide = wasNearSide;
        state.officer.start(StepOffCurb, valuesOf(state.officer));
    }
    if (!nowInStreet && state.inStreet) {
        const bool nowNearSide = state.player.y + kCell / 2 >= kStreetBottom;
        if (nowNearSide != state.leftNearSide) {
            state.signal.start(ReachFarSide, valuesOf(state.signal));
            state.officer.start(ReachFarSide, valuesOf(state.officer));
        }
    }
    state.inStreet = nowInStreet;
    if (state.hit) {
        state.hit = false;
        state.signal.start(HitByCar, valuesOf(state.signal));
    }
    if (state.officerHit) {
        state.officerHit = false;
        state.officer.start(OfficerHit, valuesOf(state.officer));
    }

    // Last: every processor is handed the values its scripts read, and steps once.
    ScriptKit::stepScripts({&state.signal, &state.cars, &state.officer}, bindings, state);
}

// ── Drawing ─────────────────────────────────────────────────────────────────────────────────────

// The characters of assets/font.png, one per cell, in the sheet's read order.
constexpr std::string_view kGlyphs = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?'-:";

// The cells of assets/tiles.png and assets/sprites.png, in sheet order.
enum class Tile : std::uint16_t { Sidewalk, Road, RoadDash, Crosswalk, Wall, PanelTop, Dark };
enum class Slot : std::size_t {
    Player, Down, Officer, CarRear, CarFront, Pole, Lamp, Walk, DontWalk, Mark,
    AmbulanceRear, AmbulanceFront, LightBar, Medic,
};

struct Art {
    retropp::AtlasManifest tiles;
    retropp::AtlasManifest sprites;
    retropp::AtlasManifest font;
    retropp::PaletteId street, strip, player, officer, carEast, carWest, patrol, ambulance, sirenRed, sirenBlue;
    retropp::PaletteId pole;
    retropp::PaletteId lampGreen, lampYellow, lampRed, walk, walkDim, dontWalk;
    retropp::PaletteId text, textShout, textDim;
};

// The bytes of a file under this example's assets/ directory.
std::vector<std::uint8_t> readAsset(const std::string& name) {
    std::ifstream in(std::string(SCRIPTKIT_EXAMPLE_ASSETS) + "/" + name, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open assets/" + name);
    }
    return {std::istreambuf_iterator<char>(in), {}};
}

// A palette image from this example's assets/ directory, uploaded.
retropp::PaletteId loadPalette(retropp::Renderer& renderer, const std::string& name) {
    const std::vector<retropp::Rgba16> colors =
        retropp::slicePaletteImage(retropp::loadPngFromMemory(readAsset("palettes/" + name + ".png")));
    return renderer.uploadPalette(std::span<const retropp::Rgba16>(colors));
}

Art loadArt(retropp::Renderer& renderer) {
    const auto sheet = [&renderer](const std::string& name, int count, retropp::TransparentIndices transparent) {
        return renderer.loadAtlasFromMemory(readAsset(name), retropp::AssetDimensions{.width = kCell, .height = kCell},
                                            retropp::ContentKind::Tileset,
                                            retropp::ReadOrder::LeftRightThenDown, count, transparent);
    };
    return Art{
        // The street's tiles draw all four of their indices; on the other two sheets index 0 is
        // transparent.
        .tiles = sheet("tiles.png", 7, retropp::TransparentIndices::None),
        .sprites = sheet("sprites.png", 14, retropp::TransparentIndices::of({0})),
        .font = sheet("font.png", static_cast<int>(kGlyphs.size()), retropp::TransparentIndices::of({0})),
        .street = loadPalette(renderer, "tiles"),
        .strip = loadPalette(renderer, "strip"),
        .player = loadPalette(renderer, "player"),
        .officer = loadPalette(renderer, "officer"),
        .carEast = loadPalette(renderer, "car-east"),
        .carWest = loadPalette(renderer, "car-west"),
        .patrol = loadPalette(renderer, "patrol"),
        .ambulance = loadPalette(renderer, "ambulance"),
        .sirenRed = loadPalette(renderer, "siren-red"),
        .sirenBlue = loadPalette(renderer, "siren-blue"),
        .pole = loadPalette(renderer, "pole"),
        .lampGreen = loadPalette(renderer, "lamp-green"),
        .lampYellow = loadPalette(renderer, "lamp-yellow"),
        .lampRed = loadPalette(renderer, "lamp-red"),
        .walk = loadPalette(renderer, "walk"),
        .walkDim = loadPalette(renderer, "walk-dim"),
        .dontWalk = loadPalette(renderer, "dont-walk"),
        .text = loadPalette(renderer, "text"),
        .textShout = loadPalette(renderer, "text-shout"),
        .textDim = loadPalette(renderer, "text-dim"),
    };
}

// The screen's cells, top to bottom: a wall, the far sidewalk, two lanes with a line between them and
// a crosswalk across, the near sidewalk, the message panel, and the readout's three lines.
std::vector<retropp::TileCell> Street(const Art& art) {
    std::vector<retropp::TileCell> cells;
    cells.reserve(static_cast<std::size_t>(kColumns) * kRows);
    for (int row = 0; row < kRows; ++row) {
        for (int column = 0; column < kColumns; ++column) {
            Tile tile = Tile::Sidewalk;
            if (row == 0) {
                tile = Tile::Wall;
            } else if (row >= 4 && row <= 9) {
                tile = column == 9 || column == 10 ? Tile::Crosswalk : row == 7 ? Tile::RoadDash : Tile::Road;
            } else if (row == 13) {
                tile = Tile::PanelTop;
            } else if (row >= 14) {
                tile = Tile::Dark;
            }
            cells.push_back(retropp::TileCell{
                .atlas = art.tiles.atlasId,
                .tile = art.tiles.slots[static_cast<std::size_t>(tile)].tile,
                .palette = row >= 16 ? art.strip : art.street,
            });
        }
    }
    return cells;
}

// One cell of the sprite sheet at a place.
retropp::Sprite Thing(std::string key, retropp::Vec2i at, const Art& art, Slot slot, retropp::PaletteId palette) {
    const retropp::AssetSlot& cell = art.sprites.slots[static_cast<std::size_t>(slot)];
    return retropp::Sprite{.key = std::move(key),
                           .x = at.x,
                           .y = at.y,
                           .z = at.y,
                           .size = cell.dimensions,
                           .atlas = art.sprites.atlasId,
                           .tile = cell.tile,
                           .palette = palette};
}

// Everything standing on the street: the pole, the signal's two lamps, the walk sign, the cars, the
// patrol car and the ambulance with their light bars while they are out, the medics, the officer
// unless he is aboard one of the two, and you. Each is drawn from the state; what a script changed shows because the state changed.
std::vector<retropp::Sprite> Things(const State& state, const Art& art) {
    const retropp::PaletteId lamp =
        state.lamp == Lamp::Green ? art.lampGreen : state.lamp == Lamp::Yellow ? art.lampYellow : art.lampRed;
    std::vector<retropp::Sprite> sprites{
        Thing("pole", {.x = kPole.x, .y = kPole.y}, art, Slot::Pole, art.pole),
        Thing("lamp:far", {.x = 64, .y = 24}, art, Slot::Lamp, lamp),
        Thing("lamp:near", {.x = 88, .y = 80}, art, Slot::Lamp, lamp),
        state.walkLit
            ? Thing("walk-sign", {.x = 88, .y = 24}, art, Slot::Walk, state.walkDim ? art.walkDim : art.walk)
            : Thing("walk-sign", {.x = 88, .y = 24}, art, Slot::DontWalk, art.dontWalk),
    };
    for (int car = 0; car < kCars; ++car) {
        // A car is two cells of the sheet side by side, drawn as one 16×8 sprite facing east, and
        // mirrored for the westbound lane. One coming back in on the far side is a new sprite: the
        // lap in its key keeps it from sliding across the screen.
        const retropp::IntRect at = carBox(state, car);
        const bool east = laneOf(car) == Lane::East;
        const std::string key =
            "car:" + std::to_string(car) + ":" + std::to_string(state.carLap[static_cast<std::size_t>(car)]);
        retropp::Sprite sprite =
            Thing(key, {.x = at.x, .y = at.y}, art, Slot::CarRear, east ? art.carEast : art.carWest);
        sprite.size = {.width = kCarLength, .height = kCell};
        sprite.flipX = !east;
        sprites.push_back(std::move(sprite));
    }
    if (state.ambulanceOut) {
        // The ambulance is two cells side by side, as a car is. Its light bar is a cell of its own
        // over the cab, holding both lamps: the red palette lights one and the blue palette the other,
        // so swapping the palette is the flash.
        retropp::Sprite van =
            Thing("ambulance", {.x = state.ambulanceX, .y = kAmbulanceY}, art, Slot::AmbulanceRear, art.ambulance);
        van.size = {.width = kCarLength, .height = kCell};
        sprites.push_back(std::move(van));
        retropp::Sprite lights = Thing("ambulance:lights", {.x = state.ambulanceX + kCell, .y = kAmbulanceY}, art,
                                       Slot::LightBar, state.sirenRed ? art.sirenRed : art.sirenBlue);
        lights.z = kAmbulanceY + 1;
        sprites.push_back(std::move(lights));
    }
    if (state.patrolOut) {
        // The patrol car is a car in its own colors under the same light bar, turned to face the way
        // it drives.
        const bool east = state.patrolHeading > 0;
        retropp::Sprite car = Thing("patrol", state.patrolAt, art, Slot::CarRear, art.patrol);
        car.size = {.width = kCarLength, .height = kCell};
        car.flipX = !east;
        sprites.push_back(std::move(car));
        retropp::Sprite lights = Thing("patrol:lights", {.x = state.patrolAt.x + (east ? kCell : 0), .y = state.patrolAt.y},
                                       art, Slot::LightBar, state.sirenRed ? art.sirenRed : art.sirenBlue);
        lights.flipX = !east;
        lights.z = state.patrolAt.y + 1;
        sprites.push_back(std::move(lights));
    }
    if (state.medicsOut) {
        // Two medics, one either side of the point they walk.
        for (const int side : {-1, 1}) {
            sprites.push_back(Thing(side < 0 ? "medic:left" : "medic:right",
                                    {.x = state.medicsAt.x + side * 6, .y = state.medicsAt.y}, art, Slot::Medic,
                                    art.ambulance));
        }
    }
    if (!state.officerAboard) {
        // Whoever is down is the same figure lying flat, in their own colors. Carried, he is drawn two
        // pixels up, between the medics and over them.
        retropp::Sprite officer =
            Thing("officer", state.officerAt, art, state.officerFloored ? Slot::Down : Slot::Officer, art.officer);
        officer.flipX = state.officerFacing == retropp::Dir::Left;
        if (state.officerCarried) {
            officer.y -= 2;
            officer.z = state.officerAt.y + 1;
        }
        sprites.push_back(std::move(officer));
    }
    sprites.push_back(Thing("player", state.player, art, state.down ? Slot::Down : Slot::Player, art.player));
    return sprites;
}

// Appends `text` as one sprite per character, eight pixels apart, from `at`; '\n' starts the next
// line. The sheet has capitals only, so small letters are drawn with them.
void Write(std::vector<retropp::Sprite>& sprites, const std::string& key, std::string_view text,
           retropp::Vec2i at, const Art& art, retropp::PaletteId palette) {
    int x = at.x;
    int y = at.y;
    for (std::size_t n = 0; n < text.size(); ++n) {
        if (text[n] == '\n') {
            x = at.x;
            y += kCell;
            continue;
        }
        const std::size_t glyph = kGlyphs.find(static_cast<char>(std::toupper(static_cast<unsigned char>(text[n]))));
        if (glyph != std::string_view::npos) {
            const retropp::AssetSlot& cell = art.font.slots[glyph];
            sprites.push_back(retropp::Sprite{.key = key + ":" + std::to_string(n),
                                              .x = x,
                                              .y = y,
                                              .size = cell.dimensions,
                                              .atlas = art.font.atlasId,
                                              .tile = cell.tile,
                                              .palette = palette});
        }
        x += kCell;
    }
}

// The message showing, in the panel under the street.
void MessageText(std::vector<retropp::Sprite>& sprites, const State& state, const Art& art) {
    if (state.messages.empty()) {
        return;
    }
    const Message& message = state.messages.front();
    Write(sprites, "message", message.text, {.x = kCell, .y = kMessageY}, art,
          message.bubble == Bubble::Shout ? art.textShout : art.text);
}

// One line of the readout, for one processor: the name of the script it steps first — the first of
// `scripts` that is running and not held — and a mark for each of that script's instructions, four
// pixels apart, with the one it is on lit: white while the script runs, yellow while it waits. A
// processor with nothing to step reads IDLE.
//
// `scripts` are the processor's scripts that have a trigger, in the order it steps them: highest
// priority first, and the order of its list between equals.
void Readout(std::vector<retropp::Sprite>& sprites, const std::string& key,
             const ScriptKit::ScriptProcessor& processor, std::span<const ScriptKit::Script* const> scripts, int y,
             const Art& art) {
    constexpr int kMarkStep = 4;
    const ScriptKit::Script* stepped = nullptr;
    for (const ScriptKit::Script* script : scripts) {
        if (processor.running(*script) && !processor.suspended(*script)) {
            stepped = script;
            break;
        }
    }
    if (stepped == nullptr) {
        Write(sprites, key + ":name", "IDLE", {.x = 0, .y = y}, art, art.textDim);
        return;
    }
    Write(sprites, key + ":name", stepped->name, {.x = 0, .y = y}, art, art.text);
    const int marksAt = static_cast<int>(stepped->name.size() + 1) * kCell;
    const std::size_t position = processor.position(*stepped);
    const retropp::PaletteId lit = processor.waiting(*stepped) ? art.textShout : art.text;
    for (std::size_t mark = 0; mark < processor.length(*stepped); ++mark) {
        retropp::Sprite sprite =
            Thing(key + ":mark:" + std::to_string(mark), {.x = marksAt + static_cast<int>(mark) * kMarkStep, .y = y},
                  art, Slot::Mark, mark == position ? lit : art.textDim);
        sprite.z = 0;
        sprites.push_back(std::move(sprite));
    }
}

}  // namespace

int main() {
    retropp::EngineConfig::setActive(engineConfig());

    retropp::SteadyClock clock;
    retropp::SdlPlatform platform;
    retropp::Renderer renderer{platform.device(), platform.sdlWindow()};
    retropp::RunLoop runLoop{clock};
    platform.actions(retropp::ActionMap{
        {Action::Up, {SDL_SCANCODE_UP, SDL_SCANCODE_W, retropp::PadButton::DpadUp}},
        {Action::Down, {SDL_SCANCODE_DOWN, SDL_SCANCODE_S, retropp::PadButton::DpadDown}},
        {Action::Left, {SDL_SCANCODE_LEFT, SDL_SCANCODE_A, retropp::PadButton::DpadLeft}},
        {Action::Right, {SDL_SCANCODE_RIGHT, SDL_SCANCODE_D, retropp::PadButton::DpadRight}},
        {Action::Press, {SDL_SCANCODE_SPACE, SDL_SCANCODE_RETURN, SDL_SCANCODE_Z, retropp::PadButton::FaceLabelA}},
    });

    const Art art = loadArt(renderer);
    const std::vector<retropp::TileCell> street = Street(art);

    State state;
    runLoop.simTick([&](const retropp::InputState& input) { tick(state, input); });

    // The readout's three lines: each processor's scripts that have a trigger, in the order it steps
    // them. `blink`, `apprehend`, `backup`, `ambulance` and `siren` have none — the first four run only
    // inside a call, and the siren is started and stopped by the two that use it — so they are left
    // out.
    const std::array<const ScriptKit::Script*, 6> signalScripts{&struck,   &walkPhase, &traffic,
                                                                &reminder, &pressed,   &crossed};
    const std::array<const ScriptKit::Script*, 6> carScripts{&driving[0], &driving[1], &driving[2],
                                                             &driving[3], &driving[4], &driving[5]};
    const std::array<const ScriptKit::Script*, 5> officerScripts{&carryOn, &crossing::officerDown, &watch,
                                                                 &arrest, &arrestOnArrival};

    retropp::FrameDrawState frame;
    std::vector<retropp::Sprite> things;
    std::vector<retropp::Sprite> words;
    runLoop.renderLoop([&] {
        things = Things(state, art);
        words.clear();
        MessageText(words, state, art);
        Readout(words, "readout:signal", state.signal, signalScripts, kReadoutY, art);
        Readout(words, "readout:cars", state.cars, carScripts, kReadoutY + kCell, art);
        Readout(words, "readout:officer", state.officer, officerScripts, kReadoutY + 2 * kCell, art);
        const retropp::PixelSize screen = retropp::Renderer::defaultViewport.size();
        frame.layers = {
            retropp::DrawLayer{.key = "street",
                               .z = 0,
                               .size = screen,
                               .content = retropp::TileContent{.widthInTiles = kColumns,
                                                               .heightInTiles = kRows,
                                                               .cells = street,
                                                               .wrap = retropp::TileWrap::Blank}},
            retropp::DrawLayer{.key = "things", .z = 10, .size = screen,
                               .content = retropp::SpriteContent{.sprites = things}},
            retropp::DrawLayer{.key = "words", .z = 20, .size = screen,
                               .content = retropp::SpriteContent{.sprites = words}},
        };
        renderer.renderFrame(frame);
    });

    retropp::WindowedHost{runLoop, platform}.run();
    return 0;
}
