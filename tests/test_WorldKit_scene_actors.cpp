// WorldKit's scene, actor by actor: the reach an actor is written with and its confinement; Stop at the
// edge of its reach; carried through an opening inside its reach by the opening's transition; Despawn and
// Hide at the edge; awake or frozen by its rule, by a freeze by hand, from a handler; the hound promoted
// from a handler; a handle on a removed actor. The world is built in code, the actors are moved by a mover
// of the test's own, and the tests run headless.

#include <gtest/gtest.h>

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

#include "ActorKit/actor.h"
#include "ActorKit/actors.h"
#include "ActorKit/commands.h"
#include "ActorKit/config.h"
#include "ActorKit/look.h"
#include "ActorKit/mover.h"
#include "ActorKit/triggers.h"
#include "ActorKit/values.h"

namespace {

using ActorKit::Actor;
using ActorKit::Confinement;
using ActorKit::Id;
using retropp::Dir;
using retropp::IntRect;
using retropp::Vec2i;
using ScriptKit::wait;
using ScriptKit::waitWhile;
using WorldKit::AtEdge;
using WorldKit::AwakeWhen;
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
const ScriptKit::Value<int> Mine;
const ScriptKit::Value<bool> Pressed;
const ScriptKit::Command Halt{"halt"};
const ScriptKit::Command GoAnywhere{"go anywhere"};

struct State {
    WorldKit::Scene scene;
};

const ActorKit::Handlers<State> kHandlers{
    {Halt, [](State& state, Id asker, const ScriptKit::Arguments&) { state.scene.actor(asker).freeze(); }},
    {GoAnywhere, [](State& state, Id asker, const ScriptKit::Arguments&) {
         state.scene.actor(asker).reach(WorldKit::Reach::World);
     }},
};

const ActorKit::Look kLook{.size = {.width = 8, .height = 8}, .atlas = retropp::AtlasId{3}, .palette = retropp::PaletteId{1}};

// The marker: driven by the input's `way`, solid, 8 × 8.
Actor marker() {
    return {.slot = "actors", .solid = true, .look = kLook, .mover = ActorKit::movedBy<Slider>(SliderConfig{}, &Input::way)};
}

// A walker driven by its own Heading, starting at `heading`, solid, 8 × 8, setting off touch.
Actor walker(Dir heading = Dir::None, ActorKit::Scripts scripts = {}) {
    return {.slot = "actors",
            .solid = true,
            .ownValues = {{Heading, heading}, {Mine, 0}},
            .look = kLook,
            .scripts = std::move(scripts),
            .mover = ActorKit::movedBy<Slider>(SliderConfig{}, Heading),
            .setsOff = {ActorKit::On::Touch}};
}

WorldKit::Area area(std::string name, const retropp::IndexGrid& map, std::vector<WorldKit::Opening> openings,
                    std::vector<WorldKit::AreaActor> actors = {}) {
    return WorldKit::Area{.name = std::move(name),
                          .layers = {{.name = "ground", .tiles = &map, .art = art()}},
                          .actorSlots = {{.name = "actors", .z = 5}},
                          .collision = &map,
                          .openings = std::move(openings),
                          .actors = std::move(actors)};
}

// The keep and the moat, with a guard in the court, a hound roaming the keep from the hall, and the marker,
// which goes anywhere, in the court.
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
                           .leadsTo = {.area = Keep::Court, .opening = "stairs"}}}),
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

// Whether any sprite of `layers` carries `key`.
bool drawn(const LayerKit::Layers& layers, const std::string& key) {
    for (const retropp::DrawLayer& layer : layers) {
        if (const auto* content = std::get_if<retropp::SpriteContent>(&layer.content)) {
            for (const retropp::Sprite& sprite : content->sprites) {
                if (sprite.key.value == key) {
                    return true;
                }
            }
        }
    }
    return false;
}

class SceneActors : public ::testing::Test {
protected:
    Maps maps;
    WorldKit::World world = worldOver(maps);
    std::optional<State> state;
    const ActorKit::Config config{.handlers = &kHandlers};

    void SetUp() override {
        retropp::EngineConfig::active.timing = retropp::TimingProfile{.tickPeriodNs = retropp::TickPeriodNs::Hz60};
        WorldKit::validate(world);
        state.emplace(State{.scene = WorldKit::Scene{world, WorldKit::Camera{kCourt.layout, kCourt.area}, config}});
    }

    WorldKit::Scene& scene() { return state->scene; }
    ActorKit::Actors& actors() { return state->scene.actors(); }

    void tick(Way way = {}) { (void)state->scene.tick(Input{.way = way}, *state); }

    void ticks(int count, Way way = {}) {
        for (int n = 0; n < count; ++n) {
            tick(way);
        }
    }

    // A walker headed `heading`, spawned in `place` at `at` with the reach of `level` and `atEdge`.
    Id spawnAt(Reach level, Place place, Vec2i at, Dir heading, AtEdge atEdge = AtEdge::Stop,
               ActorKit::Scripts scripts = {}) {
        const Actor actor = walker(heading, std::move(scripts));
        switch (level) {
            case Reach::Area:
                return scene().spawn(WorldKit::AreaActor{.actor = actor, .at = at, .atEdge = atEdge}, place);
            case Reach::Layout:
                return scene().spawn(WorldKit::LayoutActor{.actor = actor, .in = place.area, .at = at, .atEdge = atEdge},
                                     place.layout);
            case Reach::World:
            default: return scene().spawn(WorldKit::WorldActor{.actor = actor, .in = place, .at = at, .atEdge = atEdge});
        }
    }

    // Ticks until the actor's place changes or it is gone, and returns how many ticks that took; 0 after 200.
    int ticksUntilItLeaves(Id id) {
        const Place from = actors().place(id);
        for (int n = 1; n <= 200; ++n) {
            tick();
            if (!actors().has(id) || actors().place(id) != from) {
                return n;
            }
        }
        return 0;
    }
};

// ── Reach and confinement ──────────────────────────────────────────────────────────────────────────

TEST_F(SceneActors, EachActorHasTheReachOfTheLevelItIsWrittenAtAndTheConfinementThatGoesWithIt) {
    const Id guard = scene().actor("guard").id();
    const Id hound = scene().actor("hound").id();
    const Id marker = scene().actor("marker").id();
    EXPECT_EQ(scene().actor(guard).reach(), Reach::Area);
    EXPECT_EQ(actors().confinement(guard), Confinement::Area);
    EXPECT_EQ(scene().actor(hound).reach(), Reach::Layout);
    EXPECT_EQ(actors().confinement(hound), Confinement::Layout);
    EXPECT_EQ(scene().actor(marker).reach(), Reach::World);
    EXPECT_EQ(actors().confinement(marker), Confinement::None);
    EXPECT_EQ(scene().actor(guard).atEdge(), AtEdge::Stop);
    EXPECT_EQ(scene().actor(guard).awakeWhen(), AwakeWhen::Always);

    // | atEdge \ reach | Area | Layout | World |, row by row.
    const auto confinementOf = [&](Reach reach, AtEdge atEdge) {
        scene().actor(guard).reach(reach);
        scene().actor(guard).atEdge(atEdge);
        return actors().confinement(guard);
    };
    EXPECT_EQ(confinementOf(Reach::Area, AtEdge::Stop), Confinement::Area);
    EXPECT_EQ(confinementOf(Reach::Layout, AtEdge::Stop), Confinement::Layout);
    EXPECT_EQ(confinementOf(Reach::World, AtEdge::Stop), Confinement::None);
    for (const AtEdge atEdge : {AtEdge::Despawn, AtEdge::Hide}) {
        for (const Reach reach : {Reach::Area, Reach::Layout, Reach::World}) {
            EXPECT_EQ(confinementOf(reach, atEdge), Confinement::None);
        }
    }
    EXPECT_EQ(scene().actor(guard).reach(), Reach::World);
    EXPECT_EQ(scene().actor(guard).atEdge(), AtEdge::Hide);
}

TEST_F(SceneActors, StopHoldsEachActorAtTheEdgeOfItsReach) {
    const Id inArea = spawnAt(Reach::Area, kCourt, {150, 64}, Dir::Right);
    const Id inLayout = spawnAt(Reach::Layout, kCourt, {150, 72}, Dir::Right);
    ticks(40);
    EXPECT_EQ(actors().place(inArea), kCourt);   // held at the east door
    EXPECT_EQ(actors().position(inArea), (Vec2i{152, 64}));
    EXPECT_EQ(actors().place(inLayout), kHall);   // through it, within the keep

    const Id layoutSouth = spawnAt(Reach::Layout, kCourt, {66, 110}, Dir::Down);
    const Id worldSouth = spawnAt(Reach::World, kCourt, {80, 110}, Dir::Down);
    ticks(40);
    EXPECT_EQ(actors().place(layoutSouth), kCourt);   // held at the door into the moat
    EXPECT_EQ(actors().position(layoutSouth), (Vec2i{66, 120}));
    EXPECT_EQ(actors().place(worldSouth), kBank);     // through it
}

// ── Carried inside its reach ───────────────────────────────────────────────────────────────────────

TEST_F(SceneActors, AnActorInsideItsReachGoesOnByTheOpeningsTransitionOnTheTickItCrosses) {
    // The east door: into the hall's pixels where it crossed, in the doorway, under its own key; then it
    // walks in.
    const Id door = spawnAt(Reach::World, kCourt, {150, 60}, Dir::Right);
    tick();
    const std::string doorKey = actors().key(door);
    EXPECT_EQ(ticksUntilItLeaves(door), 5);   // its center at 160 on the fifth tick more
    EXPECT_EQ(actors().place(door), kHall);
    EXPECT_EQ(actors().position(door), (Vec2i{-4, 60}));   // from 156, 160 back: half in the doorway
    EXPECT_EQ(actors().key(door), doorKey);
    EXPECT_FALSE(scene().camera().transitioning());
    tick();
    EXPECT_EQ(actors().position(door), (Vec2i{-3, 60}));   // a pixel a tick, in through the doorway
    EXPECT_EQ(actors().place(door), kHall);

    // The seam: into the garden's pixels on the crossing tick, under its own key.
    const Id seam = spawnAt(Reach::Layout, kCourt, {110, 10}, Dir::Up);
    tick();
    const std::string seamKey = actors().key(seam);
    EXPECT_GT(ticksUntilItLeaves(seam), 0);
    EXPECT_EQ(actors().place(seam), kGarden);
    EXPECT_EQ(actors().position(seam), (Vec2i{110, 91}));   // from y −5, 96 down
    EXPECT_EQ(actors().key(seam), seamKey);

    // The stairs: centered on the cellar's stairs, the camera staying in the court, shown fresh.
    const Id stairs = spawnAt(Reach::Layout, kCourt, {100, 50}, Dir::Up);
    tick();
    const std::string stairsKey = actors().key(stairs);
    EXPECT_EQ(ticksUntilItLeaves(stairs), 14);   // its center at 39, inside the stairs
    EXPECT_EQ(actors().place(stairs), kCellar);
    EXPECT_EQ(actors().position(stairs), (Vec2i{44, 32}));
    EXPECT_NE(actors().key(stairs), stairsKey);
    EXPECT_FALSE(scene().camera().transitioning());
    EXPECT_EQ(scene().camera().area(), Keep::Court);
}

// ── Despawn ────────────────────────────────────────────────────────────────────────────────────────

TEST_F(SceneActors, DespawnRemovesAnActorAsItGoesOutOfItsReachAndNeverTheFollowedOne) {
    const Id door = spawnAt(Reach::Area, kCourt, {150, 60}, Dir::Right, AtEdge::Despawn);
    EXPECT_EQ(actors().confinement(door), Confinement::None);
    ticks(5);
    EXPECT_TRUE(actors().has(door));
    tick();   // its center comes to 160
    EXPECT_FALSE(actors().has(door));

    const Id stairs = spawnAt(Reach::Area, kCourt, {100, 50}, Dir::Up, AtEdge::Despawn);
    ticks(14);
    EXPECT_TRUE(actors().has(stairs));
    tick();   // its center comes to 39, on the stairs
    EXPECT_FALSE(actors().has(stairs));

    const Id roamer = spawnAt(Reach::Layout, kCourt, {150, 64}, Dir::Right, AtEdge::Despawn);
    ticks(20);
    EXPECT_EQ(actors().place(roamer), kHall);   // the hall is inside its reach
    actors().moveTo(roamer, kCourt, {66, 110});
    actors().set(roamer, Heading, Dir::Down);
    ticks(20);
    EXPECT_FALSE(actors().has(roamer));   // the moat is not

    const Id marker = scene().actor("marker").id();
    scene().actor(marker).reach(Reach::Area);
    scene().actor(marker).atEdge(AtEdge::Despawn);
    scene().follow(marker);
    for (int n = 0; n < 140; ++n) {
        (void)scene().tick(Input{.way = {.right = true}}, *state);
    }
    ASSERT_TRUE(actors().has(marker));
    EXPECT_EQ(actors().place(marker), kHall);   // carried by the camera, as the player is
}

// ── Hide ───────────────────────────────────────────────────────────────────────────────────────────

const ScriptKit::Script counting{.name = "counting", .on = ActorKit::Enter, .body = {wait(60), set(Mine, 1)}};
const ScriptKit::Script press{.name = "press", .on = ActorKit::Touch,
                              .body = {set(Pressed, true), waitWhile(ActorKit::Touched), set(Pressed, false)}};

TEST_F(SceneActors, HideTakesAnActorOutOfTheWorldUntilTheGamePutsItBackAndItCarriesOnFromThere) {
    const Id hidden = spawnAt(Reach::Area, kCourt, {146, 56}, Dir::Right, AtEdge::Hide, {&counting});
    const Id twin = spawnAt(Reach::Area, kCourt, {40, 100}, Dir::None, AtEdge::Stop, {&counting});
    const Id plate = scene().spawn(
        WorldKit::AreaActor{.actor = Actor{.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 8},
                                           .ownValues = {{Pressed, false}}, .look = kLook, .scripts = {&press}},
                            .at = {152, 56}},
        kCourt);
    const Id passer = spawnAt(Reach::World, kCourt, {126, 56}, Dir::Right);

    int n = 0;
    int twinMarked = 0;
    int hiddenAt = 0;
    while (!scene().actor(hidden).hidden() && n < 40) {
        tick();
        ++n;
        if (n == 5) {
            EXPECT_TRUE(actors().get(plate, Pressed));   // the hidden-to-be stands on the plate
        }
    }
    ASSERT_TRUE(scene().actor(hidden).hidden());
    hiddenAt = n;
    EXPECT_EQ(hiddenAt, 10);   // from 146 to 156, its center at 160
    EXPECT_TRUE(scene().actor(hidden).frozen());
    EXPECT_FALSE(scene().actor(hidden).awake());
    EXPECT_TRUE(actors().has(hidden));
    EXPECT_FALSE(drawn(scene().layers(*state), actors().key(hidden)));

    while (n < 100) {
        tick();
        ++n;
        if (twinMarked == 0 && actors().get(twin, Mine) == 1) {
            twinMarked = n;
        }
    }
    EXPECT_FALSE(actors().get(plate, Pressed));     // nothing touches the plate now
    EXPECT_EQ(actors().place(passer), kHall);        // the passer walked on through where it stood
    EXPECT_EQ(actors().get(hidden, Mine), 0);        // its wait held

    const int putBackAfter = n;
    const std::string own = actors().key(hidden);
    actors().moveTo(hidden, kCourt, {40, 40});
    tick();
    ++n;
    EXPECT_FALSE(scene().actor(hidden).hidden());
    EXPECT_TRUE(scene().actor(hidden).awake());
    EXPECT_TRUE(drawn(scene().layers(*state), actors().key(hidden)));
    EXPECT_NE(actors().key(hidden), own);   // shown fresh: it walked on that tick and took a throwaway key
    while (actors().get(hidden, Mine) == 0 && n < 300) {
        tick();
        ++n;
    }
    // Frozen from the tick after it was hidden to the tick it was put back: its wait was that much later.
    EXPECT_EQ(n, twinMarked + (putBackAfter - hiddenAt));
}

// ── Awake or frozen ────────────────────────────────────────────────────────────────────────────────

TEST_F(SceneActors, AwakeWhenAlwaysInLayoutAndOnScreenWakeAndFreezeByTheAreasOnScreen) {
    const Id always = spawnAt(Reach::Area, kBank, {40, 40}, Dir::Right);
    const Id inLayout = spawnAt(Reach::Area, kBank, {40, 60}, Dir::Right);
    scene().actor(inLayout).awakeWhen(AwakeWhen::InLayout);
    const Id inGarden = spawnAt(Reach::Area, kGarden, {40, 40}, Dir::Right);
    scene().actor(inGarden).awakeWhen(AwakeWhen::OnScreen);
    scene().follow("marker");   // at (72, 56), the camera at y −4: the garden in view across the seam
    ticks(5);
    EXPECT_EQ(actors().position(always), (Vec2i{45, 40}));     // walks with the camera in the court
    EXPECT_EQ(actors().position(inLayout), (Vec2i{40, 60}));   // stands: no area of the moat on screen
    EXPECT_TRUE(scene().actor(inLayout).frozen());
    EXPECT_GT(actors().position(inGarden).x, 40);               // on screen, beyond the seam

    // The camera in the bank: the moat on screen.
    scene().follow(ActorKit::kNoId);
    scene().camera() = WorldKit::Camera{kBank.layout, kBank.area};
    scene().camera().place(world, actors().box(inLayout));
    ticks(5);
    EXPECT_EQ(actors().position(inLayout), (Vec2i{45, 60}));
    EXPECT_FALSE(scene().actor(inGarden).awake());   // the garden is off screen now

    // A change of rule holds from the next tick.
    scene().actor(always).awakeWhen(AwakeWhen::OnScreen);
    EXPECT_FALSE(scene().actor(always).frozen());
    scene().camera() = WorldKit::Camera{kCourt.layout, kCourt.area};
    tick();
    EXPECT_TRUE(scene().actor(always).frozen());
    const Vec2i held = actors().position(always);
    ticks(5);
    EXPECT_EQ(actors().position(always), held);
}

TEST_F(SceneActors, AnOnScreenActorWakesWhenAScrollBringsItsAreaOnScreenAndWalksOnceTheScrollEnds) {
    const Id inHall = spawnAt(Reach::Area, kHall, {100, 100}, Dir::Right);
    scene().actor(inHall).awakeWhen(AwakeWhen::OnScreen);
    const Id marker = scene().actor("marker").id();
    actors().moveTo(marker, kCourt, {150, 64});
    scene().follow(marker);
    while (!scene().camera().transitioning()) {
        tick(Way{.right = true});
        EXPECT_TRUE(scene().actor(inHall).frozen());
    }
    tick();   // the hall is on screen as this tick begins
    EXPECT_TRUE(scene().actor(inHall).awake());
    EXPECT_EQ(actors().position(inHall), (Vec2i{100, 100}));   // nothing walks while a scroll runs
    while (scene().camera().transitioning()) {
        tick();
    }
    tick();
    EXPECT_GT(actors().position(inHall).x, 100);
    EXPECT_EQ(scene().camera().area(), Keep::Hall);
}

const ScriptKit::Script counter{.name = "counter", .on = ActorKit::Enter,
                                .body = {ScriptKit::loop({set(Mine, Mine + 1), wait(1)})}};

TEST_F(SceneActors, WhileAScrollRunsTheWholeWorldHoldsStillScriptsIncludedAndCarriesOnAfter) {
    const Id counting = spawnAt(Reach::Area, kCourt, {40, 100}, Dir::Right, AtEdge::Stop, {&counter});
    const Id marker = scene().actor("marker").id();
    actors().moveTo(marker, kCourt, {150, 64});
    scene().follow(marker);
    while (!scene().camera().transitioning()) {
        tick(Way{.right = true});
    }
    const int count = actors().get(counting, Mine);
    const Vec2i at = actors().position(counting);
    int scrolling = 0;
    while (scene().camera().transitioning()) {
        tick();
        ++scrolling;
        if (scene().camera().transitioning()) {
            EXPECT_EQ(actors().get(counting, Mine), count);   // its script holds
            EXPECT_EQ(actors().position(counting), at);      // and so does its walk
        }
    }
    EXPECT_EQ(scrolling, 32);
    EXPECT_EQ(actors().place(marker), kHall);   // the followed actor carried through meanwhile
    ticks(3);
    EXPECT_GT(actors().get(counting, Mine), count);   // and the world carries on once it ends
}

TEST_F(SceneActors, AnActorFrozenByItsRuleCarriesOnFromWhereItStoppedWhenItWakes) {
    const Id paused = spawnAt(Reach::Area, kCourt, {40, 40}, Dir::None, AtEdge::Stop, {&counting});
    const Id twin = spawnAt(Reach::Area, kCourt, {60, 40}, Dir::None, AtEdge::Stop, {&counting});
    scene().actor(paused).awakeWhen(AwakeWhen::OnScreen);
    ticks(10);
    scene().camera() = WorldKit::Camera{kBank.layout, kBank.area};   // the court off screen for 20 ticks
    ticks(20);
    EXPECT_TRUE(scene().actor(paused).frozen());
    scene().camera() = WorldKit::Camera{kCourt.layout, kCourt.area};
    int n = 30;
    int twinAt = 0;
    while (actors().get(paused, Mine) == 0 && n < 300) {
        tick();
        ++n;
        if (twinAt == 0 && actors().get(twin, Mine) == 1) {
            twinAt = n;
        }
    }
    EXPECT_GT(twinAt, 0);
    EXPECT_EQ(n, twinAt + 20);
}

const ScriptKit::Script halting{.name = "halting", .on = ActorKit::Enter, .body = {Halt()}};

TEST_F(SceneActors, FreezeHoldsAnActorWhateverItsRuleAndWakeHandsItBackToItsRule) {
    const Id walking = spawnAt(Reach::Area, kCourt, {40, 40}, Dir::Right);
    tick();
    scene().actor(walking).freeze();
    EXPECT_TRUE(scene().actor(walking).frozen());
    EXPECT_FALSE(scene().actor(walking).awake());
    ticks(5);
    EXPECT_EQ(actors().position(walking), (Vec2i{41, 40}));
    scene().actor(walking).wake();
    EXPECT_TRUE(scene().actor(walking).awake());   // its rule, Always, holds: awake at once
    EXPECT_FALSE(scene().actor(walking).frozen());
    ticks(5);
    EXPECT_EQ(actors().position(walking), (Vec2i{46, 40}));

    const Id offScreen = spawnAt(Reach::Area, kBank, {40, 40}, Dir::Right);
    scene().actor(offScreen).awakeWhen(AwakeWhen::OnScreen);
    scene().actor(offScreen).freeze();
    scene().actor(offScreen).wake();
    EXPECT_TRUE(scene().actor(offScreen).frozen());   // its rule keeps it frozen off screen
    ticks(3);
    EXPECT_EQ(actors().position(offScreen), (Vec2i{40, 40}));

    const Id halted = spawnAt(Reach::Area, kCourt, {40, 80}, Dir::Right, AtEdge::Stop, {&halting});
    tick();   // its script starts and asks for Halt
    EXPECT_EQ(actors().position(halted), (Vec2i{41, 80}));
    tick();   // the handler freezes it before it walks
    EXPECT_EQ(actors().position(halted), (Vec2i{41, 80}));
    EXPECT_TRUE(scene().actor(halted).frozen());
}

// ── The hound's promotion ──────────────────────────────────────────────────────────────────────────

const ScriptKit::Script promoted{.name = "promoted", .on = ActorKit::Touch, .body = {GoAnywhere()}};

TEST_F(SceneActors, AHandlerThatSetsTheReachToWorldLetsTheActorThroughTheDoorItWasHeldAt) {
    const Id hound = spawnAt(Reach::Layout, kCourt, {66, 110}, Dir::Down, AtEdge::Stop, {&promoted});
    ticks(20);
    EXPECT_EQ(actors().position(hound), (Vec2i{66, 120}));   // held at the south door, into the moat
    scene().spawn(WorldKit::AreaActor{.actor = Actor{.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 8},
                                                     .look = kLook, .setsOff = {ActorKit::On::Touch}},
                                      .at = {60, 116}},
                  kCourt);
    ticks(3);   // the touch starts its script, which asks, and the handler promotes it
    EXPECT_EQ(scene().actor(hound).reach(), Reach::World);
    ticks(20);
    EXPECT_EQ(actors().place(hound), kBank);
}

// ── A removed actor ────────────────────────────────────────────────────────────────────────────────

TEST_F(SceneActors, EveryMemberOfAHandleOnARemovedActorThrowsOutOfRangeNamingItsId) {
    WorldKit::SceneActor guard = scene().actor("guard");
    actors().remove(guard.id());
    const std::string id = std::to_string(guard.id());
    const std::vector<std::function<void()>> calls{
        [&] { guard.reach(Reach::World); },        [&] { (void)guard.reach(); },
        [&] { guard.atEdge(AtEdge::Hide); },       [&] { (void)guard.atEdge(); },
        [&] { guard.awakeWhen(AwakeWhen::Always); }, [&] { (void)guard.awakeWhen(); },
        [&] { guard.freeze(); },                   [&] { guard.wake(); },
        [&] { (void)guard.frozen(); },             [&] { (void)guard.awake(); },
        [&] { (void)guard.hidden(); },
    };
    for (const std::function<void()>& call : calls) {
        EXPECT_NE(thrown(call).find(id), std::string::npos);
        EXPECT_THROW(call(), std::out_of_range);
    }
}

}  // namespace
