// WorldKit's scene: the world running. Building one spawns every actor the world lists, in the order the
// world is written; the camera follows the actor named to it and carries it through the openings as the
// camera's own calls do by hand; the frame is the world's layers and one sprite layer per area on screen
// and slot; a copy runs on by itself; spawn, validate and the scene's lookups throw what they say. The
// world is built in code, the actors are moved by a mover of the test's own, and the tests run headless.

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "retropp/animation.h"
#include "retropp/draw_state.h"
#include "retropp/engine_config.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/palette.h"
#include "retropp/tilemap.h"
#include "retropp/timing.h"

#include "ScriptKit/bindings.h"
#include "ScriptKit/command.h"
#include "ScriptKit/entry.h"
#include "ScriptKit/script.h"
#include "ScriptKit/value.h"

#include "CameraKit/camera.h"

#include "LayerKit/types.h"

#include "WorldKit/area.h"
#include "WorldKit/camera.h"
#include "WorldKit/scene.h"
#include "WorldKit/world_layers.h"

#include "ActorKit/actor.h"
#include "ActorKit/actors.h"
#include "ActorKit/commands.h"
#include "ActorKit/config.h"
#include "ActorKit/look.h"
#include "ActorKit/mover.h"
#include "ActorKit/tick.h"
#include "ActorKit/triggers.h"

namespace {

using namespace std::chrono_literals;
using ActorKit::Actor;
using ActorKit::Id;
using retropp::Dir;
using retropp::IntRect;
using retropp::Vec2i;
using ScriptKit::wait;
using WorldKit::Place;
using WorldKit::Reach;
using WorldKit::Side;
using WorldKit::Transition;

// ── The world: two layouts ─────────────────────────────────────────────────────────────────────────

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

std::vector<Vec2i> joined(std::vector<std::vector<Vec2i>> parts) {
    std::vector<Vec2i> cells;
    for (const std::vector<Vec2i>& part : parts) {
        cells.insert(cells.end(), part.begin(), part.end());
    }
    return cells;
}

retropp::TileCatalog art() {
    return retropp::TileCatalog{.entries = {
        {.id = 0, .sheet = retropp::AtlasId{7}, .slot = 1, .palette = retropp::PaletteId{4}},
        {.id = 1, .sheet = retropp::AtlasId{7}, .slot = 0, .palette = retropp::PaletteId{4}},
    }};
}

// Layout 0, the keep, four areas. 0, the court, 160 × 128 pixels: an east door (rows 6–9) scrolling into
// 1; its north wall (cells 1–18) joined seamlessly to 2, two cells to the right, so the garden's top-left
// is (0, −96) in the court's pixels; a postern in its west wall (rows 6–9) that leads nowhere; a south
// door (cells 8–11) scrolling into layout 1; stairs at (12, 4) fading into 3. 1, the hall, 240 pixels
// wide, its west door back. 2, the garden, 24 × 12 cells, its south wall back. 3, the cellar, its stairs
// at (5, 4) back up. Layout 1, the moat, one area, the bank, with its north door back.
struct Maps {
    retropp::IndexGrid court = room(20, 16, joined({column(19, 6, 9), row(0, 1, 18), column(0, 6, 9), row(15, 8, 11)}));
    retropp::IndexGrid hall = room(30, 16, column(0, 6, 9));
    retropp::IndexGrid garden = room(24, 12, row(11, 3, 20));
    retropp::IndexGrid cellar = room(12, 10);
    retropp::IndexGrid bank = room(20, 16, row(0, 8, 11));
};

enum Keep : std::uint8_t { Court, Hall, Garden, Cellar };

constexpr Place kCourt{.layout = 0, .area = Keep::Court};
constexpr Place kHall{.layout = 0, .area = Keep::Hall};
constexpr Place kGarden{.layout = 0, .area = Keep::Garden};
constexpr Place kCellar{.layout = 0, .area = Keep::Cellar};
constexpr Place kBank{.layout = 1, .area = 0};

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
// would meet a solid.
class Slider {
public:
    explicit Slider(Vec2i& at) : at_(&at) {}
    void bind(Vec2i& at) { at_ = &at; }

    template <class Solids>
    void walk(const SliderConfig& config, Way way, const Solids& solids) {
        const int dx = (way.right ? 1 : 0) - (way.left ? 1 : 0);
        const int dy = (way.down ? 1 : 0) - (way.up ? 1 : 0);
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

    [[nodiscard]] Dir facing() const { return Dir::Down; }
    [[nodiscard]] SliderShape collision(const SliderConfig& config) const {
        return {{.x = at_->x + config.collision.x, .y = at_->y + config.collision.y,
                 .width = config.collision.width, .height = config.collision.height}};
    }
    [[nodiscard]] const retropp::AnimationFrame& frame(const SliderConfig& config) const { return config.frame; }

private:
    Vec2i* at_;
};

// ── The game's names, input and state ──────────────────────────────────────────────────────────────

struct Input {
    Way way;
};

const ScriptKit::Value<Dir> Heading;
const ScriptKit::Value<int> Count;
const ScriptKit::Value<int> Mine;
const ScriptKit::Command Shout{"shout"};

struct State {
    WorldKit::Scene scene;
    int count = 0;
    int shouts = 0;
};

const ScriptKit::Bindings<State> kBindings{{Count, &State::count}};
const ActorKit::Handlers<State> kHandlers{
    {Shout, [](State& state, Id, const ScriptKit::Arguments&) { ++state.shouts; }},
};

const ActorKit::Look kLook{.size = {.width = 8, .height = 8}, .atlas = retropp::AtlasId{3}, .palette = retropp::PaletteId{1}};

// The marker: driven by the input's `way`, solid, 8 × 8.
Actor marker() {
    return {.slot = "actors", .solid = true, .look = kLook, .mover = ActorKit::movedBy<Slider>(SliderConfig{}, &Input::way)};
}

// A walker driven by its own Heading, solid, 8 × 8.
Actor walker(ActorKit::Scripts scripts = {}) {
    return {.slot = "actors",
            .solid = true,
            .ownValues = {{Heading, Dir::None}, {Mine, 0}},
            .look = kLook,
            .scripts = std::move(scripts),
            .mover = ActorKit::movedBy<Slider>(SliderConfig{}, Heading)};
}

WorldKit::Area area(std::string name, const retropp::IndexGrid& map, std::vector<WorldKit::Opening> openings,
                    std::vector<WorldKit::AreaActor> actors = {}) {
    std::vector<WorldKit::ActorSlot> slots{{.name = "actors", .z = 5}};
    if (name == "court") {
        slots.insert(slots.begin(), {.name = "underfoot", .z = 2});
    }
    return WorldKit::Area{.name = std::move(name),
                          .layers = {{.name = "ground", .tiles = &map, .art = art()}},
                          .actorSlots = std::move(slots),
                          .collision = &map,
                          .openings = std::move(openings),
                          .actors = std::move(actors)};
}

// The keep and the moat, with a guard in the court, a bat in the cellar, a hound roaming the keep from
// the hall, and the marker, which goes anywhere, in the court.
WorldKit::World worldOver(const Maps& maps) {
    return WorldKit::World{
        .layouts = {
            WorldKit::Layout{
                .name = "keep",
                .areas = {
                    area("court", maps.court,
                         {{.name = "east door", .side = Side::East, .span = {6, 9}, .transition = Transition::Scroll,
                           .leadsTo = {.area = Keep::Hall, .opening = "west door"}},
                          {.name = "north wall", .side = Side::North, .span = {1, 18}, .transition = Transition::Seamless,
                           .leadsTo = {.area = Keep::Garden, .opening = "south wall", .offset = {.x = 2, .y = 0}}},
                          {.name = "postern", .side = Side::West, .span = {6, 9}},
                          {.name = "south door", .side = Side::South, .span = {8, 11}, .transition = Transition::Scroll,
                           .leadsTo = {.layout = 1, .area = 0, .opening = "north door"}},
                          {.name = "stairs", .at = {.x = 12, .y = 4, .width = 2, .height = 1},
                           .transition = Transition::Fade, .leadsTo = {.area = Keep::Cellar, .opening = "stairs"}}},
                         {{.name = "guard", .actor = walker(), .at = {24, 24}}}),
                    area("hall", maps.hall,
                         {{.name = "west door", .side = Side::West, .span = {6, 9}, .transition = Transition::Scroll,
                           .leadsTo = {.area = Keep::Court, .opening = "east door"}}}),
                    area("garden", maps.garden,
                         {{.name = "south wall", .side = Side::South, .span = {3, 20}, .transition = Transition::Seamless,
                           .leadsTo = {.area = Keep::Court, .opening = "north wall", .offset = {.x = -2, .y = 0}}}}),
                    area("cellar", maps.cellar,
                         {{.name = "stairs", .at = {.x = 5, .y = 4, .width = 2, .height = 1}, .transition = Transition::Fade,
                           .leadsTo = {.area = Keep::Court, .opening = "stairs"}}},
                         {{.actor = walker(), .at = {16, 16}}}),
                },
                .actors = {{.name = "hound", .actor = walker(), .in = Keep::Hall, .at = {40, 40}}},
                .cellSize = kCell,
                .open = Cell::Open,
                .view = kView,
            },
            WorldKit::Layout{
                .name = "moat",
                .areas = {area("bank", maps.bank,
                               {{.name = "north door", .side = Side::North, .span = {8, 11}, .transition = Transition::Scroll,
                                 .leadsTo = {.layout = 0, .area = Keep::Court, .opening = "south door"}}})},
                .cellSize = kCell,
                .open = Cell::Open,
                .view = kView,
            },
        },
        .actors = {{.name = "marker", .actor = marker(), .in = kCourt, .at = {72, 56}}},
    };
}

std::string thrown(const std::function<void()>& run) {
    try {
        run();
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

std::span<const retropp::Sprite> spritesOf(const retropp::DrawLayer& layer) {
    return std::get<retropp::SpriteContent>(layer.content).sprites;
}

const retropp::DrawLayer* layerKeyed(const LayerKit::Layers& layers, std::string_view key) {
    for (const retropp::DrawLayer& layer : layers) {
        if (std::string_view{layer.key} == key) {
            return &layer;
        }
    }
    return nullptr;
}

constexpr Way kRight{.right = true};
constexpr Way kUp{.up = true};

class Scenes : public ::testing::Test {
protected:
    Maps maps;
    WorldKit::World world = worldOver(maps);
    std::optional<State> state;
    const ActorKit::Config config{.bindings = &kBindings, .handlers = &kHandlers};

    void SetUp() override { retropp::EngineConfig::active.timing = retropp::TimingProfile{.tickPeriodNs = retropp::TickPeriodNs::Hz60}; }

    // Validates the world and builds the scene, the camera handed over in `camera`.
    void start(WorldKit::Camera camera = WorldKit::Camera{kCourt.layout, kCourt.area}) {
        WorldKit::validate(world);
        state.emplace(State{.scene = WorldKit::Scene{world, std::move(camera), config}});
    }

    WorldKit::Scene& scene() { return state->scene; }
    ActorKit::Actors& actors() { return state->scene.actors(); }
    Id markerId() { return scene().actor("marker").id(); }

    WorldKit::Step tick(Way way = {}) { return state->scene.tick(Input{.way = way}, *state); }

    void ticks(int count, Way way = {}) {
        for (int n = 0; n < count; ++n) {
            (void)tick(way);
        }
    }

    // Ticks walking `way` until the camera's Step reports a place entered, and returns that Step; an empty
    // one after 200 ticks.
    WorldKit::Step tickUntilEntered(Way way) {
        for (int n = 0; n < 200; ++n) {
            const WorldKit::Step step = tick(way);
            if (step.entered) {
                return step;
            }
        }
        return {};
    }
};

// ── Building the scene ─────────────────────────────────────────────────────────────────────────────

TEST_F(Scenes, BuildingItSpawnsEveryEntryInTheOrderTheWorldIsWrittenAtItsPlaceAndPosition) {
    start();
    ASSERT_EQ(actors().size(), 4u);
    // The court's guard, the cellar's bat, the keep's hound, the world's marker.
    EXPECT_EQ(scene().actor("guard").id(), 1u);
    EXPECT_EQ(scene().actor("hound").id(), 3u);
    EXPECT_EQ(scene().actor("marker").id(), 4u);
    EXPECT_EQ(actors().place(1), kCourt);
    EXPECT_EQ(actors().position(1), (Vec2i{24, 24}));
    EXPECT_EQ(actors().place(2), kCellar);   // the unnamed bat, second in the order
    EXPECT_EQ(actors().position(2), (Vec2i{16, 16}));
    EXPECT_EQ(actors().place(3), kHall);
    EXPECT_EQ(actors().position(3), (Vec2i{40, 40}));
    EXPECT_EQ(actors().place(4), kCourt);
    EXPECT_EQ(actors().position(4), (Vec2i{72, 56}));
}

const ScriptKit::Script asking{.name = "asking", .on = ActorKit::Enter, .body = {set(Count, Count + 1), Shout()}};
const ScriptKit::Script marking{.name = "marking", .on = ActorKit::Enter, .body = {wait(1s), set(Mine, 1)}};

TEST_F(Scenes, ItFillsInTheConfigsWorldAndTimesWaitsByTheEnginesActiveConfig) {
    world.layouts[0].areas[Keep::Court].actors.push_back({.name = "asker", .actor = walker({&asking})});
    start();
    EXPECT_EQ(scene().config().world, &world);
    ticks(2);   // the script starts and asks; then what it asked is written
    EXPECT_EQ(state->count, 1);
    EXPECT_EQ(state->shouts, 1);

    // With the world alone for a config: an actor whose script uses its own values alone, its wait timed
    // at 60 Hz and then at 50 Hz.
    const auto ticksToMark = [&](retropp::TickPeriodNs period) {
        retropp::EngineConfig::active.timing = retropp::TimingProfile{.tickPeriodNs = period};
        WorldKit::World plain = worldOver(maps);
        plain.layouts[0].areas[Keep::Court].actors = {{.name = "marked", .actor = walker({&marking})}};
        WorldKit::validate(plain);
        struct Plain {
        } nothing;
        WorldKit::Scene own{plain, WorldKit::Camera{kCourt.layout, kCourt.area}};
        const Id id = own.actor("marked").id();
        int count = 0;
        while (own.actors().get(id, Mine) == 0 && count < 200) {
            (void)own.tick(Input{}, nothing);
            ++count;
        }
        return count;
    };
    const int at60 = ticksToMark(retropp::TickPeriodNs::Hz60);
    const int at50 = ticksToMark(retropp::TickPeriodNs::SnesPal);
    EXPECT_EQ(at60 - at50, 10);   // a second is 60 ticks at 60 Hz and 50 at 50 Hz
}

TEST_F(Scenes, FollowTakesAnIdOrANameAndRecordsTheSetsPlayer) {
    start();
    EXPECT_EQ(scene().following(), ActorKit::kNoId);
    scene().follow(markerId());
    EXPECT_EQ(scene().following(), 4u);
    EXPECT_EQ(actors().player(), 4u);
    scene().follow("guard");
    EXPECT_EQ(scene().following(), 1u);
    EXPECT_EQ(actors().player(), 1u);
    actors().remove(1);
    EXPECT_EQ(scene().following(), ActorKit::kNoId);

    EXPECT_NE(thrown([&] { scene().follow(Id{99}); }).find("99"), std::string::npos);
    EXPECT_NE(thrown([&] { scene().follow("nobody"); }).find("'nobody'"), std::string::npos);
    EXPECT_THROW(scene().follow("guard"), std::out_of_range);   // removed: its name is no one's
}

// ── The tick and the followed actor ────────────────────────────────────────────────────────────────

struct Plain {};

TEST_F(Scenes, TheFollowedActorGoesThroughTheEastDoorExactlyAsTheCamerasOwnCallsTakeItByHand) {
    start();
    scene().follow("marker");
    const Id id = markerId();

    // The same marker taken through by hand: ActorKit's tick, then the camera's follow and moveTo.
    ActorKit::Actors hand{retropp::EngineConfig::active};
    const Id mine = hand.spawn(marker(), kCourt, {72, 56});
    hand.player(mine);
    WorldKit::Camera camera{kCourt.layout, kCourt.area};
    const ActorKit::Config bare{.world = &world};
    Plain plain;

    (void)tick();
    const std::string own = actors().key(id);
    ActorKit::tick(hand, bare, camera, Input{}, plain);
    (void)camera.follow(world, hand.box(mine));

    bool entered = false;
    for (int n = 0; n < 140; ++n) {
        const WorldKit::Step step = tick(kRight);
        ActorKit::tick(hand, bare, camera, Input{.way = kRight}, plain);
        const WorldKit::Step byHand = camera.follow(world, hand.box(mine));
        const Vec2i at = hand.position(mine);
        hand.moveTo(mine, byHand.entered.value_or(hand.place(mine)), {.x = at.x + byHand.move.x, .y = at.y + byHand.move.y});

        ASSERT_EQ(step.move, byHand.move) << "tick " << n;
        ASSERT_EQ(step.entered, byHand.entered) << "tick " << n;
        ASSERT_EQ(actors().place(id), hand.place(mine)) << "tick " << n;
        ASSERT_EQ(actors().position(id), hand.position(mine)) << "tick " << n;
        ASSERT_EQ(scene().camera().position(), camera.position()) << "tick " << n;
        if (step.entered) {
            entered = true;
            EXPECT_EQ(*step.entered, kHall);
            EXPECT_EQ(actors().position(id), (Vec2i{12, 56}));   // the carry, 16, in from the hall's west wall
            EXPECT_NE(actors().key(id), own);                   // shown fresh on the tick it enters
            (void)tick(kRight);
            EXPECT_EQ(actors().key(id), own);                   // and under its own key on the next
            break;
        }
    }
    EXPECT_TRUE(entered);
}

TEST_F(Scenes, TheFollowedActorOnTheStairsIsInTheCellarOnTheFadesSwitchCenteredOnItsStairs) {
    world.actors[0].at = {100, 50};
    start();
    scene().follow("marker");
    const WorldKit::Step step = tickUntilEntered(kUp);
    ASSERT_TRUE(step.entered.has_value());
    EXPECT_EQ(*step.entered, kCellar);
    EXPECT_EQ(actors().place(markerId()), kCellar);
    EXPECT_EQ(actors().position(markerId()), (Vec2i{44, 32}));   // centered on the cellar's stairs, cells 5–6 of row 4
    EXPECT_EQ(scene().camera().area(), Keep::Cellar);
}

TEST_F(Scenes, TheFollowedActorAcrossTheSeamIsInTheGardenOnTheCrossingTickAndNoOtherActorMoves) {
    world.actors[0].at = {70, 10};
    start();
    scene().follow("marker");
    const Id guard = scene().actor("guard").id();
    const WorldKit::Step step = tickUntilEntered(kUp);
    ASSERT_TRUE(step.entered.has_value());
    EXPECT_EQ(*step.entered, kGarden);
    EXPECT_EQ(actors().place(markerId()), kGarden);
    EXPECT_EQ(actors().position(markerId()), (Vec2i{70, 91}));   // from y −5 in the court's pixels, 96 down
    EXPECT_EQ(actors().position(guard), (Vec2i{24, 24}));
    EXPECT_EQ(actors().place(guard), kCourt);
}

TEST_F(Scenes, TheFollowedActorMovedByHandIntoAnotherAreaHasTheCameraThereOnTheNextTickShownFresh) {
    start();
    scene().follow("marker");
    (void)tick();
    const Id id = markerId();
    const std::string own = actors().key(id);
    actors().moveTo(id, kHall, {120, 56});
    (void)tick(kRight);
    EXPECT_EQ(scene().camera().layout(), kHall.layout);
    EXPECT_EQ(scene().camera().area(), kHall.area);
    WorldKit::Camera placed{kHall.layout, kHall.area};
    placed.place(world, IntRect{.x = 120, .y = 56, .width = 8, .height = 8});
    EXPECT_EQ(scene().camera().position().y, placed.position().y);
    EXPECT_EQ(actors().position(id), (Vec2i{121, 56}));
    EXPECT_NE(actors().key(id), own);   // reported fresh: it walked on that tick and took a throwaway key
}

TEST_F(Scenes, WithNothingFollowedTheCameraStaysAndAScrollAlreadyRunningRunsToItsEnd) {
    start();
    const Id id = markerId();
    ticks(10, kRight);
    EXPECT_EQ(actors().position(id), (Vec2i{82, 56}));
    EXPECT_EQ(scene().camera().position(), (Vec2i{0, 0}));

    actors().moveTo(id, kCourt, {150, 56});
    scene().follow(id);
    int walked = 0;
    while (!scene().camera().transitioning() && walked < 20) {
        (void)tick(kRight);
        ++walked;
    }
    ASSERT_TRUE(scene().camera().transitioning());
    actors().remove(id);
    int left = 0;
    while (scene().camera().transitioning() && left < 40) {
        (void)tick();
        ++left;
    }
    EXPECT_FALSE(scene().camera().transitioning());
    EXPECT_EQ(left, 32);   // the layout's scrollTicks
    EXPECT_EQ(scene().camera().area(), Keep::Hall);
    EXPECT_EQ(scene().following(), ActorKit::kNoId);
}

TEST_F(Scenes, ACameraFollowedByHandWithNothingFollowedScrollsAtItsOwnPace) {
    start();
    IntRect focus{.x = 150, .y = 64, .width = 8, .height = 8};   // a point of the game's own the camera looks at
    int ticksScrolling = 0;
    bool started = false;
    for (int n = 0; n < 80; ++n) {
        (void)tick();
        if (!scene().camera().transitioning()) {
            ++focus.x;
        }
        const WorldKit::Step step = scene().camera().follow(world, focus);   // the game's own follow
        focus.x += step.move.x;
        focus.y += step.move.y;
        started = started || scene().camera().transitioning();
        if (started) {
            ++ticksScrolling;
        }
        if (step.entered) {
            break;
        }
    }
    EXPECT_EQ(ticksScrolling, 33);   // the tick it starts, then the layout's 32 scrollTicks
    EXPECT_EQ(scene().camera().area(), Keep::Hall);
}

TEST_F(Scenes, TheCameraPlacedByHandStaysWhereItIsPutAndTheNextTicksCarryOnFromThere) {
    world.actors[0].in = kHall;
    world.actors[0].at = {120, 56};
    start(WorldKit::Camera{kHall.layout, kHall.area});
    scene().follow("marker");
    (void)tick();
    EXPECT_EQ(scene().camera().position().x, 44);   // the law: the marker's center, 124, in the middle

    const Id id = markerId();
    actors().moveTo(id, kHall, {8, 56});
    scene().camera().place(world, actors().box(id));
    EXPECT_EQ(scene().camera().position().x, 0);
    (void)tick();
    EXPECT_EQ(scene().camera().position().x, 0);
    ticks(80, kRight);   // its center 92, past the middle of the view by 12
    EXPECT_EQ(actors().position(id), (Vec2i{88, 56}));
    EXPECT_EQ(scene().camera().position().x, 12);
}

// ── The frame ──────────────────────────────────────────────────────────────────────────────────────

TEST_F(Scenes, LayersAreTheWorldsAndOneSpriteLayerPerAreaOnScreenAndSlot) {
    world.actors[0].at = {72, 80};   // low enough that the camera leaves the garden out of view
    start();
    scene().follow("marker");
    (void)tick();

    const LayerKit::Layers alone = scene().layers(*state);
    ASSERT_EQ(alone.size(), 3u);
    EXPECT_EQ(std::string_view{alone[0].key}, "area:0:0:ground");
    const std::vector<WorldKit::Shown> shown = scene().camera().shown(world);
    const retropp::DrawLayer* underfoot = layerKeyed(alone, WorldKit::SceneLayerKey(0, Keep::Court, "underfoot"));
    const retropp::DrawLayer* courtActors = layerKeyed(alone, "scene:0:0:actors");
    ASSERT_NE(underfoot, nullptr);
    ASSERT_NE(courtActors, nullptr);
    EXPECT_EQ(underfoot->z, 2);
    EXPECT_TRUE(spritesOf(*underfoot).empty());   // a slot with no sprite still has its layer
    EXPECT_EQ(courtActors->z, WorldKit::ActorZ(world, shown[0], "actors"));
    EXPECT_EQ(courtActors->z, 5);
    EXPECT_EQ(courtActors->scroll, shown[0].scroll);
    ASSERT_EQ(courtActors->regions.size(), 1u);
    EXPECT_EQ(courtActors->regions[0].effects[0].stencil, retropp::StencilMode::TransparentOutside);
    ASSERT_EQ(spritesOf(*courtActors).size(), 2u);   // the guard and the marker
    EXPECT_EQ(spritesOf(*courtActors)[1].key.value, actors().key(markerId()));
    EXPECT_TRUE(scene().fade().empty());

    // During the scroll into the hall: the hall's actors too, its hound among them.
    actors().moveTo(markerId(), kCourt, {150, 64});
    while (!scene().camera().transitioning()) {
        (void)tick(kRight);
    }
    const LayerKit::Layers scrolling = scene().layers(*state);
    EXPECT_EQ(scrolling.size(), 5u);   // two grounds, the court's two slots, the hall's one
    const retropp::DrawLayer* hallActors = layerKeyed(scrolling, "scene:0:1:actors");
    ASSERT_NE(hallActors, nullptr);
    const std::vector<WorldKit::Shown> both = scene().camera().shown(world);
    ASSERT_EQ(both.size(), 2u);
    EXPECT_EQ(hallActors->z, WorldKit::ActorZ(world, both[1], "actors"));
    EXPECT_EQ(hallActors->scroll, both[1].scroll);
    ASSERT_EQ(spritesOf(*hallActors).size(), 1u);
    EXPECT_EQ(spritesOf(*hallActors)[0].key.value, actors().key(scene().actor("hound").id()));
}

TEST_F(Scenes, LayersAcrossTheSeamHoldTheGardensAndFadeIsOneRegionDuringAFade) {
    world.actors[0].at = {100, 10};
    start();
    scene().follow("marker");
    (void)tick();
    const LayerKit::Layers layers = scene().layers(*state);
    const retropp::DrawLayer* gardenActors = layerKeyed(layers, "scene:0:2:actors");
    ASSERT_NE(gardenActors, nullptr);
    EXPECT_TRUE(spritesOf(*gardenActors).empty());

    actors().moveTo(markerId(), kCourt, {100, 50});
    int walked = 0;
    while (!scene().camera().transitioning() && walked < 40) {   // up onto the stairs
        (void)tick(kUp);
        ++walked;
    }
    ASSERT_TRUE(scene().camera().transitioning());
    (void)tick();
    const LayerKit::Regions fade = scene().fade();
    ASSERT_EQ(fade.size(), 1u);
    EXPECT_EQ(std::string_view{fade[0].key}, "world:fade");
}

TEST_F(Scenes, DressIsHandedEachAreaOnScreenAndSlotAndWhatItReturnsIsDrawn) {
    world.actors[0].at = {72, 80};   // the court alone on screen
    start();
    scene().follow("marker");
    (void)tick();
    std::vector<std::pair<Place, std::string>> calls;
    const LayerKit::Layers layers =
        scene().layers(*state, [&](Place place, std::string_view slot, LayerKit::Sprites sprites) {
            calls.emplace_back(place, std::string{slot});
            if (slot == "underfoot") {
                sprites.push_back(retropp::Sprite{.key = "puddle", .x = 30, .y = 40});
            } else {
                for (retropp::Sprite& sprite : sprites) {
                    sprite.alpha = 0.5f;
                }
            }
            return sprites;
        });
    ASSERT_EQ(calls.size(), 2u);
    EXPECT_EQ(calls[0], (std::pair<Place, std::string>{kCourt, "underfoot"}));
    EXPECT_EQ(calls[1], (std::pair<Place, std::string>{kCourt, "actors"}));
    const std::span<const retropp::Sprite> underfoot = spritesOf(*layerKeyed(layers, "scene:0:0:underfoot"));
    ASSERT_EQ(underfoot.size(), 1u);
    EXPECT_EQ(underfoot[0].key.value, "puddle");
    const std::span<const retropp::Sprite> standing = spritesOf(*layerKeyed(layers, "scene:0:0:actors"));
    ASSERT_EQ(standing.size(), 2u);
    EXPECT_EQ(standing[0].alpha, 0.5f);
    EXPECT_EQ(standing[1].alpha, 0.5f);
}

// ── Copies ─────────────────────────────────────────────────────────────────────────────────────────

TEST_F(Scenes, ACopiedSceneRunsOnByItselfAndTheOriginalStays) {
    start();
    scene().follow("marker");
    (void)tick();
    const Id id = markerId();
    const Vec2i camera = scene().camera().position();

    State other{.scene = scene()};
    for (int n = 0; n < 10; ++n) {
        (void)other.scene.tick(Input{.way = kUp}, other);
    }
    EXPECT_EQ(other.scene.following(), id);
    EXPECT_EQ(other.scene.actors().position(id), (Vec2i{72, 46}));
    EXPECT_EQ(other.scene.camera().position().y, camera.y - 10);
    EXPECT_EQ(actors().position(id), (Vec2i{72, 56}));
    EXPECT_EQ(scene().camera().position(), camera);
}

// ── Spawning and the throws ────────────────────────────────────────────────────────────────────────

const Actor kSpawnedByScript = walker();
const ScriptKit::Script spawning{.name = "spawning", .on = ActorKit::Enter, .body = {ActorKit::spawn(kSpawnedByScript, {8, 0})}};

TEST_F(Scenes, SpawnTakesTheReachOfItsEntrysLevelAndActorsSpawnedOtherwiseTakeTheDefaults) {
    world.layouts[0].areas[Keep::Court].actors.push_back({.actor = walker({&spawning}), .at = {100, 100}});
    start();
    const Id inHall = scene().spawn(WorldKit::AreaActor{.actor = walker(), .at = {60, 60}}, kHall);
    const Id inCellar = scene().spawn(WorldKit::LayoutActor{.actor = walker(), .in = Keep::Cellar, .at = {30, 30}}, 0);
    const Id anywhere = scene().spawn(WorldKit::WorldActor{.name = "wanderer", .actor = walker(), .in = kBank, .at = {50, 50}});
    EXPECT_EQ(scene().actor(inHall).reach(), Reach::Area);
    EXPECT_EQ(actors().place(inHall), kHall);
    EXPECT_EQ(scene().actor(inCellar).reach(), Reach::Layout);
    EXPECT_EQ(actors().place(inCellar), kCellar);
    EXPECT_EQ(scene().actor("wanderer").reach(), Reach::World);
    EXPECT_EQ(scene().actor("wanderer").id(), anywhere);
    EXPECT_EQ(actors().place(anywhere), kBank);

    EXPECT_THROW((void)scene().spawn(WorldKit::AreaActor{.name = "guard", .actor = walker()}, kHall), std::invalid_argument);
    actors().remove(scene().actor("guard").id());
    const Id second = scene().spawn(WorldKit::AreaActor{.name = "guard", .actor = walker()}, kHall);
    EXPECT_EQ(scene().actor("guard").id(), second);

    const Id byHand = actors().spawn(walker(), kHall, {80, 80});
    const std::size_t before = actors().size();
    ticks(2);   // the spawner's script spawns on its first tick, and its actor takes part from the next
    ASSERT_EQ(actors().size(), before + 1);
    const Id byScript = actors().ids().back();
    for (const Id id : {byHand, byScript}) {
        EXPECT_EQ(scene().actor(id).reach(), Reach::Area);
        EXPECT_EQ(scene().actor(id).atEdge(), WorldKit::AtEdge::Stop);
        EXPECT_EQ(scene().actor(id).awakeWhen(), WorldKit::AwakeWhen::Always);
        EXPECT_EQ(actors().confinement(id), ActorKit::Confinement::Area);
    }
}

TEST_F(Scenes, ValidateRejectsEveryWrongEntryAndTheSceneRejectsAnEntryOfAnotherType) {
    const auto rejected = [&](const std::function<void(WorldKit::World&)>& change) {
        WorldKit::World changed = worldOver(maps);
        change(changed);
        return thrown([&] { WorldKit::validate(changed); });
    };
    EXPECT_EQ(rejected([](WorldKit::World& w) { w.layouts[0].areas[Keep::Court].actors.push_back({.name = "empty"}); }),
              "WorldKit: layout 0 'keep' area 0 'court' actor 1 'empty' has no actor");
    EXPECT_EQ(rejected([](WorldKit::World& w) { w.actors.push_back({.name = "guard", .actor = walker(), .in = kHall}); }),
              "WorldKit: actor 1 'guard' is named twice");
    EXPECT_EQ(rejected([](WorldKit::World& w) { w.layouts[0].actors.push_back({.actor = walker(), .in = 9}); }),
              "WorldKit: layout 0 'keep' actor 1 '' is in area 9, which the layout lacks");
    EXPECT_EQ(rejected([](WorldKit::World& w) { w.actors.push_back({.actor = walker(), .in = {.layout = 5, .area = 0}}); }),
              "WorldKit: actor 1 '' is in layout 5 area 0, which the world lacks");
    EXPECT_EQ(rejected([](WorldKit::World& w) {
                  w.layouts[1].areas[0].actors = {{.actor = walker()}, {.actor = walker()}};   // two unnamed
              }),
              "");

    world.layouts[0].areas[Keep::Hall].actors.push_back({.name = "number", .actor = 7});
    WorldKit::validate(world);
    EXPECT_EQ(thrown([&] { start(); }), "WorldKit::Scene: actor 'number' is not an ActorKit::Actor");
    EXPECT_THROW(start(), std::logic_error);
}

TEST_F(Scenes, AnActorTheSceneLacksThrowsOutOfRangeNamingTheIdOrTheName) {
    start();
    EXPECT_NE(thrown([&] { (void)scene().actor(Id{99}); }).find("99"), std::string::npos);
    EXPECT_NE(thrown([&] { (void)scene().actor("nobody"); }).find("'nobody'"), std::string::npos);
    WorldKit::SceneActor guard = scene().actor("guard");
    actors().remove(guard.id());
    EXPECT_NE(thrown([&] { (void)guard.reach(); }).find(std::to_string(guard.id())), std::string::npos);
    EXPECT_THROW((void)scene().actor(guard.id()), std::out_of_range);
}

}  // namespace
