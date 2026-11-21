// ActorKit's frozen actors: freeze, wake and frozen. A frozen actor stays exactly as it is — its
// position, its values, where its scripts are and the ticks left in their waits, what they asked for,
// its solidity — and is still there: reported by Sprites, solid to walkers while it is solid, and a setter
// of touch to the awake actors it meets. Its scripts stay off, an action aimed at it goes to the awake
// actor behind it, and its place goes unchecked. On waking it carries on from what it held. The world is
// built in code, the actors are moved by a mover of the test's own, and the tests run headless.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include "retropp/animation.h"
#include "retropp/atlas_manifest.h"
#include "retropp/draw_state.h"
#include "retropp/engine_config.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/input_actions.h"
#include "retropp/palette.h"
#include "retropp/tilemap.h"
#include "retropp/timing.h"

#include "ScriptKit/bindings.h"
#include "ScriptKit/command.h"
#include "ScriptKit/entry.h"
#include "ScriptKit/expression.h"
#include "ScriptKit/script.h"
#include "ScriptKit/value.h"

#include "CameraKit/camera.h"

#include "LayerKit/types.h"

#include "WorldKit/area.h"
#include "WorldKit/camera.h"

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

retropp::EngineConfig configAt60() {
    retropp::EngineConfig config;
    config.timing = retropp::TimingProfile{.tickPeriodNs = retropp::TickPeriodNs::Hz60};
    return config;
}
const retropp::EngineConfig kConfig = configAt60();

// ── The world: one layout, two areas ───────────────────────────────────────────────────────────────

enum class Cell : std::uint16_t { Wall = 0, Open = 1 };

constexpr int kCell = 8;
constexpr CameraKit::View kView{.size = {.width = 160, .height = 128}, .origin = {.x = 0, .y = 16}};

retropp::IndexGrid room(int width, int height, int doorColumn) {
    retropp::IndexGrid map{.width = width, .height = height};
    map.values.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool edge = x == 0 || y == 0 || x == width - 1 || y == height - 1;
            const bool door = x == doorColumn && y >= 6 && y <= 9;
            map.values[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] =
                static_cast<std::uint16_t>(edge && !door ? Cell::Wall : Cell::Open);
        }
    }
    return map;
}

retropp::TileCatalog art() {
    return retropp::TileCatalog{.entries = {
        {.id = 0, .sheet = retropp::AtlasId{7}, .slot = 1, .palette = retropp::PaletteId{4}},
        {.id = 1, .sheet = retropp::AtlasId{7}, .slot = 0, .palette = retropp::PaletteId{4}},
    }};
}

// The court, one screen, with an east door (rows 6–9) scrolling into the hall, one screen, and back.
struct Maps {
    retropp::IndexGrid court = room(20, 16, 19);
    retropp::IndexGrid hall = room(20, 16, 0);
};

WorldKit::World worldOver(const Maps& maps) {
    const auto area = [](std::string name, const retropp::IndexGrid& map, WorldKit::Opening door) {
        return WorldKit::Area{.name = std::move(name),
                              .layers = {{.name = "ground", .tiles = &map, .art = art()}},
                              .actorSlots = {{.name = "actors", .z = 5}},
                              .collision = &map,
                              .openings = {std::move(door)}};
    };
    return WorldKit::World{.layouts = {WorldKit::Layout{
        .name = "keep",
        .areas = {area("court", maps.court,
                       {.name = "east door", .side = Side::East, .span = {6, 9}, .transition = Transition::Scroll,
                        .leadsTo = {.area = 1, .opening = "west door"}}),
                  area("hall", maps.hall,
                       {.name = "west door", .side = Side::West, .span = {6, 9}, .transition = Transition::Scroll,
                        .leadsTo = {.area = 0, .opening = "east door"}})},
        .cellSize = kCell,
        .open = Cell::Open,
        .view = kView,
    }}};
}

constexpr Place kCourt{.layout = 0, .area = 0};

// ── A mover of the test's own ──────────────────────────────────────────────────────────────────────

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

enum class Action : std::uint8_t { Interact };

struct Input {
    Way way;
    bool interact = false;
};

const ScriptKit::Value<int> Count;
const ScriptKit::Value<bool> Flag;
const ScriptKit::Value<bool> Lit;

const ScriptKit::Value<int> Mine;
const ScriptKit::Value<bool> Up;

const ScriptKit::Command Shout{"shout"};
const ScriptKit::Command Halt{"halt"};

struct State {
    ActorKit::Actors actors{kConfig};
    WorldKit::Camera camera{kCourt.layout, kCourt.area};
    int count = 0;
    bool flag = false;
    bool lit = false;
    int shouts = 0;
    Id halted = ActorKit::kNoId;   // the actor the halt command freezes
};

const ScriptKit::Bindings<State> kBindings{{Count, &State::count}, {Flag, &State::flag}, {Lit, &State::lit}};

const ActorKit::Handlers<State> kHandlers{
    {Shout, [](State& state, Id, const ScriptKit::Arguments&) { ++state.shouts; }},
    {Halt, [](State& state, Id, const ScriptKit::Arguments&) { state.actors.freeze(state.halted); }},
};

std::string thrown(const std::function<void()>& run) {
    try {
        run();
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

// A walker driven by the input's `way` that performs Interact on the input's `interact`, solid, 8 × 8.
Actor slider() {
    return {.slot = "actors",
            .solid = true,
            .mover = ActorKit::movedBy<Slider>(SliderConfig{}, &Input::way),
            .actions = {{Action::Interact, &Input::interact}}};
}

// An actor with an empty mover and an 8 × 8 box.
Actor block(ActorKit::Scripts scripts = {}) {
    return {.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 8}, .scripts = std::move(scripts)};
}

// A solid 16 × 8 actor to stand in front of and act on.
Actor bell(ActorKit::Scripts scripts) {
    return {.slot = "actors", .box = {.x = 0, .y = 0, .width = 16, .height = 8}, .solid = true,
            .scripts = std::move(scripts)};
}

class Freezing : public ::testing::Test {
protected:
    Maps maps;
    WorldKit::World world = worldOver(maps);
    State state;
    ActorKit::Config config{.world = &world, .interactAction = Action::Interact, .bindings = &kBindings,
                            .handlers = &kHandlers};

    void SetUp() override { WorldKit::validate(world); }

    void tick(const Input& input = {}) { ActorKit::tick(state.actors, config, state.camera, input, state); }

    void ticks(int count, const Input& input = {}) {
        for (int n = 0; n < count; ++n) {
            tick(input);
        }
    }
};

TEST_F(Freezing, ANewActorIsAwakeAndFreezeAndWakeAreReportedCarriedByACopyAndThrowForAnUnknownId) {
    const Id actor = state.actors.spawn(block(), kCourt, {40, 40});
    EXPECT_FALSE(state.actors.frozen(actor));
    state.actors.freeze(actor);
    state.actors.freeze(actor);
    EXPECT_TRUE(state.actors.frozen(actor));

    ActorKit::Actors copy = state.actors;
    EXPECT_TRUE(copy.frozen(actor));
    copy.wake(actor);
    EXPECT_FALSE(copy.frozen(actor));
    EXPECT_TRUE(state.actors.frozen(actor));

    state.actors.wake(actor);
    state.actors.wake(actor);
    EXPECT_FALSE(state.actors.frozen(actor));

    EXPECT_NE(thrown([&] { state.actors.freeze(99); }).find("99"), std::string::npos);
    EXPECT_THROW(state.actors.wake(99), std::out_of_range);
    EXPECT_THROW((void)state.actors.frozen(99), std::out_of_range);
}

TEST_F(Freezing, AFrozenWalkerStandsUnderItsDriveAndWalksOnWhenWoken) {
    const Id walker = state.actors.spawn(slider(), kCourt, {40, 40});
    tick(Input{.way = {.right = true}});
    EXPECT_EQ(state.actors.position(walker), (Vec2i{41, 40}));
    state.actors.freeze(walker);
    ticks(5, Input{.way = {.right = true}});
    EXPECT_EQ(state.actors.position(walker), (Vec2i{41, 40}));
    state.actors.wake(walker);
    ticks(5, Input{.way = {.right = true}});
    EXPECT_EQ(state.actors.position(walker), (Vec2i{46, 40}));
}

const ScriptKit::Script marking{.name = "marking", .on = ActorKit::Enter, .body = {wait(10), set(Mine, 1)}};

TEST_F(Freezing, AFrozenActorsScriptsHoldTheirPlaceAndAWaitIsAsLongOnWakingAsItWasWhenFrozen) {
    const Actor marker{.ownValues = {{Mine, 0}}, .scripts = {&marking}};
    const Id frozen = state.actors.spawn(marker, kCourt, {16, 16});
    const Id awake = state.actors.spawn(marker, kCourt, {32, 16});
    ticks(3);
    state.actors.freeze(frozen);
    const std::size_t position = state.actors.processor(frozen).position(marking);

    int awakeTicks = 3;
    while (state.actors.get(awake, Mine) == 0 && awakeTicks < 100) {
        tick();
        ++awakeTicks;
    }
    ticks(20);
    EXPECT_EQ(state.actors.get(frozen, Mine), 0);
    EXPECT_EQ(state.actors.processor(frozen).position(marking), position);

    state.actors.wake(frozen);
    int frozenTicks = 3;
    while (state.actors.get(frozen, Mine) == 0 && frozenTicks < 100) {
        tick();
        ++frozenTicks;
    }
    EXPECT_EQ(frozenTicks, awakeTicks);
}

const ScriptKit::Script asking{.name = "asking", .on = ActorKit::Enter, .body = {set(Count, 7), Shout()}};

TEST_F(Freezing, WhatAFrozenActorsScriptAskedForIsWrittenOnItsFirstTickAwake) {
    const Id asker = state.actors.spawn(block({&asking}), kCourt, {40, 40});
    tick();   // the script starts and asks
    state.actors.freeze(asker);
    ticks(3);
    EXPECT_EQ(state.count, 0);
    EXPECT_EQ(state.shouts, 0);
    state.actors.wake(asker);
    tick();
    EXPECT_EQ(state.count, 7);
    EXPECT_EQ(state.shouts, 1);
}

const ScriptKit::Script countTouch{.name = "count touch", .on = ActorKit::Touch, .body = {set(Count, Count + 1)}};
const ScriptKit::Script countNear{.name = "count near", .on = ActorKit::Near(24), .body = {set(Count, Count + 1)}};
const ScriptKit::Script countEnter{.name = "count enter", .on = ActorKit::Enter, .body = {set(Count, Count + 1)}};
const ScriptKit::Script countBecomes{.name = "count becomes", .on = ActorKit::Becomes(Flag),
                                     .body = {set(Count, Count + 1)}};
const ScriptKit::Script countInteract{.name = "count interact", .on = ActorKit::Interact,
                                      .body = {set(Count, Count + 1)}};

TEST_F(Freezing, NothingStartsOnAFrozenActorTouchNearEnterBecomesOrInteract) {
    const Id watcher = state.actors.spawn(
        Actor{.slot = "actors", .box = {.x = 0, .y = 0, .width = 16, .height = 8},
              .scripts = {&countTouch, &countNear, &countEnter, &countBecomes, &countInteract}},
        kCourt, {64, 64});
    state.actors.freeze(watcher);   // before its first tick, in the camera's area
    Actor hero = slider();
    hero.setsOff = {ActorKit::On::Touch, ActorKit::On::Near};
    state.actors.spawn(hero, kCourt, {68, 72});
    tick();
    state.flag = true;
    ticks(5, Input{.way = {.up = true}, .interact = true});   // walks into it, facing it, pressing Interact
    EXPECT_EQ(state.count, 0);
    EXPECT_TRUE(state.actors.processor(watcher).idle());
}

const ScriptKit::Script lightUp{.name = "light up", .on = ActorKit::Interact, .body = {set(Lit, true)}};
const ScriptKit::Script raiseFlag{.name = "raise flag", .on = ActorKit::Interact, .body = {set(Flag, true)}};

TEST_F(Freezing, AnActionAimedAtAFrozenActorPassesToTheAwakeActorBehindIt) {
    const Id front = state.actors.spawn(bell({&lightUp}), kCourt, {64, 64});
    state.actors.spawn(bell({&raiseFlag}), kCourt, {64, 64});
    state.actors.freeze(front);
    state.actors.spawn(slider(), kCourt, {68, 72});
    tick(Input{.way = {.up = true}, .interact = true});
    tick();
    EXPECT_TRUE(state.flag);
    EXPECT_FALSE(state.lit);
}

const ScriptKit::Script pressed{.name = "pressed", .on = ActorKit::Touch, .body = {set(Count, Count + 1)}};
const ScriptKit::Script watching{.name = "watching", .on = ActorKit::Enter,
                                 .body = {loop({set(Lit, ActorKit::Touched), wait(1)})}};

TEST_F(Freezing, AFrozenSolidActorStopsAWalkerAndAFrozenSetterKeepsAnAwakePlateTouched) {
    const Id wall = state.actors.spawn(Actor{.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 8},
                                             .solid = true},
                                       kCourt, {52, 40});
    state.actors.freeze(wall);
    const Id walker = state.actors.spawn(slider(), kCourt, {40, 40});
    ticks(10, Input{.way = {.right = true}});
    EXPECT_EQ(state.actors.position(walker), (Vec2i{44, 40}));

    state.actors.spawn(block({&pressed, &watching}), kCourt, {100, 100});
    Actor stone = block();
    stone.setsOff = {ActorKit::On::Touch};
    const Id setter = state.actors.spawn(stone, kCourt, {102, 102});
    state.actors.freeze(setter);
    ticks(5);
    EXPECT_EQ(state.count, 1);
    EXPECT_TRUE(state.lit);
}

TEST_F(Freezing, AFrozenActorKeepsTheSolidityItHadAndOneFrozenBeforeItsFirstTickHasItWorkedOut) {
    const Actor gate{.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 8}, .solid = Flag};
    state.flag = true;
    const Id first = state.actors.spawn(gate, kCourt, {52, 40});
    const Id walker = state.actors.spawn(slider(), kCourt, {40, 40});
    tick();
    state.actors.freeze(first);
    state.flag = false;
    ticks(10, Input{.way = {.right = true}});
    EXPECT_EQ(state.actors.position(walker), (Vec2i{44, 40}));

    state.actors.remove(first);
    state.flag = true;
    const Id second = state.actors.spawn(gate, kCourt, {52, 40});
    state.actors.freeze(second);
    ticks(5, Input{.way = {.right = true}});
    state.flag = false;
    ticks(5, Input{.way = {.right = true}});
    EXPECT_EQ(state.actors.position(walker), (Vec2i{44, 40}));
    state.actors.wake(second);
    ticks(10, Input{.way = {.right = true}});
    EXPECT_EQ(state.actors.position(walker), (Vec2i{54, 40}));
}

retropp::AtlasManifest sheet() {
    return retropp::AtlasManifest{.atlasId = retropp::AtlasId{3},
                                  .slots = {{.tile = 0, .dimensions = {.width = 16, .height = 16}},
                                            {.tile = 2, .dimensions = {.width = 16, .height = 16}}}};
}

TEST_F(Freezing, AFrozenActorIsReportedBySpritesAndItsLookFollowsItsValues) {
    const ActorKit::Look open = ActorKit::lookOf(sheet(), 0, retropp::PaletteId{1});
    const ActorKit::Look shut = ActorKit::lookOf(sheet(), 1, retropp::PaletteId{1});
    const Id door = state.actors.spawn(Actor{.slot = "actors", .ownValues = {{Up, false}}, .look = {{Up, open}, {shut}}},
                                       kCourt, {40, 40});
    state.actors.freeze(door);
    tick();
    LayerKit::Sprites sprites = ActorKit::Sprites(state.actors, config, kCourt, "actors", state);
    ASSERT_EQ(sprites.size(), 1u);
    EXPECT_EQ(sprites[0].key.value, state.actors.key(door));
    EXPECT_EQ(sprites[0].tile, 2);
    state.actors.set(door, Up, true);
    sprites = ActorKit::Sprites(state.actors, config, kCourt, "actors", state);
    ASSERT_EQ(sprites.size(), 1u);
    EXPECT_EQ(sprites[0].tile, 0);
}

const ScriptKit::Script countFlag{.name = "count flag", .on = ActorKit::Becomes(Flag), .body = {set(Count, Count + 1)}};
const ScriptKit::Script lightOnTouch{.name = "light on touch", .on = ActorKit::Touch, .body = {set(Lit, true)}};

TEST_F(Freezing, OnWakingItAnswersWhatChangedWhileItWasFrozen) {
    const Id watcher = state.actors.spawn(block({&countFlag, &lightOnTouch}), kCourt, {40, 40});
    ticks(2);
    state.actors.freeze(watcher);
    state.flag = true;
    Actor stone = block();
    stone.setsOff = {ActorKit::On::Touch};
    state.actors.spawn(stone, kCourt, {42, 42});
    ticks(3);
    EXPECT_EQ(state.count, 0);
    EXPECT_FALSE(state.lit);
    state.actors.wake(watcher);
    ticks(2);
    EXPECT_EQ(state.count, 1);
    EXPECT_TRUE(state.lit);
}

TEST_F(Freezing, TickReturnsWithAFrozenActorInAPlaceOutsideTheWorldAndThrowsOnceItIsWokenThere) {
    const Id stray = state.actors.spawn(block(), kCourt, {40, 40});
    tick();
    state.actors.moveTo(stray, Place{.layout = 5, .area = 0}, {});
    state.actors.freeze(stray);
    EXPECT_NO_THROW(tick());
    state.actors.wake(stray);
    EXPECT_NE(thrown([&] { tick(); }).find("actor " + std::to_string(stray)), std::string::npos);
}

const ScriptKit::Script halting{.name = "halting", .on = ActorKit::Enter, .body = {Halt()}};

TEST_F(Freezing, FrozenFromAHandlerAnActorStandsOnThatSameTick) {
    const Id walker = state.actors.spawn(slider(), kCourt, {40, 40});
    state.halted = walker;
    state.actors.spawn(block({&halting}), kCourt, {100, 100});
    tick(Input{.way = {.right = true}});   // the halt is asked for
    EXPECT_EQ(state.actors.position(walker), (Vec2i{41, 40}));
    tick(Input{.way = {.right = true}});   // the handler freezes the walker before it walks
    EXPECT_TRUE(state.actors.frozen(walker));
    EXPECT_EQ(state.actors.position(walker), (Vec2i{41, 40}));
}

const ScriptKit::Script ring{.name = "ring", .on = ActorKit::Interact, .body = {set(Count, 5)}};

TEST_F(Freezing, AnActionAskedForWithActOnAFrozenActorIsPerformedOnItsFirstTickAwake) {
    state.actors.spawn(bell({&ring}), kCourt, {64, 64});
    const Id hero = state.actors.spawn(slider(), kCourt, {68, 72});
    tick(Input{.way = {.up = true}});   // faces the bell
    state.actors.freeze(hero);
    state.actors.act(hero, Action::Interact);
    ticks(3);
    EXPECT_EQ(state.count, 0);
    state.actors.wake(hero);
    ticks(2);
    EXPECT_EQ(state.count, 5);
}

}  // namespace
