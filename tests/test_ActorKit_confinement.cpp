// ActorKit's confinement: what a walking actor's walk is held inside. An actor starts held in its area;
// confined to its layout it walks out through a door and across a seam into its own layout's areas and
// is held at the door into another layout; unconfined it walks out through every opening that leads
// somewhere. Walls, and openings that lead nowhere, hold it either way; the player walks out whatever its
// confinement. An actor that walked out is still in its area, and WorldKit's landing reports the area
// it went into. The world is built in code, the actors are moved by a mover of the test's own, and the
// tests run headless.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "retropp/engine_config.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/palette.h"
#include "retropp/tilemap.h"
#include "retropp/timing.h"

#include "CameraKit/camera.h"

#include "WorldKit/area.h"
#include "WorldKit/camera.h"

#include "ActorKit/actor.h"
#include "ActorKit/actors.h"
#include "ActorKit/config.h"
#include "ActorKit/mover.h"
#include "ActorKit/tick.h"

namespace {

using ActorKit::Actor;
using ActorKit::Confinement;
using ActorKit::Id;
using retropp::Dir;
using retropp::IntRect;
using retropp::Vec2i;
using WorldKit::Place;
using WorldKit::Side;
using WorldKit::Transition;

retropp::EngineConfig configAt60() {
    retropp::EngineConfig config;
    config.timing = retropp::TimingProfile{.tickPeriodNs = retropp::TickPeriodNs::Hz60};
    return config;
}
const retropp::EngineConfig kConfig = configAt60();

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

// Layout 0, the keep, three areas. 0, the court, 160 × 128 pixels: an east door (rows 6–9) scrolling into
// 1; its north wall (cells 1–18) joined seamlessly to 2, two cells to the right; a postern in its west
// wall (rows 6–9) that leads nowhere; a south door (cells 8–11) scrolling into layout 1; stairs at
// (12, 4) fading into layout 1. 1, the hall, its west door back. 2, the garden, 24 × 12 cells, its south
// wall back: its top-left is (0, −96) in the court's pixels. Layout 1, the moat, one area, the bank, with
// its north door and its stairs back up.
struct Maps {
    retropp::IndexGrid court = room(20, 16, joined({column(19, 6, 9), row(0, 1, 18), column(0, 6, 9), row(15, 8, 11)}));
    retropp::IndexGrid hall = room(20, 16, column(0, 6, 9));
    retropp::IndexGrid garden = room(24, 12, row(11, 3, 20));
    retropp::IndexGrid bank = room(20, 16, row(0, 8, 11));
};

enum Keep : std::uint8_t { Court, Hall, Garden };

WorldKit::Area area(std::string name, const retropp::IndexGrid& map, std::vector<WorldKit::Opening> openings) {
    return WorldKit::Area{.name = std::move(name),
                          .layers = {{.name = "ground", .tiles = &map, .art = art()}},
                          .actorSlots = {{.name = "actors", .z = 5}},
                          .collision = &map,
                          .openings = std::move(openings)};
}

WorldKit::World worldOver(const Maps& maps) {
    return WorldKit::World{.layouts = {
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
                      {.name = "stairs", .at = {.x = 12, .y = 4, .width = 2, .height = 1}, .transition = Transition::Fade,
                       .leadsTo = {.layout = 1, .area = 0, .opening = "stairs"}}}),
                area("hall", maps.hall,
                     {{.name = "west door", .side = Side::West, .span = {6, 9}, .transition = Transition::Scroll,
                       .leadsTo = {.area = Keep::Court, .opening = "east door"}}}),
                area("garden", maps.garden,
                     {{.name = "south wall", .side = Side::South, .span = {3, 20}, .transition = Transition::Seamless,
                       .leadsTo = {.area = Keep::Court, .opening = "north wall", .offset = {.x = -2, .y = 0}}}}),
            },
            .cellSize = kCell,
            .open = Cell::Open,
            .view = kView,
        },
        WorldKit::Layout{
            .name = "moat",
            .areas = {area("bank", maps.bank,
                           {{.name = "north door", .side = Side::North, .span = {8, 11}, .transition = Transition::Scroll,
                             .leadsTo = {.layout = 0, .area = Keep::Court, .opening = "south door"}},
                            {.name = "stairs", .at = {.x = 5, .y = 8, .width = 2, .height = 1},
                             .transition = Transition::Fade,
                             .leadsTo = {.layout = 0, .area = Keep::Court, .opening = "stairs"}}})},
            .cellSize = kCell,
            .open = Cell::Open,
            .view = kView,
        },
    }};
}

constexpr Place kCourt{.layout = 0, .area = Keep::Court};
constexpr Place kHall{.layout = 0, .area = Keep::Hall};
constexpr Place kGarden{.layout = 0, .area = Keep::Garden};

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

struct Input {
    Way way;
};

struct State {
    ActorKit::Actors actors{kConfig};
    WorldKit::Camera camera{kCourt.layout, kCourt.area};
};

// A walker driven by the input's `way`, solid, 8 × 8 from its position.
Actor slider() {
    return {.slot = "actors", .solid = true, .mover = ActorKit::movedBy<Slider>(SliderConfig{}, &Input::way)};
}

std::string thrown(const std::function<void()>& run) {
    try {
        run();
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

class Confined : public ::testing::Test {
protected:
    Maps maps;
    WorldKit::World world = worldOver(maps);
    State state;
    ActorKit::Config config{.world = &world};

    void SetUp() override { WorldKit::validate(world); }

    void tick(const Input& input = {}) { ActorKit::tick(state.actors, config, state.camera, input, state); }

    void walk(Way way, int ticks) {
        for (int n = 0; n < ticks; ++n) {
            tick(Input{.way = way});
        }
    }

    // A walker in the court at `at`, its confinement set to `confinement`.
    Id walkerAt(Vec2i at, Confinement confinement) {
        const Id id = state.actors.spawn(slider(), kCourt, at);
        state.actors.confinement(id, confinement);
        return id;
    }

    // Walks the actor by `way` until WorldKit's landing, handed its box before and after each tick,
    // reports a place entered, and returns that place; empty after 40 ticks.
    std::optional<Place> walkUntilLanding(Id id, Way way) {
        for (int n = 0; n < 40; ++n) {
            const IntRect was = state.actors.box(id);
            tick(Input{.way = way});
            const WorldKit::Step landing =
                WorldKit::detail::through(world, state.actors.place(id), was, state.actors.box(id));
            if (landing.entered) {
                return landing.entered;
            }
        }
        return std::nullopt;
    }
};

TEST_F(Confined, ANewActorsConfinementIsItsAreaAndItIsHeldAtEveryEdgeWithItsBoxInside) {
    const Id walker = state.actors.spawn(slider(), kCourt, {150, 56});
    EXPECT_EQ(state.actors.confinement(walker), Confinement::Area);
    walk({.right = true}, 20);   // the east door
    EXPECT_EQ(state.actors.position(walker), (Vec2i{152, 56}));
    state.actors.moveTo(walker, kCourt, {70, 10});
    walk({.up = true}, 20);   // the seam into the garden
    EXPECT_EQ(state.actors.position(walker), (Vec2i{70, 0}));
    state.actors.moveTo(walker, kCourt, {70, 110});
    walk({.down = true}, 20);   // the south door, into the other layout
    EXPECT_EQ(state.actors.position(walker), (Vec2i{70, 120}));
    EXPECT_EQ(state.actors.place(walker), kCourt);
}

TEST_F(Confined, UnconfinedItWalksOutThroughADoorAndTheWallBesideTheDoorHoldsIt) {
    const Id walker = walkerAt({150, 56}, Confinement::None);
    walk({.right = true}, 20);
    EXPECT_EQ(state.actors.position(walker), (Vec2i{170, 56}));
    walk({.up = true}, 20);   // past the edge, the wall above the door's rows 6–9 (pixels 48–79)
    EXPECT_EQ(state.actors.position(walker), (Vec2i{170, 48}));
    walk({.down = true}, 40);
    EXPECT_EQ(state.actors.position(walker), (Vec2i{170, 72}));
    EXPECT_EQ(state.actors.place(walker), kCourt);
}

TEST_F(Confined, ConfinedToItsLayoutItWalksOutThroughADoorWithinItAndIsHeldAtTheDoorIntoTheOther) {
    const Id gardener = walkerAt({150, 56}, Confinement::Layout);
    walk({.right = true}, 20);   // into the hall's direction, the keep
    EXPECT_EQ(state.actors.position(gardener), (Vec2i{170, 56}));

    state.actors.moveTo(gardener, kCourt, {66, 110});
    const Id hound = walkerAt({80, 110}, Confinement::None);
    walk({.down = true}, 20);   // the south door, into the moat
    EXPECT_EQ(state.actors.position(gardener), (Vec2i{66, 120}));
    EXPECT_EQ(state.actors.position(hound), (Vec2i{80, 130}));
}

TEST_F(Confined, AcrossASeamWithinTheLayoutTheLayoutAndNoneWalkOnOverTheNextAreasCellsAndAreaIsHeld) {
    const Id layout = walkerAt({30, 10}, Confinement::Layout);
    const Id none = walkerAt({70, 10}, Confinement::None);
    const Id area = walkerAt({110, 10}, Confinement::Area);
    walk({.up = true}, 120);   // the garden's interior is rows −88 to −9 of the court's pixels
    EXPECT_EQ(state.actors.position(layout), (Vec2i{30, -88}));
    EXPECT_EQ(state.actors.position(none), (Vec2i{70, -88}));
    EXPECT_EQ(state.actors.position(area), (Vec2i{110, 0}));
}

TEST_F(Confined, AnOpeningThatLeadsNowhereHoldsAnUnconfinedActor) {
    const Id walker = walkerAt({10, 56}, Confinement::None);
    walk({.left = true}, 20);   // the postern
    EXPECT_EQ(state.actors.position(walker), (Vec2i{0, 56}));
}

TEST_F(Confined, ThePlayerWalksOutThroughEveryOpeningWhateverItsConfinement) {
    const Id hero = state.actors.spawn(slider(), kCourt, {150, 56});
    state.actors.player(hero);
    walk({.right = true}, 20);
    EXPECT_EQ(state.actors.position(hero), (Vec2i{170, 56}));

    state.actors.confinement(hero, Confinement::Layout);
    state.actors.moveTo(hero, kCourt, {70, 110});
    walk({.down = true}, 20);   // the south door, into the moat
    EXPECT_EQ(state.actors.position(hero), (Vec2i{70, 130}));
}

TEST_F(Confined, ConfinementIsReadBackKeptByMoveToCarriedByACopyAndAnUnknownIdThrows) {
    const Id walker = walkerAt({40, 40}, Confinement::Layout);
    EXPECT_EQ(state.actors.confinement(walker), Confinement::Layout);
    state.actors.moveTo(walker, kHall, {40, 40});
    EXPECT_EQ(state.actors.confinement(walker), Confinement::Layout);

    ActorKit::Actors copy = state.actors;
    EXPECT_EQ(copy.confinement(walker), Confinement::Layout);
    copy.confinement(walker, Confinement::None);
    EXPECT_EQ(copy.confinement(walker), Confinement::None);
    EXPECT_EQ(state.actors.confinement(walker), Confinement::Layout);

    EXPECT_NE(thrown([&] { (void)state.actors.confinement(99); }).find("99"), std::string::npos);
    EXPECT_THROW(state.actors.confinement(99, Confinement::None), std::out_of_range);
}

TEST_F(Confined, AnUnconfinedActorWalksOverStairsAndStaysInItsArea) {
    const Id walker = walkerAt({100, 50}, Confinement::None);
    walk({.up = true}, 15);   // onto the stairs, pixels 96–111 × 32–39
    EXPECT_EQ(state.actors.position(walker), (Vec2i{100, 35}));
    EXPECT_EQ(state.actors.place(walker), kCourt);
}

TEST_F(Confined, AnActorThatWalkedOutIsStillInItsAreaAndWorldKitsLandingReportsTheAreaItWentInto) {
    const Id walker = walkerAt({150, 60}, Confinement::None);
    EXPECT_EQ(walkUntilLanding(walker, {.right = true}), kHall);
    EXPECT_EQ(state.actors.place(walker), kCourt);
    EXPECT_EQ(state.actors.position(walker), (Vec2i{156, 60}));   // its center at 160, past the wall

    const Id climber = walkerAt({70, 10}, Confinement::Layout);
    EXPECT_EQ(walkUntilLanding(climber, {.up = true}), kGarden);
    EXPECT_EQ(state.actors.place(climber), kCourt);
    EXPECT_EQ(state.actors.position(climber), (Vec2i{70, -5}));   // its center at −1, past the wall
}

}  // namespace
