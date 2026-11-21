// The ActorKit module: spawning and removing actors, ids, an Actors value copied whole; own values, the
// game's values and ActorKit's six; scripts by address and by value, and a script function; the trigger
// map and every detection; starting scripts; solidity by a value; walking by each drive, against the map,
// other actors and an area's edge; the player carried through a scroll, a fade and a seam by the
// game's own camera calls; the order of a tick; spawn and remove from a script; handlers; act; looks; and
// the sprites the actors report, drawn through WorldKit beside a sprite of the game's own. The world is
// built in code and the actors are moved by a mover of the test's own and by TopDownMovement's walkers.
// Every test but the last runs headless; the last resolves a mover's frame through a
// renderer on a real GPU device.

#include <gtest/gtest.h>

#include <SDL3/SDL.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "retropp/animation.h"
#include "retropp/atlas_manifest.h"
#include "retropp/draw_state.h"
#include "retropp/engine_config.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/input_actions.h"
#include "retropp/palette.h"
#include "retropp/renderer.h"
#include "retropp/tilemap.h"
#include "retropp/timing.h"

#include "ScriptKit/bindings.h"
#include "ScriptKit/command.h"
#include "ScriptKit/entry.h"
#include "ScriptKit/expression.h"
#include "ScriptKit/script.h"
#include "ScriptKit/trigger.h"
#include "ScriptKit/value.h"

#include "CameraKit/camera.h"

#include "LayerKit/types.h"

#include "WorldKit/area.h"
#include "WorldKit/camera.h"
#include "WorldKit/world_layers.h"

#include "TopDownMovement/directions.h"
#include "TopDownMovement/walker.h"

#include "ActorKit/actor.h"
#include "ActorKit/actors.h"
#include "ActorKit/commands.h"
#include "ActorKit/config.h"
#include "ActorKit/look.h"
#include "ActorKit/mover.h"
#include "ActorKit/sprites.h"
#include "ActorKit/tick.h"
#include "ActorKit/triggers.h"
#include "ActorKit/values.h"

namespace {

using ActorKit::Actor;
using ActorKit::Id;
using retropp::Dir;
using retropp::IntRect;
using retropp::Vec2i;
using ScriptKit::loop;
using ScriptKit::wait;
using WorldKit::Place;
using WorldKit::Side;
using WorldKit::Transition;
using namespace std::chrono_literals;

// An engine configuration ticking at 60 Hz.
retropp::EngineConfig configAt60() {
    retropp::EngineConfig config;
    config.timing = retropp::TimingProfile{.tickPeriodNs = retropp::TickPeriodNs::Hz60};
    return config;
}
const retropp::EngineConfig kConfig = configAt60();

// ── The world: two layouts, four areas ─────────────────────────────────────────────────────────────

enum class Cell : std::uint16_t { Wall = 0, Open = 1 };
constexpr int kCell = 8;
constexpr CameraKit::View kView{.size = {.width = 160, .height = 128}, .origin = {.x = 0, .y = 16}};

retropp::IndexGrid room(int width, int height, const std::vector<Vec2i>& doorways = {}) {
    retropp::IndexGrid map{.width = width, .height = height};
    map.values.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool edge = x == 0 || y == 0 || x == width - 1 || y == height - 1;
            map.values[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] =
                static_cast<std::uint16_t>(edge ? Cell::Wall : Cell::Open);
        }
    }
    for (const Vec2i door : doorways) {
        map.values[static_cast<std::size_t>(door.y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(door.x)] =
            static_cast<std::uint16_t>(Cell::Open);
    }
    return map;
}

std::vector<Vec2i> column(int x, int first, int last) {
    std::vector<Vec2i> cells;
    for (int y = first; y <= last; ++y) {
        cells.push_back({.x = x, .y = y});
    }
    return cells;
}

std::vector<Vec2i> row(int y, int first, int last) {
    std::vector<Vec2i> cells;
    for (int x = first; x <= last; ++x) {
        cells.push_back({.x = x, .y = y});
    }
    return cells;
}

std::vector<Vec2i> both(std::vector<Vec2i> a, const std::vector<Vec2i>& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

retropp::TileCatalog art() {
    return retropp::TileCatalog{.entries = {
        {.id = 0, .sheet = retropp::AtlasId{7}, .slot = 1, .palette = retropp::PaletteId{4}},
        {.id = 1, .sheet = retropp::AtlasId{7}, .slot = 0, .palette = retropp::PaletteId{4}},
    }};
}

// Layout 0: area 0 is one screen with a door east to area 1 and stairs fading to layout 1; area 1 is
// one screen with the door back west and its north wall joined seamlessly to area 2; area 2 is one screen
// joined along its south wall. Layout 1: area 0, smaller than the screen, with stairs back up. Area 0
// has an "underfoot" slot below its "actors" slot; the others have "actors" alone.
struct Maps {
    retropp::IndexGrid a = room(20, 16, column(19, 6, 9));
    retropp::IndexGrid b = room(20, 16, both(column(0, 6, 9), row(0, 1, 18)));
    retropp::IndexGrid c = room(20, 16, row(15, 1, 18));
    retropp::IndexGrid d = room(12, 10);
};

WorldKit::World worldOver(const Maps& maps) {
    const std::vector<WorldKit::ActorSlot> actorsOnly{{.name = "actors", .z = 5}};
    return WorldKit::World{.layouts = {
        WorldKit::Layout{
            .name = "above",
            .areas = {
                WorldKit::Area{.name = "a",
                               .layers = {{.name = "ground", .tiles = &maps.a, .art = art()}},
                               .actorSlots = {{.name = "actors", .z = 5}, {.name = "underfoot", .z = 2}},
                               .collision = &maps.a,
                               .openings = {{.name = "east", .side = Side::East, .span = {6, 9},
                                             .transition = Transition::Scroll, .leadsTo = {.area = 1, .opening = "west"}},
                                            {.name = "stairs", .at = {.x = 12, .y = 4, .width = 2, .height = 1},
                                             .transition = Transition::Fade,
                                             .leadsTo = {.layout = 1, .area = 0, .opening = "stairs"}}}},
                WorldKit::Area{.name = "b",
                               .layers = {{.name = "ground", .tiles = &maps.b, .art = art()}},
                               .actorSlots = actorsOnly,
                               .collision = &maps.b,
                               .openings = {{.name = "west", .side = Side::West, .span = {6, 9},
                                             .transition = Transition::Scroll, .leadsTo = {.area = 0, .opening = "east"}},
                                            {.name = "north", .side = Side::North, .span = {1, 18},
                                             .transition = Transition::Seamless, .leadsTo = {.area = 2, .opening = "south"}}}},
                WorldKit::Area{.name = "c",
                               .layers = {{.name = "ground", .tiles = &maps.c, .art = art()}},
                               .actorSlots = actorsOnly,
                               .collision = &maps.c,
                               .openings = {{.name = "south", .side = Side::South, .span = {1, 18},
                                             .transition = Transition::Seamless, .leadsTo = {.area = 1, .opening = "north"}}}},
            },
            .cellSize = kCell,
            .open = Cell::Open,
            .view = kView,
        },
        WorldKit::Layout{
            .name = "below",
            .areas = {
                WorldKit::Area{.name = "d",
                               .layers = {{.name = "ground", .tiles = &maps.d, .art = art()}},
                               .actorSlots = actorsOnly,
                               .collision = &maps.d,
                               .openings = {{.name = "stairs", .at = {.x = 5, .y = 4, .width = 2, .height = 1},
                                             .transition = Transition::Fade,
                                             .leadsTo = {.layout = 0, .area = 0, .opening = "stairs"}}}},
            },
            .cellSize = kCell,
            .open = Cell::Open,
            .view = kView,
        },
    }};
}

constexpr Place kA{.layout = 0, .area = 0};
constexpr Place kB{.layout = 0, .area = 1};
constexpr Place kC{.layout = 0, .area = 2};
constexpr Place kD{.layout = 1, .area = 0};

// ── A mover of the test's own ──────────────────────────────────────────────────────────────────────

// A heading of the test's own type: four flags, as ActorKit's Dir drive asks.
struct Way {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
};

struct SliderConfig {
    int speed = 1;
    IntRect collision{.x = 0, .y = 0, .width = 8, .height = 8};
    retropp::AnimationFrame frame{};
};

struct SliderShape {
    IntRect box;
    [[nodiscard]] IntRect bounds() const { return box; }
};

// Moves `speed` pixels a tick on each axis of its heading, a pixel at a time, stopping where its box
// would meet a solid; faces the last axis it was headed along.
class Slider {
public:
    explicit Slider(Vec2i& at) : at_(&at) {}
    void bind(Vec2i& at) { at_ = &at; }

    template <class Solids>
    void walk(const SliderConfig& config, Way way, const Solids& solids) {
        const int dx = (way.right ? 1 : 0) - (way.left ? 1 : 0);
        const int dy = (way.down ? 1 : 0) - (way.up ? 1 : 0);
        if (dx != 0) {
            facing_ = dx > 0 ? Dir::Right : Dir::Left;
        }
        if (dy != 0) {
            facing_ = dy > 0 ? Dir::Down : Dir::Up;
        }
        for (const Vec2i unit : {Vec2i{.x = dx, .y = 0}, Vec2i{.x = 0, .y = dy}}) {
            if (unit.x == 0 && unit.y == 0) {
                continue;
            }
            for (int n = 0; n < config.speed; ++n) {
                const IntRect next{.x = at_->x + unit.x + config.collision.x, .y = at_->y + unit.y + config.collision.y,
                                   .width = config.collision.width, .height = config.collision.height};
                if (solids.solidBox(next)) {
                    break;
                }
                at_->x += unit.x;
                at_->y += unit.y;
            }
        }
    }

    [[nodiscard]] Dir facing() const { return facing_; }
    [[nodiscard]] SliderShape collision(const SliderConfig& config) const {
        return {{.x = at_->x + config.collision.x, .y = at_->y + config.collision.y,
                 .width = config.collision.width, .height = config.collision.height}};
    }
    [[nodiscard]] const retropp::AnimationFrame& frame(const SliderConfig& config) const { return config.frame; }

private:
    Vec2i* at_;
    Dir facing_ = Dir::Down;
};

// ── The game's names ───────────────────────────────────────────────────────────────────────────────

enum class Action : std::uint8_t { Interact, Kick };
enum class On : std::uint8_t { Interact, Touch, Near, Enter, Becomes, Bump };

constexpr ScriptKit::Trigger Interact{On::Interact};
constexpr ScriptKit::Trigger Touch{On::Touch};
constexpr ScriptKit::Trigger Bump{On::Bump};
constexpr ScriptKit::Trigger Enter{On::Enter};
constexpr ScriptKit::Trigger Near(int distance) { return {On::Near, distance}; }
ScriptKit::Trigger Becomes(ScriptKit::Value<bool> value) { return {On::Becomes, value}; }

struct Input {
    Way way;
    bool interact = false;
    TopDownMovement::Directions heading;
};

// The game's values, bound to the state below.
const ScriptKit::Value<int> Count;
const ScriptKit::Value<bool> Flag;
const ScriptKit::Value<bool> Lit;
const ScriptKit::Value<int> SeenX;
const ScriptKit::Value<int> SeenY;
const ScriptKit::Value<int> SeenFX;
const ScriptKit::Value<int> SeenFY;
const ScriptKit::Value<bool> SeenHere;
const ScriptKit::Value<bool> SeenTouched;

// Values the tests' actors own.
const ScriptKit::Value<int> Mine;
const ScriptKit::Value<bool> Up;
const ScriptKit::Value<Dir> Heading;

// A value outside every binding and every actor's own values.
const ScriptKit::Value<bool> Stray;

const ScriptKit::Command Shout{"shout"};
const ScriptKit::Command Whisper{"whisper"};

struct State {
    ActorKit::Actors actors{kConfig};
    WorldKit::Camera camera{0, 0};
    int count = 0;
    bool flag = false;
    bool lit = false;
    int seenX = 0;
    int seenY = 0;
    int seenFX = 0;
    int seenFY = 0;
    bool seenHere = false;
    bool seenTouched = false;
    int shouts = 0;
    Id asker = ActorKit::kNoId;
};

const ScriptKit::Bindings<State> kBindings{
    {Count, &State::count},   {Flag, &State::flag},     {Lit, &State::lit},
    {SeenX, &State::seenX},   {SeenY, &State::seenY},   {SeenFX, &State::seenFX},
    {SeenFY, &State::seenFY}, {SeenHere, &State::seenHere}, {SeenTouched, &State::seenTouched},
};

const ActorKit::Handlers<State> kHandlers{
    {Shout, [](State& state, Id asker, const ScriptKit::Arguments&) {
         ++state.shouts;
         state.asker = asker;
     }},
};

ActorKit::TriggerMap triggers() {
    return {
        {On::Interact, ActorKit::actedOn(Action::Interact)},
        {On::Touch, ActorKit::touched()},
        {On::Near, ActorKit::near()},
        {On::Enter, ActorKit::entered()},
        {On::Becomes, ActorKit::becomes()},
        {On::Bump, ActorKit::touched()},
    };
}

// The message `run` throws, or empty when it returns.
std::string thrown(const std::function<void()>& run) {
    try {
        run();
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

bool mentions(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

// One world, one state, one config; each test spawns what it needs. The fixture is the game: its tick
// runs the actors, then makes the game's own camera calls for the player.
class Farm : public ::testing::Test {
protected:
    Maps maps;
    WorldKit::World world = worldOver(maps);
    State state;
    ActorKit::Config config{.world = &world, .triggers = triggers(), .bindings = &kBindings, .handlers = &kHandlers};

    void SetUp() override { WorldKit::validate(world); }

    // One tick of the game: ActorKit's tick, then, while the player is in the camera's area, the camera
    // following its box and the player carried where the camera says. Returns what the camera said.
    WorldKit::Step tick(const Input& input = {}) {
        ActorKit::tick(state.actors, config, state.camera, input, state);
        const Id player = state.actors.player();
        if (player == ActorKit::kNoId ||
            state.actors.place(player) != Place{.layout = state.camera.layout(), .area = state.camera.area()}) {
            return {};
        }
        const WorldKit::Step step = state.camera.follow(world, state.actors.box(player));
        const Vec2i at = state.actors.position(player);
        state.actors.moveTo(player, step.entered.value_or(state.actors.place(player)),
                            {.x = at.x + step.move.x, .y = at.y + step.move.y});
        return step;
    }

    // The game's warp: the actor put there at once, and the camera put in that area on its box.
    void warp(Id id, Place place, Vec2i at) {
        state.actors.moveTo(id, place, at);
        state.camera = WorldKit::Camera(place.layout, place.area);
        state.camera.place(world, state.actors.box(id));
    }

    void ticks(int count, const Input& input = {}) {
        for (int n = 0; n < count; ++n) {
            tick(input);
        }
    }

    // Ticks with `input` until `done` holds, at most `limit` times; whether it came to hold.
    bool tickUntil(const Input& input, const std::function<bool()>& done, int limit = 200) {
        for (int n = 0; n < limit; ++n) {
            if (done()) {
                return true;
            }
            tick(input);
        }
        return done();
    }
};

// A walker of the test's own, driven by the input's `way`.
Actor slider(SliderConfig config = {}) {
    return {.slot = "actors", .solid = true, .mover = ActorKit::movedBy<Slider>(config, &Input::way)};
}

// An actor with an empty mover and an 8×8 box.
Actor block(ActorKit::Scripts scripts = {}) { return {.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 8}, .scripts = scripts}; }

// ── Spawning, ids and the live actors ──────────────────────────────────────────────────────────────

TEST_F(Farm, SpawnHandsOutIdsFromOneInOrderAndNeverHandsOneOutTwice) {
    ActorKit::Actors& actors = state.actors;
    EXPECT_EQ(actors.spawn(block(), kA, {16, 16}), 1u);
    EXPECT_EQ(actors.spawn(block(), kA, {32, 16}), 2u);
    EXPECT_EQ(actors.spawn(block(), kB, {48, 16}), 3u);
    EXPECT_TRUE(actors.remove(2));
    EXPECT_FALSE(actors.remove(2));
    EXPECT_EQ(actors.spawn(block(), kA, {64, 16}), 4u);
    EXPECT_EQ(actors.size(), 3u);
    EXPECT_FALSE(actors.has(2));
    EXPECT_TRUE(actors.has(4));
    const std::vector<Id> ids(actors.ids().begin(), actors.ids().end());
    EXPECT_EQ(ids, (std::vector<Id>{1, 3, 4}));
}

TEST_F(Farm, AnActorsPlacePositionBoxAndFacingAreReadAndAnUnknownIdThrowsNamingIt) {
    ActorKit::Actors& actors = state.actors;
    const Id sign = actors.spawn(Actor{.slot = "actors", .box = {.x = 2, .y = 4, .width = 12, .height = 6}}, kB, {40, 24});
    EXPECT_EQ(actors.place(sign), kB);
    EXPECT_EQ(actors.position(sign), (Vec2i{40, 24}));
    EXPECT_EQ(actors.box(sign), (IntRect{.x = 42, .y = 28, .width = 12, .height = 6}));
    EXPECT_EQ(actors.facing(sign), Dir::Down);
    actors.moveTo(sign, kC, {8, 8});
    EXPECT_EQ(actors.place(sign), kC);
    EXPECT_EQ(actors.box(sign), (IntRect{.x = 10, .y = 12, .width = 12, .height = 6}));
    EXPECT_TRUE(mentions(thrown([&] { (void)actors.position(99); }), "99"));
    EXPECT_THROW((void)actors.place(99), std::out_of_range);
    EXPECT_THROW(actors.moveTo(99, kA, {}), std::out_of_range);
}

TEST_F(Farm, ACopiedActorsWalksItsOwnPositions) {
    const Id hero = state.actors.spawn(slider(), kA, {40, 40});
    tick();
    State copy = state;
    for (int n = 0; n < 3; ++n) {
        ActorKit::tick(copy.actors, config, copy.camera, Input{.way = {.right = true}}, copy);
    }
    EXPECT_EQ(copy.actors.position(hero), (Vec2i{43, 40}));
    EXPECT_EQ(state.actors.position(hero), (Vec2i{40, 40}));
    tick(Input{.way = {.down = true}});
    EXPECT_EQ(state.actors.position(hero), (Vec2i{40, 41}));
    EXPECT_EQ(copy.actors.position(hero), (Vec2i{43, 40}));
}

const ScriptKit::Script named{.name = "named", .on = Enter, .body = {set(Flag, true)}};
const ScriptKit::Script sameName{.name = "named", .on = Enter, .body = {set(Lit, true)}};

TEST_F(Farm, SpawnRefusesABoxBesideAMoverAnOwnValueTwiceANullScriptAndScriptKitsRefusals) {
    ActorKit::Actors& actors = state.actors;
    Actor boxed = slider();
    boxed.box = {.x = 0, .y = 0, .width = 8, .height = 8};
    EXPECT_THROW(actors.spawn(boxed, kA, {}), std::invalid_argument);
    EXPECT_THROW(actors.spawn(Actor{.ownValues = {{Mine, 1}, {Mine, 2}}}, kA, {}), std::invalid_argument);
    EXPECT_THROW(actors.spawn(Actor{.scripts = {static_cast<const ScriptKit::Script*>(nullptr)}}, kA, {}),
                 std::invalid_argument);
    EXPECT_THROW(actors.spawn(Actor{.scripts = {&named, &sameName}}, kA, {}), std::logic_error);
    EXPECT_EQ(actors.size(), 0u);
}

// ── Values ─────────────────────────────────────────────────────────────────────────────────────────

const ScriptKit::Script countUp{.name = "count up", .on = Enter, .body = {set(Mine, Mine + 1)}};

TEST_F(Farm, EachActorHoldsItsOwnInstanceOfItsOwnValues) {
    const Actor counter{.ownValues = {{Mine, 5}}, .scripts = {&countUp}};
    const Id a = state.actors.spawn(counter, kA, {16, 16});
    const Id b = state.actors.spawn(counter, kA, {32, 16});
    EXPECT_EQ(state.actors.get(a, Mine), 5);
    ticks(2);
    EXPECT_EQ(state.actors.get(a, Mine), 6);
    EXPECT_EQ(state.actors.get(b, Mine), 6);
    state.actors.set(a, Mine, 10);
    EXPECT_EQ(state.actors.get(a, Mine), 10);
    EXPECT_EQ(state.actors.get(b, Mine), 6);
    EXPECT_TRUE(mentions(thrown([&] { (void)state.actors.get(a, Count); }), std::to_string(a)));
    EXPECT_THROW(state.actors.set(a, Up, true), std::out_of_range);
}

const ScriptKit::Script addNow{.name = "add now", .on = Enter, .body = {set(Count, Count + 1)}};
const ScriptKit::Script addLater{.name = "add later", .on = Enter, .body = {wait(1), set(Count, Count + 1)}};

TEST_F(Farm, TheGamesValuesAreOneInstanceThatEveryActorsScriptsReadAndWrite) {
    state.actors.spawn(block({&addNow}), kA, {16, 16});
    state.actors.spawn(block({&addLater}), kA, {32, 16});
    ticks(3);
    EXPECT_EQ(state.count, 2);
}

const ScriptKit::Script seeX{.name = "see x", .on = Enter, .body = {loop({set(SeenX, ActorKit::X), wait(1)})}};
const ScriptKit::Script seeY{.name = "see y", .on = Enter, .body = {loop({set(SeenY, ActorKit::Y), wait(1)})}};
const ScriptKit::Script seeFX{.name = "see fx", .on = Enter, .body = {loop({set(SeenFX, ActorKit::PlayerX), wait(1)})}};
const ScriptKit::Script seeFY{.name = "see fy", .on = Enter, .body = {loop({set(SeenFY, ActorKit::PlayerY), wait(1)})}};
const ScriptKit::Script seeHere{.name = "see here", .on = Enter,
                                .body = {loop({set(SeenHere, ActorKit::PlayerHere), wait(1)})}};
const ScriptKit::Script seeTouched{.name = "see touched", .on = Enter,
                                   .body = {loop({set(SeenTouched, ActorKit::Touched), wait(1)})}};

TEST_F(Farm, ActorKitFillsInTouchedXYAndThePlayersPlaceAndPositionForEachActor) {
    const Id watcher = state.actors.spawn(block({&seeX, &seeY, &seeFX, &seeFY, &seeHere, &seeTouched}), kA, {40, 48});
    const Id hero = state.actors.spawn(Actor{.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 8},
                                             .setsOff = {On::Touch}},
                                       kB, {100, 60});
    tick();   // the camera's area: the watcher's scripts start
    state.actors.player(hero);
    ticks(3);
    EXPECT_EQ(state.seenX, 40);
    EXPECT_EQ(state.seenY, 48);
    EXPECT_FALSE(state.seenHere);
    EXPECT_EQ(state.seenFX, 40);   // the player is elsewhere: the watcher's own position
    EXPECT_EQ(state.seenFY, 48);
    EXPECT_FALSE(state.seenTouched);

    state.actors.moveTo(hero, kA, {44, 50});   // overlapping the watcher
    ticks(3);
    EXPECT_TRUE(state.seenHere);
    EXPECT_EQ(state.seenFX, 44);
    EXPECT_EQ(state.seenFY, 50);
    EXPECT_TRUE(state.seenTouched);
    (void)watcher;
}

// The six are constants before any constructor at namespace scope runs, so the scripts above read them
// whatever order the toolchain builds this file's constants and the header's in.
constinit const ScriptKit::Value<int> kEarlyX = ActorKit::X;

TEST(ActorKitValues, TheSixAreCompileTimeConstantsWithIdsOfTheirOwnAboveEveryMintedId) {
    static_assert(ActorKit::Touched.id() == 0x40000000);
    static_assert(ActorKit::X.id() == 0x40000001);
    static_assert(ActorKit::Y.id() == 0x40000002);
    static_assert(ActorKit::PlayerHere.id() == 0x40000003);
    static_assert(ActorKit::PlayerX.id() == 0x40000004);
    static_assert(ActorKit::PlayerY.id() == 0x40000005);
    EXPECT_EQ(kEarlyX.id(), ActorKit::X.id());

    const ScriptKit::Value<int> minted;
    EXPECT_LT(minted.id(), ActorKit::Touched.id());
}

TEST_F(Farm, AValueThatIsNeitherOwnActorKitsNorBoundThrowsOnTheActorsFirstTickNamingTheActorAndTheValue) {
    const Id stray = state.actors.spawn(Actor{.solid = Stray}, kA, {16, 16});
    const std::string message = thrown([&] { tick(); });
    EXPECT_TRUE(mentions(message, "actor " + std::to_string(stray))) << message;
    EXPECT_TRUE(mentions(message, std::to_string(Stray.id()))) << message;
}

// ── Scripts ────────────────────────────────────────────────────────────────────────────────────────

ScriptKit::Script raise(ScriptKit::Value<bool> value) {
    return {.name = "raise", .on = Interact, .body = {set(value, true)}};
}

TEST_F(Farm, AScriptByValueIsOneScriptForEveryActorSpawnedFromTheValueAndEachRunsItsOwnCopy) {
    const Actor lamp{.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 8}, .scripts = {raise(Lit), &named}};
    const ScriptKit::Script* byValue = lamp.scripts[0].get();
    const Id a = state.actors.spawn(lamp, kA, {40, 40});
    const Id b = state.actors.spawn(lamp, kA, {80, 40});
    EXPECT_EQ(state.actors.processor(a).length(*byValue), 1u);
    EXPECT_EQ(state.actors.processor(b).length(*byValue), 1u);
    EXPECT_EQ(state.actors.processor(a).length(named), 1u);

    // Entered on the first tick: each actor's `named` runs once, and only its own.
    tick();
    EXPECT_TRUE(state.actors.processor(a).idle());
    tick();
    EXPECT_TRUE(state.flag);
    EXPECT_FALSE(state.lit);
}

TEST_F(Farm, AScriptFunctionCalledWithTwoValuesActsOnEach) {
    const Id hero = state.actors.spawn(Actor{.slot = "actors", .mover = ActorKit::movedBy<Slider>(SliderConfig{}, &Input::way),
                                             .actions = {{Action::Interact, &Input::interact}}},
                                       kA, {40, 48});
    state.actors.spawn(block({raise(Flag)}), kA, {40, 40});   // above the hero
    state.actors.spawn(block({raise(Lit)}), kA, {48, 48});    // to its right
    tick(Input{.way = {.up = true}, .interact = true});        // faces up, the way clear: steps to 47
    tick();
    EXPECT_TRUE(state.flag);
    EXPECT_FALSE(state.lit);
    tick(Input{.way = {.right = true}, .interact = true});
    tick();
    EXPECT_TRUE(state.lit);
    (void)hero;
}

// ── The trigger map ────────────────────────────────────────────────────────────────────────────────

const ScriptKit::Script onTouch{.name = "on touch", .on = Touch, .body = {set(Flag, true)}};
const ScriptKit::Script onBump{.name = "on bump", .on = Bump, .body = {set(Lit, true)}};

TEST_F(Farm, ATriggerBoundTwiceThrowsAndTwoTriggersBoundToOneDetectionBothHappen) {
    EXPECT_THROW((ActorKit::TriggerMap{{On::Touch, ActorKit::touched()}, {On::Touch, ActorKit::near()}}),
                 std::invalid_argument);
    state.actors.spawn(block({&onTouch, &onBump}), kA, {40, 40});
    state.actors.spawn(Actor{.box = {.x = 0, .y = 0, .width = 8, .height = 8}, .setsOff = {On::Touch, On::Bump}}, kA,
                       {44, 44});
    ticks(2);
    EXPECT_TRUE(state.flag);
    EXPECT_TRUE(state.lit);
}

// ── ActorKit's own triggers ────────────────────────────────────────────────────────────────────────

const ScriptKit::Script presetTouch{.name = "preset touch", .on = ActorKit::Touch, .body = {set(Flag, true)}};
const ScriptKit::Script presetNear{.name = "preset near", .on = ActorKit::Near(24), .body = {set(Lit, true)}};
const ScriptKit::Script presetEnter{.name = "preset enter", .on = ActorKit::Enter, .body = {set(SeenX, 7)}};
const ScriptKit::Script presetBecomes{.name = "preset becomes", .on = ActorKit::Becomes(Flag), .body = {set(SeenY, 8)}};
const ScriptKit::Script presetInteract{.name = "preset interact", .on = ActorKit::Interact, .body = {set(Count, 5)}};

TEST_F(Farm, ActorKitsOwnTriggersWorkInAConfigThatNamesOnlyTheInteractAction) {
    const ActorKit::Config presets{.world = &world, .interactAction = Action::Interact, .bindings = &kBindings};
    const auto run = [&](const Input& input = {}) { ActorKit::tick(state.actors, presets, state.camera, input, state); };

    state.actors.spawn(block({&presetTouch, &presetNear, &presetEnter, &presetBecomes, &presetInteract}), kA, {40, 40});
    const Id hero = state.actors.spawn(Actor{.slot = "actors", .mover = ActorKit::movedBy<Slider>(SliderConfig{}, &Input::way),
                                             .actions = {{Action::Interact, &Input::interact}},
                                             .setsOff = {ActorKit::On::Touch, ActorKit::On::Near}},
                                       kA, {100, 40});
    run();
    run();
    EXPECT_EQ(state.seenX, 7);   // entered, on its first tick in the camera's area
    EXPECT_FALSE(state.lit);

    state.actors.moveTo(hero, kA, {60, 40});   // 12 pixels away: near
    run();
    run();
    EXPECT_TRUE(state.lit);
    EXPECT_FALSE(state.flag);

    state.actors.moveTo(hero, kA, {46, 42});   // overlapping: touched, which makes Flag true, which Becomes sees
    run();
    run();
    EXPECT_TRUE(state.flag);
    run();
    run();
    EXPECT_EQ(state.seenY, 8);

    state.actors.moveTo(hero, kA, {40, 48});   // below it, facing up
    run(Input{.way = {.up = true}, .interact = true});
    run();
    EXPECT_EQ(state.count, 5);
}

const ScriptKit::Script ownTouch{.name = "own touch", .on = Touch, .body = {set(Flag, true)}};

TEST_F(Farm, TheGamesTriggersAndActorKitsStandSideBySideAndInteractIsBoundOnlyOnceItsActionIsNamed) {
    // The fixture's map binds the game's own triggers; ActorKit's ride along in it. Each touch trigger
    // has its own setters.
    state.actors.spawn(block({&ownTouch, &presetTouch, &presetInteract}), kA, {40, 40});
    const Id presetSetter = state.actors.spawn(
        Actor{.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 8}, .setsOff = {ActorKit::On::Touch}}, kA,
        {44, 44});
    ticks(2);
    EXPECT_TRUE(state.flag);   // presetTouch
    state.flag = false;
    state.actors.remove(presetSetter);
    state.actors.spawn(Actor{.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 8}, .setsOff = {On::Touch}},
                       kA, {44, 44});
    ticks(3);
    EXPECT_TRUE(state.flag);   // ownTouch

    // The fixture's config leaves the interact action empty, so ActorKit::Interact stays quiet under it;
    // named another action, the same; named the one the hero performs, it starts.
    state.actors.spawn(Actor{.slot = "actors", .mover = ActorKit::movedBy<Slider>(SliderConfig{}, &Input::way),
                             .actions = {{Action::Interact, &Input::interact}}},
                       kA, {40, 48});
    tick();
    tick(Input{.way = {.up = true}, .interact = true});
    tick();
    EXPECT_EQ(state.count, 0);
    config.interactAction = Action::Kick;
    tick(Input{.way = {.up = true}, .interact = true});
    tick();
    EXPECT_EQ(state.count, 0);
    config.interactAction = Action::Interact;
    tick(Input{.way = {.up = true}, .interact = true});
    tick();
    EXPECT_EQ(state.count, 5);

    EXPECT_THROW((ActorKit::TriggerMap{{ActorKit::On::Touch, ActorKit::near()}}), std::invalid_argument);
}

// ── actedOn ────────────────────────────────────────────────────────────────────────────────────────

const ScriptKit::Script fromBelow{.name = "below", .on = Interact, .from = ActorKit::Below, .body = {set(Count, 1)}};
const ScriptKit::Script fromAbove{.name = "above", .on = Interact, .from = ActorKit::Above, .body = {set(Count, 2)}};
const ScriptKit::Script fromLeft{.name = "left", .on = Interact, .from = ActorKit::Left, .body = {set(Count, 3)}};
const ScriptKit::Script fromRight{.name = "right", .on = Interact, .from = ActorKit::Right, .body = {set(Count, 4)}};
const ScriptKit::Script fromAny{.name = "any", .on = Interact, .body = {set(Lit, true)}};

class ActedOn : public Farm {
protected:
    Id hero = 0;
    Id bell = 0;

    void SetUp() override {
        Farm::SetUp();
        hero = state.actors.spawn(Actor{.slot = "actors", .mover = ActorKit::movedBy<Slider>(SliderConfig{}, &Input::way),
                                        .actions = {{Action::Interact, &Input::interact}}},
                                  kA, {0, 0});
        bell = state.actors.spawn(Actor{.slot = "actors", .box = {.x = 0, .y = 0, .width = 16, .height = 8},
                                        .solid = true,
                                        .scripts = {&fromBelow, &fromAbove, &fromLeft, &fromRight, &fromAny}},
                                  kA, {64, 64});
    }

    // Puts the hero at `at`, faces it `way` (walking into the bell, which stops it), presses Interact,
    // and lets the script's request be written.
    void press(Vec2i at, Way way) {
        state.count = 0;
        state.lit = false;
        state.actors.moveTo(hero, kA, at);
        tick(Input{.way = way, .interact = true});
        tick();
    }
};

TEST_F(ActedOn, TheSideIsWhereTheActingActorStandsAndFromMustMatch) {
    press({68, 72}, {.up = true});   // below, facing up
    EXPECT_EQ(state.count, 1);
    EXPECT_TRUE(state.lit);
    press({68, 56}, {.down = true});   // above, facing down
    EXPECT_EQ(state.count, 2);
    press({56, 64}, {.right = true});   // left of it, facing right
    EXPECT_EQ(state.count, 3);
    press({80, 64}, {.left = true});   // right of it, facing left
    EXPECT_EQ(state.count, 4);
}

TEST_F(ActedOn, FacingAwayActsOnNothing) {
    press({68, 72}, {.down = true});
    EXPECT_EQ(state.count, 0);
    EXPECT_FALSE(state.lit);
}

TEST_F(ActedOn, TheActorActedOnIsTheOneWhoseBoxHoldsThePointPastTheFacedSide) {
    // A second bell beside the first: the hero's middle is under the second.
    const Id other = state.actors.spawn(Actor{.slot = "actors", .box = {.x = 0, .y = 0, .width = 16, .height = 8},
                                              .scripts = {raise(Flag)}},
                                        kA, {80, 64});
    press({84, 72}, {.up = true});
    EXPECT_TRUE(state.flag);
    EXPECT_EQ(state.count, 0);
    (void)other;
}

TEST_F(ActedOn, ActPerformsAnActionOnTheNextTick) {
    state.actors.moveTo(hero, kA, {68, 72});
    tick(Input{.way = {.up = true}});
    state.actors.act(hero, Action::Interact);
    tick();
    tick();
    EXPECT_EQ(state.count, 1);
}

// ── touched, near, entered, becomes ────────────────────────────────────────────────────────────────

const ScriptKit::Script countTouch{.name = "count touch", .on = Touch, .body = {set(Count, Count + 1)}};

TEST_F(Farm, TouchHappensOnTheFirstTickOfOverlapAgainAfterPartingOnlyFromASetterAndOnlyInOneArea) {
    state.actors.spawn(block({&countTouch}), kA, {40, 40});
    const Id toucher = state.actors.spawn(Actor{.box = {.x = 0, .y = 0, .width = 8, .height = 8}, .setsOff = {On::Touch}},
                                          kA, {100, 100});
    const Id bystander = state.actors.spawn(Actor{.box = {.x = 0, .y = 0, .width = 8, .height = 8}}, kA, {100, 100});
    ticks(2);
    EXPECT_EQ(state.count, 0);

    state.actors.moveTo(bystander, kA, {42, 42});
    ticks(3);
    EXPECT_EQ(state.count, 0);

    state.actors.moveTo(toucher, kA, {44, 44});
    ticks(4);
    EXPECT_EQ(state.count, 1);   // once, though the overlap lasts

    state.actors.moveTo(toucher, kA, {100, 100});
    ticks(2);
    state.actors.moveTo(toucher, kA, {44, 44});
    ticks(2);
    EXPECT_EQ(state.count, 2);

    state.actors.moveTo(toucher, kB, {100, 100});
    ticks(2);
    state.actors.moveTo(toucher, kB, {44, 44});   // the same pixels, another area
    ticks(2);
    EXPECT_EQ(state.count, 2);
}

const ScriptKit::Script near24{.name = "near 24", .on = Near(24), .body = {set(Count, Count + 1)}};
const ScriptKit::Script near8{.name = "near 8", .on = Near(8), .body = {set(Lit, true)}};

TEST_F(Farm, NearHappensWhenASetterCrossesTheScriptsDistanceInward) {
    state.actors.spawn(block({&near24, &near8}), kA, {40, 40});   // its box: 40..48
    const Id walker = state.actors.spawn(Actor{.box = {.x = 0, .y = 0, .width = 8, .height = 8}, .setsOff = {On::Near}},
                                         kA, {80, 40});   // 32 px away
    ticks(2);
    EXPECT_EQ(state.count, 0);
    state.actors.moveTo(walker, kA, {68, 40});   // 20 px
    ticks(2);
    EXPECT_EQ(state.count, 1);
    EXPECT_FALSE(state.lit);
    state.actors.moveTo(walker, kA, {54, 40});   // 6 px: inside 24 still, and now inside 8
    ticks(2);
    EXPECT_EQ(state.count, 1);
    EXPECT_TRUE(state.lit);
    state.actors.moveTo(walker, kA, {120, 40});
    ticks(2);
    state.actors.moveTo(walker, kA, {68, 40});
    ticks(2);
    EXPECT_EQ(state.count, 2);
}

TEST_F(Farm, AnActorSpawnedWithinANearDistanceSetsItOffOnItsFirstTick) {
    state.actors.spawn(Actor{.box = {.x = 0, .y = 0, .width = 8, .height = 8}, .setsOff = {On::Near}}, kA, {60, 40});
    tick();
    state.actors.spawn(block({&near24}), kA, {40, 40});
    ticks(2);
    EXPECT_EQ(state.count, 1);
}

const ScriptKit::Script countEnter{.name = "count enter", .on = Enter, .body = {set(Count, Count + 1)}};

class Entered : public Farm {
protected:
    Id hero = 0;

    void SetUp() override {
        Farm::SetUp();
        hero = state.actors.spawn(slider(SliderConfig{.speed = 2}), kA, {120, 60});
        state.actors.player(hero);
        state.camera.place(world, state.actors.box(hero));
        tick();
    }
};

TEST_F(Entered, AScrollIntoAnAreaSetsOffItsActors) {
    state.actors.spawn(block({&countEnter}), kB, {80, 60});
    ASSERT_TRUE(tickUntil(Input{.way = {.right = true}}, [&] { return state.camera.area() == 1; }));
    ticks(2);
    EXPECT_EQ(state.count, 1);
}

TEST_F(Entered, AFadeIntoAnAreaSetsOffItsActors) {
    state.actors.spawn(block({&countEnter}), kD, {24, 24});
    state.actors.moveTo(hero, kA, {100, 44});   // below the stairs at cells 12–13, row 4
    tick();
    ASSERT_TRUE(tickUntil(Input{.way = {.up = true}}, [&] { return state.camera.layout() == 1; }));
    ticks(2);
    EXPECT_EQ(state.count, 1);
}

TEST_F(Entered, ASeamIntoAnAreaSetsOffItsActorsAndASpawnIntoTheCamerasAreaDoesOnItsFirstTick) {
    state.actors.spawn(block({&countEnter}), kC, {80, 60});
    warp(hero, kB, {76, 20});
    tick();
    ASSERT_TRUE(tickUntil(Input{.way = {.up = true}}, [&] { return state.camera.area() == 2; }));
    ticks(2);
    EXPECT_EQ(state.count, 1);
    state.actors.spawn(block({&countEnter}), kC, {40, 60});
    state.actors.spawn(block({&countEnter}), kA, {40, 60});   // another area: stays quiet
    ticks(3);
    EXPECT_EQ(state.count, 2);
}

const ScriptKit::Script whenFlag{.name = "when flag", .on = Becomes(Flag), .body = {set(Count, Count + 1)}};
const ScriptKit::Script whenUp{.name = "when up", .on = Becomes(Up), .body = {set(Lit, true)}};

TEST_F(Farm, BecomesHappensWhenTheValueGoesFromFalseToTrueOwnOrTheGames) {
    state.flag = true;
    const Id watcher = state.actors.spawn(Actor{.ownValues = {{Up, false}}, .scripts = {&whenFlag, &whenUp}}, kA, {});
    ticks(3);
    EXPECT_EQ(state.count, 0);   // already true on the first tick
    state.flag = false;
    ticks(2);
    state.flag = true;
    ticks(2);
    EXPECT_EQ(state.count, 1);
    state.actors.set(watcher, Up, true);
    ticks(2);
    EXPECT_TRUE(state.lit);
}

// ── Starting scripts ───────────────────────────────────────────────────────────────────────────────

const ScriptKit::Script holdOn{.name = "hold on", .on = Enter, .body = {set(Count, Count + 1), ScriptKit::waitWhile(true)}};
const ScriptKit::Script alsoOn{.name = "also on", .on = Enter, .body = {set(Lit, true)}};
const ScriptKit::Script onlyIf{.name = "only if", .on = Enter, .when = Flag, .body = {set(SeenX, 9)}};

TEST_F(Farm, ATriggerStartsEveryMatchingScriptOfTheActorLeavesARunningOneAndChecksWhen) {
    const Id a = state.actors.spawn(block({&holdOn, &alsoOn, &onlyIf}), kA, {40, 40});
    const Id b = state.actors.spawn(block({&holdOn}), kB, {40, 40});   // in an area other than the camera's
    ticks(2);
    EXPECT_EQ(state.count, 1);
    EXPECT_TRUE(state.lit);
    EXPECT_EQ(state.seenX, 0);
    EXPECT_TRUE(state.actors.processor(a).running(holdOn));
    EXPECT_FALSE(state.actors.processor(b).running(holdOn));

    // Back into the area: Enter happens again; the running script is left as it is.
    state.camera = WorldKit::Camera(0, 1);
    tick();
    state.camera = WorldKit::Camera(0, 0);
    state.flag = true;
    ticks(2);
    EXPECT_EQ(state.count, 2);   // b's, in area 1, on the tick the camera was there
    EXPECT_EQ(state.seenX, 9);
}

// ── Solidity and walking ───────────────────────────────────────────────────────────────────────────

TEST_F(Farm, AnActorSolidByAValueStopsAWalkerUntilTheValueChanges) {
    const Id hero = state.actors.spawn(slider(), kA, {40, 40});
    state.actors.spawn(Actor{.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 8}, .solid = !Flag}, kA,
                       {52, 40});
    ticks(10, Input{.way = {.right = true}});
    EXPECT_EQ(state.actors.position(hero), (Vec2i{44, 40}));
    state.flag = true;
    ticks(10, Input{.way = {.right = true}});
    EXPECT_EQ(state.actors.position(hero), (Vec2i{54, 40}));
}

Way wayOf(const ScriptKit::Values& values) { return Way{.down = values.get(Mine) > 0}; }

TEST_F(Farm, AMoverIsWalkedByAnInputMemberADirValueOrAFunctionOfTheActorsValues) {
    const Id byInput = state.actors.spawn(slider(), kA, {16, 16});
    const Id byValue = state.actors.spawn(Actor{.slot = "actors", .ownValues = {{Heading, Dir::Right}},
                                                .mover = ActorKit::movedBy<Slider>(SliderConfig{}, Heading)},
                                          kA, {16, 32});
    const Id byFunction = state.actors.spawn(Actor{.slot = "actors", .ownValues = {{Mine, 1}},
                                                   .mover = ActorKit::movedBy<Slider>(SliderConfig{}, wayOf)},
                                             kA, {16, 48});
    const TopDownMovement::WalkerConfig walkerConfig{.collision = IntRect{.x = 0, .y = 0, .width = 8, .height = 8}};
    const Id walker = state.actors.spawn(Actor{.slot = "actors", .ownValues = {{Heading, Dir::Down}},
                                               .mover = ActorKit::movedBy<TopDownMovement::Walker>(walkerConfig, Heading)},
                                         kA, {64, 16});
    const Id byDirections = state.actors.spawn(
        Actor{.slot = "actors", .mover = ActorKit::movedBy<TopDownMovement::Walker>(walkerConfig, &Input::heading)}, kA,
        {96, 16});
    ticks(3, Input{.way = {.left = true}, .heading = {.up = true}});
    EXPECT_EQ(state.actors.position(byInput), (Vec2i{13, 16}));
    EXPECT_EQ(state.actors.position(byValue), (Vec2i{19, 32}));
    EXPECT_EQ(state.actors.position(byFunction), (Vec2i{16, 51}));
    EXPECT_EQ(state.actors.position(walker), (Vec2i{64, 19}));
    EXPECT_EQ(state.actors.facing(walker), Dir::Down);
    EXPECT_EQ(state.actors.position(byDirections), (Vec2i{96, 13}));
    state.actors.set(walker, Heading, Dir::None);
    tick();
    EXPECT_EQ(state.actors.position(walker), (Vec2i{64, 19}));
}

TEST_F(Farm, AWalkerStopsAtTheMapAndOtherSolidActorsNeverAtItselfAndAnyButThePlayerAtItsAreasEdge) {
    const Id hero = state.actors.spawn(slider(SliderConfig{.speed = 4}), kA, {16, 16});
    ticks(5, Input{.way = {.up = true}});
    EXPECT_EQ(state.actors.position(hero), (Vec2i{16, 8}));   // the wall is row 0
    state.actors.spawn(Actor{.box = {.x = 0, .y = 0, .width = 8, .height = 8}, .solid = true}, kA, {40, 8});
    ticks(10, Input{.way = {.right = true}});
    EXPECT_EQ(state.actors.position(hero), (Vec2i{32, 8}));

    // Any actor but the player stops at the open door's edge.
    state.actors.moveTo(hero, kA, {140, 60});
    ticks(20, Input{.way = {.right = true}});
    EXPECT_EQ(state.actors.position(hero), (Vec2i{152, 60}));
    EXPECT_EQ(state.camera.area(), 0);
}

// ── The player ─────────────────────────────────────────────────────────────────────────────────────

TEST_F(Entered, ThePlayerScrollsIntoTheNextAreaAndNothingWalksWhileTheScrollRuns) {
    const Id other = state.actors.spawn(slider(), kA, {40, 100});
    std::optional<Place> entered;
    bool scrolled = false;
    Vec2i otherAt{};
    for (int n = 0; n < 200 && !entered; ++n) {
        if (!scrolled && state.camera.transitioning()) {
            scrolled = true;
            otherAt = state.actors.position(other);
        }
        entered = tick(Input{.way = {.right = true}}).entered;
        if (scrolled && !entered) {
            EXPECT_EQ(state.actors.position(other), otherAt);
        }
    }
    ASSERT_TRUE(entered.has_value());
    EXPECT_EQ(*entered, kB);
    EXPECT_EQ(state.actors.place(hero), kB);
    EXPECT_LT(state.actors.position(hero).x, 40);   // in the new area's pixels, carried in from the west
    EXPECT_EQ(state.actors.place(other), kA);
}

TEST_F(Entered, ThePlayerFadesIntoAnotherLayoutAndCrossesASeam) {
    state.actors.moveTo(hero, kA, {100, 44});
    tick();
    std::optional<Place> entered;
    for (int n = 0; n < 100 && !entered; ++n) {
        entered = tick(Input{.way = {.up = true}}).entered;
    }
    ASSERT_TRUE(entered.has_value());
    EXPECT_EQ(*entered, kD);
    EXPECT_EQ(state.actors.place(hero), kD);

    warp(hero, kB, {76, 20});
    tick();
    EXPECT_EQ(state.camera.area(), 1);
    entered.reset();
    for (int n = 0; n < 100 && !entered; ++n) {
        entered = tick(Input{.way = {.up = true}}).entered;
    }
    ASSERT_TRUE(entered.has_value());
    EXPECT_EQ(*entered, kC);
    EXPECT_EQ(state.actors.place(hero), kC);
    EXPECT_GT(state.actors.position(hero).y, 100);   // the bottom of area 2, in its own pixels
}

TEST_F(Farm, PlayerRecordsThePlayerAndTheCameraStaysWhereTheGamePutIt) {
    const Id hero = state.actors.spawn(slider(), kC, {80, 60});
    EXPECT_EQ(state.actors.player(), ActorKit::kNoId);
    state.actors.player(hero);
    EXPECT_EQ(state.actors.player(), hero);
    const Vec2i cameraAt = state.camera.position();
    for (int n = 0; n < 5; ++n) {
        ActorKit::tick(state.actors, config, state.camera, Input{.way = {.right = true}}, state);
    }
    EXPECT_EQ(state.actors.position(hero), (Vec2i{85, 60}));
    EXPECT_EQ(state.camera.layout(), 0);
    EXPECT_EQ(state.camera.area(), 0);   // the hero is in area 2; the camera is the game's to move
    EXPECT_EQ(state.camera.position(), cameraAt);
    state.actors.moveTo(hero, kD, {40, 32});
    EXPECT_EQ(state.actors.place(hero), kD);
    EXPECT_EQ(state.camera.layout(), 0);
    EXPECT_THROW(state.actors.player(99), std::out_of_range);
    state.actors.remove(hero);
    EXPECT_EQ(state.actors.player(), ActorKit::kNoId);
}

// ── The order of a tick ────────────────────────────────────────────────────────────────────────────

const ScriptKit::Script headRight{.name = "head right", .on = Enter, .body = {set(Heading, Dir::Right)}};

TEST_F(Farm, ARequestMadeOnOneTickIsWrittenAtTheTopOfTheNextBeforeAnythingWalks) {
    const Id mover = state.actors.spawn(Actor{.slot = "actors", .ownValues = {{Heading, Dir::None}},
                                              .scripts = {&headRight},
                                              .mover = ActorKit::movedBy<Slider>(SliderConfig{}, Heading)},
                                        kA, {40, 40});
    tick();
    EXPECT_EQ(state.actors.position(mover), (Vec2i{40, 40}));
    EXPECT_EQ(state.actors.get(mover, Heading), Dir::None);
    tick();
    EXPECT_EQ(state.actors.get(mover, Heading), Dir::Right);
    EXPECT_EQ(state.actors.position(mover), (Vec2i{41, 40}));
}

// ── spawn, remove and handlers from a script ───────────────────────────────────────────────────────

const ScriptKit::Script lightUp{.name = "light up", .on = Enter, .body = {set(Lit, true)}};
const Actor kEgg{.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 8}, .scripts = {&lightUp}};
const ScriptKit::Script layEgg{.name = "lay egg", .on = Enter, .body = {ActorKit::spawn(kEgg, {4, 8})}};

TEST_F(Farm, ASpawnFromAScriptPutsTheNewActorAtTheAskersPositionPlusTheOffsetFromTheNextTick) {
    const Id hen = state.actors.spawn(block({&layEgg}), kA, {40, 40});
    tick();
    EXPECT_EQ(state.actors.size(), 1u);
    tick();
    ASSERT_EQ(state.actors.size(), 2u);
    const Id egg = state.actors.ids()[1];
    EXPECT_EQ(state.actors.place(egg), kA);
    EXPECT_EQ(state.actors.position(egg), (Vec2i{44, 48}));
    EXPECT_TRUE(state.actors.processor(egg).idle());   // it takes part from the next tick
    tick();
    EXPECT_FALSE(state.lit);
    tick();
    EXPECT_TRUE(state.lit);
    (void)hen;
}

const ScriptKit::Script leave{.name = "leave", .on = Enter, .body = {ActorKit::remove(), set(Flag, true)}};

TEST_F(Farm, ARemoveFromAScriptTakesEffectWhenTheTicksRequestsAreWrittenAndItsLaterRequestsStillAre) {
    const Id leaver = state.actors.spawn(block({&leave}), kA, {40, 40});
    tick();
    EXPECT_TRUE(state.actors.has(leaver));
    tick();
    EXPECT_FALSE(state.actors.has(leaver));
    EXPECT_TRUE(state.flag);
}

const ScriptKit::Script shout{.name = "shout", .on = Enter, .body = {Shout()}};
const ScriptKit::Script whisper{.name = "whisper", .on = Enter, .body = {Whisper()}};

TEST_F(Farm, AHandlerIsHandedTheAskingActorsIdAndACommandWithNoHandlerThrowsNamingIt) {
    state.actors.spawn(block(), kA, {16, 16});
    const Id shouter = state.actors.spawn(block({&shout}), kA, {40, 40});
    ticks(2);
    EXPECT_EQ(state.shouts, 1);
    EXPECT_EQ(state.asker, shouter);
    state.actors.spawn(block({&whisper}), kA, {60, 40});
    tick();
    EXPECT_TRUE(mentions(thrown([&] { tick(); }), "whisper"));
}

TEST_F(Farm, ACommandIsDoneOnceForEveryActorThatAsksOnOneTickWhereASetOfASharedValueLandsOnce) {
    // Three actors start on the same tick. Each asks for the command: three calls of the handler. Each
    // also sets the game's count to one more than it read, and all three read the same count.
    for (int n = 0; n < 3; ++n) {
        state.actors.spawn(block({&shout, &addNow}), kA, {16 + n * 16, 16});
    }
    ticks(2);
    EXPECT_EQ(state.shouts, 3);
    EXPECT_EQ(state.count, 1);
}

struct OtherInput {
    Way way;
};

TEST_F(Farm, AnInputOfAnotherTypeThanTheDrivesThrows) {
    state.actors.spawn(slider(), kA, {40, 40});
    EXPECT_THROW(ActorKit::tick(state.actors, config, state.camera, OtherInput{}, state), std::logic_error);
}

struct OtherState {
    int count = 0;
};

TEST_F(Farm, AStateOfAnotherTypeThanTheBindingsOrTheHandlersWereWrittenForThrows) {
    // Bindings and handlers written for State, handed a tick and a Sprites call with an OtherState.
    OtherState other;
    const ActorKit::Look look = ActorKit::Look{.size = {.width = 8, .height = 8}};
    ActorKit::Actors readers{kConfig};
    readers.spawn(Actor{.slot = "actors", .look = {{Flag, look}}}, kA, {16, 16});
    EXPECT_THROW(ActorKit::tick(readers, config, state.camera, Input{}, other), std::logic_error);
    EXPECT_THROW((void)ActorKit::Sprites(readers, config, kA, "actors", other), std::logic_error);

    ActorKit::Actors askers{kConfig};
    askers.spawn(block({&shout}), kA, {16, 16});
    const ActorKit::Config handlersOnly{.world = &world, .triggers = triggers(), .handlers = &kHandlers};
    ActorKit::tick(askers, handlersOnly, state.camera, Input{}, state);   // the script asks
    EXPECT_THROW(ActorKit::tick(askers, handlersOnly, state.camera, Input{}, other), std::logic_error);

    // A config of a world alone takes a state of any type.
    ActorKit::Actors plain{kConfig};
    plain.spawn(block(), kA, {16, 16});
    const ActorKit::Config bare{.world = &world};
    EXPECT_NO_THROW(ActorKit::tick(plain, bare, state.camera, Input{}, other));
}

// ── Looks and the sprites the actors report ────────────────────────────────────────────────────────

retropp::AtlasManifest sheet() {
    return retropp::AtlasManifest{.atlasId = retropp::AtlasId{3},
                                  .slots = {{.tile = 0, .dimensions = {.width = 16, .height = 16}},
                                            {.tile = 2, .dimensions = {.width = 16, .height = 16}},
                                            {.tile = 4, .dimensions = {.width = 8, .height = 8}}}};
}

const retropp::Sprite* spriteKeyed(const LayerKit::Sprites& sprites, const std::string& key) {
    for (const retropp::Sprite& sprite : sprites) {
        if (sprite.key.value == key) {
            return &sprite;
        }
    }
    return nullptr;
}

TEST_F(Farm, TheFirstLookThatHoldsIsShownAndAnActorWithNoneHoldingAndNoMoverIsLeftOut) {
    const ActorKit::Look open = ActorKit::lookOf(sheet(), 0, retropp::PaletteId{1});
    const ActorKit::Look shut = ActorKit::lookOf(sheet(), 1, retropp::PaletteId{1});
    EXPECT_EQ(shut.tile, 2);
    EXPECT_EQ(shut.size, (retropp::AssetDimensions{.width = 16, .height = 16}));
    EXPECT_THROW((void)ActorKit::lookOf(sheet(), 9, retropp::PaletteId{1}), std::out_of_range);

    const Id gate = state.actors.spawn(Actor{.slot = "actors", .look = {{Flag, open}, {shut}}}, kA, {40, 40});
    const Id lamp = state.actors.spawn(Actor{.slot = "actors", .ownValues = {{Up, false}}, .look = {{Up, open}}}, kA,
                                       {80, 40});
    LayerKit::Sprites sprites = ActorKit::Sprites(state.actors, config, kA, "actors", state);
    ASSERT_NE(spriteKeyed(sprites, state.actors.key(gate)), nullptr);
    EXPECT_EQ(spriteKeyed(sprites, state.actors.key(gate))->tile, 2);
    EXPECT_EQ(spriteKeyed(sprites, state.actors.key(lamp)), nullptr);

    state.flag = true;
    state.actors.set(lamp, Up, true);
    sprites = ActorKit::Sprites(state.actors, config, kA, "actors", state);
    EXPECT_EQ(spriteKeyed(sprites, state.actors.key(gate))->tile, 0);
    ASSERT_NE(spriteKeyed(sprites, state.actors.key(lamp)), nullptr);
}

TEST_F(Farm, SpritesAreTheActorsOfOnePlaceAtOneSlotInSpawnOrder) {
    const ActorKit::Look look = ActorKit::lookOf(sheet(), 2, retropp::PaletteId{1});
    const Id first = state.actors.spawn(Actor{.slot = "actors", .look = look}, kA, {40, 40});
    const Id under = state.actors.spawn(Actor{.slot = "underfoot", .look = look}, kA, {60, 40});
    const Id elsewhere = state.actors.spawn(Actor{.slot = "actors", .look = look}, kB, {60, 40});
    const Id second = state.actors.spawn(Actor{.slot = "actors", .look = look}, kA, {80, 40});

    const LayerKit::Sprites actors = ActorKit::Sprites(state.actors, config, kA, "actors", state);
    ASSERT_EQ(actors.size(), 2u);
    EXPECT_EQ(actors[0].key.value, state.actors.key(first));
    EXPECT_EQ(actors[1].key.value, state.actors.key(second));

    const LayerKit::Sprites underfoot = ActorKit::Sprites(state.actors, config, kA, "underfoot", state);
    ASSERT_EQ(underfoot.size(), 1u);
    EXPECT_EQ(underfoot[0].key.value, state.actors.key(under));

    const LayerKit::Sprites next = ActorKit::Sprites(state.actors, config, kB, "actors", state);
    ASSERT_EQ(next.size(), 1u);
    EXPECT_EQ(next[0].key.value, state.actors.key(elsewhere));
    EXPECT_TRUE(ActorKit::Sprites(state.actors, config, kA, "overhead", state).empty());
}

TEST_F(Farm, TheGameDrawsTheActorsSpritesThroughWorldKitBesideASpriteOfItsOwn) {
    const ActorKit::Look look = ActorKit::lookOf(sheet(), 2, retropp::PaletteId{1});
    const Id actor = state.actors.spawn(Actor{.slot = "actors", .look = look}, kA, {40, 40});
    const LayerKit::Sprites own{retropp::Sprite{.key = "the game's own", .x = 8, .y = 8}};

    LayerKit::Layers layers = WorldKit::WorldLayers(state.camera, world);
    const std::size_t worldLayers = layers.size();
    layers.push_back(WorldKit::Actors(state.camera, world, "actors",
                                      {ActorKit::Sprites(state.actors, config, kA, "actors", state), own}));
    ASSERT_EQ(layers.size(), worldLayers + 1);
    EXPECT_EQ(layers.back().key.value, WorldKit::ActorsKey("actors"));
    EXPECT_EQ(layers.back().z, 5);
    const auto* content = std::get_if<retropp::SpriteContent>(&layers.back().content);
    ASSERT_NE(content, nullptr);
    ASSERT_EQ(content->sprites.size(), 2u);
    EXPECT_EQ(content->sprites[0].key.value, state.actors.key(actor));
    EXPECT_EQ(content->sprites[1].key.value, "the game's own");
}

TEST_F(Farm, AnActorsSpriteIsKeyedByItsIdAtItsPositionAndSortsByTheBottomOfItsBox) {
    const ActorKit::Look look = ActorKit::lookOf(sheet(), 0, retropp::PaletteId{6});
    const Id low = state.actors.spawn(Actor{.slot = "actors", .box = {.x = 2, .y = 10, .width = 12, .height = 6}, .look = look},
                                      kA, {40, 50});
    const Id high = state.actors.spawn(Actor{.slot = "actors", .look = look}, kA, {60, 30});
    const LayerKit::Sprites sprites = ActorKit::Sprites(state.actors, config, kA, "actors", state);
    const retropp::Sprite* first = spriteKeyed(sprites, state.actors.key(low));
    const retropp::Sprite* second = spriteKeyed(sprites, state.actors.key(high));
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(first->x, 40);
    EXPECT_EQ(first->y, 50);
    EXPECT_EQ(first->z, 66);    // 50 + the box's bottom, 16
    EXPECT_EQ(second->z, 46);   // 30 + the look's height, its box being empty
    EXPECT_EQ(first->atlas, retropp::AtlasId{3});
    EXPECT_EQ(first->palette, retropp::PaletteId{6});
    EXPECT_LT(second->z, first->z);
}

// ── Sprite keys ────────────────────────────────────────────────────────────────────────────────────

TEST_F(Farm, TheActorsOfTwoActorsValuesHaveKeysOfTheirOwnAndACopyKeepsItsKeys) {
    ActorKit::Actors others{kConfig};
    const Id mine = state.actors.spawn(block(), kA, {16, 16});
    const Id theirs = others.spawn(block(), kA, {16, 16});
    EXPECT_EQ(mine, theirs);   // each value numbers its actors from 1
    EXPECT_NE(state.actors.key(mine), others.key(theirs));
    EXPECT_NE(state.actors.key(mine), state.actors.key(state.actors.spawn(block(), kA, {32, 16})));
    const ActorKit::Actors copy = state.actors;
    EXPECT_EQ(copy.key(mine), state.actors.key(mine));
    EXPECT_THROW((void)state.actors.key(99), std::out_of_range);
}

TEST_F(Farm, AFreshActorThatMovedTakesAThrowawayKeyAndItsOwnAgainWhenFreshIsNoLongerReported) {
    const Id hero = state.actors.spawn(block(), kA, {16, 16});
    tick();
    const std::string own = state.actors.key(hero);

    // Reported where it stands: the key stays.
    state.actors.fresh(hero);
    EXPECT_EQ(state.actors.key(hero), own);

    // Moved and reported: a throwaway key.
    tick();
    state.actors.moveTo(hero, kA, {96, 64});
    EXPECT_EQ(state.actors.key(hero), own);   // moved, and eased, until it is reported
    state.actors.fresh(hero);
    const std::string first = state.actors.key(hero);
    EXPECT_NE(first, own);

    // Moved and reported again on the next tick: the other throwaway key.
    tick();
    state.actors.moveTo(hero, kB, {40, 40});
    state.actors.fresh(hero);
    const std::string second = state.actors.key(hero);
    EXPECT_NE(second, own);
    EXPECT_NE(second, first);

    // And a third time: the first one again, free since the tick before took the second.
    tick();
    state.actors.moveTo(hero, kB, {44, 40});
    state.actors.fresh(hero);
    EXPECT_EQ(state.actors.key(hero), first);

    // Reported and standing still: it keeps the key it has.
    tick();
    state.actors.fresh(hero);
    EXPECT_EQ(state.actors.key(hero), first);

    // Reported on an earlier tick only: its own key again, moving or not.
    tick();
    state.actors.moveTo(hero, kB, {80, 40});
    EXPECT_EQ(state.actors.key(hero), own);
    EXPECT_THROW(state.actors.fresh(99), std::out_of_range);
}

TEST_F(Farm, AFreshActorsSpriteCarriesItsThrowawayKey) {
    const ActorKit::Look look = ActorKit::lookOf(sheet(), 0, retropp::PaletteId{1});
    const Id hero = state.actors.spawn(Actor{.slot = "actors", .look = look}, kA, {16, 16});
    tick();
    const std::string own = state.actors.key(hero);
    state.actors.moveTo(hero, kA, {96, 64});
    state.actors.fresh(hero);
    const LayerKit::Sprites sprites = ActorKit::Sprites(state.actors, config, kA, "actors", state);
    ASSERT_EQ(sprites.size(), 1u);
    EXPECT_NE(sprites[0].key.value, own);
    EXPECT_EQ(sprites[0].key.value, state.actors.key(hero));
    EXPECT_EQ(sprites[0].x, 96);
}

// ── A mover's frame, resolved through a renderer on a GPU device ───────────────────────────────────

// Windows on ARM has no production-representative GPU backend in CI; its production path (D3D12 + DXIL)
// is covered by the Windows x64 job, so a missing device there is a skip. Everywhere else it fails.
#if defined(_WIN32) && (defined(_M_ARM64) || defined(__aarch64__))
inline constexpr bool kDeviceOptional = true;
#else
inline constexpr bool kDeviceOptional = false;
#endif

class FramesOnDevice : public Farm {
protected:
    static inline SDL_GPUDevice* device_ = nullptr;
    static inline std::string error_;

    static void SetUpTestSuite() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            error_ = std::string("SDL_Init failed: ") + SDL_GetError();
            return;
        }
        device_ = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB,
                                      /*debug_mode=*/false, /*name=*/nullptr);
        if (device_ == nullptr) {
            error_ = std::string("SDL_CreateGPUDevice failed: ") + SDL_GetError();
        }
    }

    static void TearDownTestSuite() {
        if (device_ != nullptr) {
            SDL_DestroyGPUDevice(device_);
            device_ = nullptr;
        }
        SDL_Quit();
    }

    void SetUp() override {
        Farm::SetUp();
        if (device_ == nullptr) {
            if (kDeviceOptional) {
                GTEST_SKIP() << "Windows on ARM has no production-representative GPU backend in CI; the "
                                "Windows x64 job covers the D3D12 path. (" << error_ << ")";
            }
            FAIL() << "no GPU device: " << error_ << ". A frame's tile and size resolve through the renderer, which needs one.";
        }
    }
};

TEST_F(FramesOnDevice, AMoversFrameIsDrawnForItsActorAndALookThatHoldsIsDrawnOverIt) {
    retropp::Renderer renderer{device_, nullptr};
    const std::vector<std::uint8_t> pixels(32 * 16, 1);
    const retropp::AtlasManifest frames = renderer.uploadAtlas(pixels.data(), 32, 16, retropp::AssetDimensions{.width = 16, .height = 16},
                                                               retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown, 2);
    const SliderConfig walk{.collision = {.x = 2, .y = 8, .width = 12, .height = 8},
                            .frame = {.sheet = frames.atlasId, .tileIndex = 1, .palette = retropp::PaletteId{5},
                                      .duration = 100ms}};
    const ActorKit::Look asleep = ActorKit::lookOf(frames, 0, retropp::PaletteId{9});
    const Id dog = state.actors.spawn(Actor{.slot = "actors", .ownValues = {{Up, true}}, .look = {{!Up, asleep}},
                                            .mover = ActorKit::movedBy<Slider>(walk, &Input::way)},
                                      kA, {40, 40});
    LayerKit::Sprites sprites = ActorKit::Sprites(state.actors, config, kA, "actors", state);
    const retropp::Sprite* sprite = spriteKeyed(sprites, state.actors.key(dog));
    ASSERT_NE(sprite, nullptr);
    EXPECT_EQ(sprite->atlas, frames.atlasId);
    EXPECT_EQ(sprite->tile, frames[1].tile);
    EXPECT_EQ(sprite->palette, retropp::PaletteId{5});
    EXPECT_EQ(sprite->size, (retropp::AssetDimensions{.width = 16, .height = 16}));
    EXPECT_EQ(sprite->z, 40 + 16);   // the bottom of the mover's collision shape
    EXPECT_EQ(state.actors.box(dog), (IntRect{.x = 42, .y = 48, .width = 12, .height = 8}));

    state.actors.set(dog, Up, false);
    sprites = ActorKit::Sprites(state.actors, config, kA, "actors", state);
    sprite = spriteKeyed(sprites, state.actors.key(dog));
    ASSERT_NE(sprite, nullptr);
    EXPECT_EQ(sprite->tile, frames[0].tile);
    EXPECT_EQ(sprite->palette, retropp::PaletteId{9});
}

}  // namespace
