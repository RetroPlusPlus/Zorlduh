// A complete program built on the ActorKit module: a farmyard of two areas — a yard one screen big and a
// field larger than the screen, joined by a gate in the yard's east fence — and the actors in it.
//
//   - The hero is steered with the arrow keys, WASD or a d-pad; X or a pad's A button is Interact.
//   - The bell rings when the hero presses Interact standing below it; from any other side it stays still.
//   - The dog sleeps by its kennel until the hero comes near, and once it is awake the bell sends it
//     trotting over to the bell and back.
//   - The gate opens and shuts with Interact, and is solid while it is shut.
//   - The puddle splashes while the hero stands in it, drawn under the hero's feet.
//   - Four hens in the field scatter when the camera comes into it, then wander, and each lays three
//     eggs, one every four seconds. Walking onto an egg picks it up; the eggs picked up are the row along
//     the top.
//
// Every actor is spawned from a value made by a function below. ActorKit::tick runs them all; the camera
// and the drawing are this program's own WorldKit calls, made with what the actors report: the hero's box
// for the camera to follow, and each slot's sprites for WorldKit::Actors to draw.

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "retropp/animation.h"
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
#include "retropp/tilemap.h"
#include "retropp/timing.h"
#include "retropp/windowed_host.h"

#include "CameraKit/camera.h"
#include "LayerKit/sprite_layer.h"
#include "LayerKit/types.h"
#include "ScriptKit/bindings.h"
#include "TopDownMovement/directions.h"
#include "TopDownMovement/input_walker.h"
#include "TopDownMovement/walker.h"
#include "WorldKit/area.h"
#include "WorldKit/camera.h"
#include "WorldKit/world_layers.h"

#include "ActorKit/actor.h"
#include "ActorKit/actors.h"
#include "ActorKit/config.h"
#include "ActorKit/look.h"
#include "ActorKit/mover.h"
#include "ActorKit/sprites.h"
#include "ActorKit/tick.h"
#include "ActorKit/triggers.h"

#include "farm.h"
#include "scripts/answer.h"
#include "scripts/lay.h"
#include "scripts/pick_up.h"
#include "scripts/press_while.h"
#include "scripts/ring.h"
#include "scripts/scatter.h"
#include "scripts/toggle.h"
#include "scripts/wake.h"
#include "scripts/wander.h"

namespace {

using namespace farm;

enum class Action : std::uint8_t { Up, Down, Left, Right, Interact };

// ── The screen and the world ─────────────────────────────────────────────────────────────────────

constexpr int kCell = 8;

// The top 16 pixels of the screen hold the egg row; the world is shown below them.
constexpr CameraKit::View kView{.size = {.width = 160, .height = 128}, .origin = {.x = 0, .y = 16}};

// The values in a map, drawn as the tiles of the same name. Only grass is walked on.
enum class Cell : std::uint16_t { Grass = 0, Fence = 1, Kennel = 2 };

// Depths: the ground, the puddle's slot under the actors, the actors, and the egg row above the world.
constexpr std::int32_t kGroundZ = 0;
constexpr std::int32_t kUnderfootZ = 5;
constexpr std::int32_t kActorsZ = 10;
constexpr std::int32_t kHudZ = 20;

// A map ringed by fence, grass inside, with the cells of `openings` opened in the fence and `kennel`
// built inside.
retropp::IndexGrid Pen(int width, int height, const std::vector<retropp::Vec2i>& openings,
                       const std::vector<retropp::Vec2i>& kennel) {
    retropp::IndexGrid map{.width = width, .height = height};
    map.values.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    const auto set = [&](retropp::Vec2i cell, Cell value) {
        map.values[static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(cell.x)] =
            static_cast<std::uint16_t>(value);
    };
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool edge = x == 0 || y == 0 || x == width - 1 || y == height - 1;
            set({.x = x, .y = y}, edge ? Cell::Fence : Cell::Grass);
        }
    }
    for (const retropp::Vec2i cell : openings) {
        set(cell, Cell::Grass);
    }
    for (const retropp::Vec2i cell : kennel) {
        set(cell, Cell::Kennel);
    }
    return map;
}

// The two maps: the yard, with its gate in the east fence at rows 7 and 8 and the kennel in its
// top-left; the field, with its gate in the west fence at rows 11 and 12.
struct Maps {
    retropp::IndexGrid yard = Pen(20, 16, {{19, 7}, {19, 8}}, {{2, 2}, {3, 2}, {2, 3}, {3, 3}});
    retropp::IndexGrid field = Pen(40, 24, {{0, 11}, {0, 12}}, {});
};

// ── The art ──────────────────────────────────────────────────────────────────────────────────────

// The bytes of a file under this example's assets/ directory.
std::vector<std::uint8_t> readAsset(const std::string& name) {
    std::ifstream in(std::string(ACTORKIT_EXAMPLE_ASSETS) + "/" + name, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open assets/" + name);
    }
    return {std::istreambuf_iterator<char>(in), {}};
}

retropp::PaletteId loadPalette(retropp::Renderer& renderer, const std::string& name) {
    const std::vector<retropp::Rgba16> colors = retropp::slicePaletteImage(retropp::loadPngFromMemory(readAsset(name)));
    return renderer.uploadPalette(std::span<const retropp::Rgba16>(colors));
}

// The slots of the sprite sheet: 16×16 frames, eight to a row. The hero, the dog and the hen each have
// a row of walking frames — two facing down, two up, two left, two right — and the last row holds the
// things.
enum Sprite : std::size_t {
    kHeroWalk = 0,
    kDogWalk = 8,
    kHenWalk = 16,
    kDogAsleep = 24,
    kBellStill,
    kBellSwinging,
    kGateShut,
    kGateOpen,
    kPuddleStill,
    kPuddleSplashing,
    kEgg,
};

struct Art {
    retropp::AtlasManifest tiles;
    retropp::AtlasManifest sprites;
    retropp::PaletteId grass{}, field{}, fence{}, kennel{};
    retropp::PaletteId hero{}, dog{}, bell{}, gate{}, puddle{}, egg{};
    retropp::PaletteId henBrown{}, henWhite{}, henBlack{}, henGold{};
};

Art loadArt(retropp::Renderer& renderer) {
    return Art{
        .tiles = renderer.loadAtlasFromMemory(readAsset("tiles.png"), retropp::AssetDimensions{.width = kCell, .height = kCell},
                                              retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
                                              /*count=*/3),
        .sprites = renderer.loadAtlasFromMemory(readAsset("sprites.png"), retropp::AssetDimensions{.width = 16, .height = 16},
                                                retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
                                                /*count=*/32, retropp::TransparentIndices::of({0})),
        .grass = loadPalette(renderer, "palettes/grass.png"),
        .field = loadPalette(renderer, "palettes/field.png"),
        .fence = loadPalette(renderer, "palettes/fence.png"),
        .kennel = loadPalette(renderer, "palettes/kennel.png"),
        .hero = loadPalette(renderer, "palettes/hero.png"),
        .dog = loadPalette(renderer, "palettes/dog.png"),
        .bell = loadPalette(renderer, "palettes/bell.png"),
        .gate = loadPalette(renderer, "palettes/gate.png"),
        .puddle = loadPalette(renderer, "palettes/puddle.png"),
        .egg = loadPalette(renderer, "palettes/egg.png"),
        .henBrown = loadPalette(renderer, "palettes/hen-brown.png"),
        .henWhite = loadPalette(renderer, "palettes/hen-white.png"),
        .henBlack = loadPalette(renderer, "palettes/hen-black.png"),
        .henGold = loadPalette(renderer, "palettes/hen-gold.png"),
    };
}

// How a map's values are drawn: grass through `grass`, the fence and the kennel through their own.
retropp::TileCatalog Ground(const Art& art, retropp::PaletteId grass) {
    return retropp::TileCatalog{.entries = {
        {.id = static_cast<std::uint16_t>(Cell::Grass), .sheet = art.tiles.atlasId, .slot = 0, .palette = grass},
        {.id = static_cast<std::uint16_t>(Cell::Fence), .sheet = art.tiles.atlasId, .slot = 1, .palette = art.fence},
        {.id = static_cast<std::uint16_t>(Cell::Kennel), .sheet = art.tiles.atlasId, .slot = 2, .palette = art.kennel},
    }};
}

WorldKit::World FarmWorld(const Maps& maps, const Art& art) {
    using WorldKit::Side;
    using WorldKit::Transition;
    return WorldKit::World{.layouts = {WorldKit::Layout{
        .areas = {
            WorldKit::Area{.layers = {{.name = "ground", .tiles = &maps.yard, .art = Ground(art, art.grass), .z = kGroundZ}},
                           .actorSlots = {{.name = "underfoot", .z = kUnderfootZ}, {.name = "actors", .z = kActorsZ}},
                           .collision = &maps.yard,
                           .openings = {{.name = "gate", .side = Side::East, .span = {7, 8},
                                         .transition = Transition::Scroll,
                                         .leadsTo = {.area = Area::Field, .opening = "gate"}}}},
            WorldKit::Area{.layers = {{.name = "ground", .tiles = &maps.field, .art = Ground(art, art.field), .z = kGroundZ}},
                           .actorSlots = {{.name = "underfoot", .z = kUnderfootZ}, {.name = "actors", .z = kActorsZ}},
                           .collision = &maps.field,
                           .openings = {{.name = "gate", .side = Side::West, .span = {11, 12},
                                         .transition = Transition::Scroll,
                                         .leadsTo = {.area = Area::Yard, .opening = "gate"}}}},
        },
        .cellSize = kCell,
        .open = Cell::Grass,
        .view = kView,
        .scrollAligns = true,
    }}};
}

// Frame `slot` of the sprite sheet and the one after it, eight ticks each, through `palette`.
retropp::Animation Steps(const Art& art, std::size_t slot, retropp::PaletteId palette) {
    return retropp::Animation{{
        {.sheet = art.sprites.atlasId, .tileIndex = slot, .palette = palette, .duration = 8 * 1s / 60},
        {.sheet = art.sprites.atlasId, .tileIndex = slot + 1, .palette = palette, .duration = 8 * 1s / 60},
    }};
}

// A walker whose row of the sprite sheet starts at `row`, through `palette`, colliding with `feet`.
TopDownMovement::WalkerConfig Walk(const Art& art, std::size_t row, retropp::PaletteId palette, retropp::IntRect feet) {
    return TopDownMovement::WalkerConfig{
        .speed = 1,
        .collision = feet,
        .walk = {.up = Steps(art, row + 2, palette), .down = Steps(art, row, palette),
                 .left = Steps(art, row + 4, palette), .right = Steps(art, row + 6, palette)},
    };
}

ActorKit::Look LookOf(const Art& art, Sprite slot, retropp::PaletteId palette) {
    return ActorKit::lookOf(art.sprites, slot, palette);
}

// ── The game's input and state ───────────────────────────────────────────────────────────────────

// What the player does on one tick, read once from the engine's input.
struct Input {
    TopDownMovement::DirectionalInput move;
    bool interact = false;
};

Input readInput(const retropp::InputState& in) {
    return Input{
        .move = TopDownMovement::readDirections(
            in, {.up = Action::Up, .down = Action::Down, .left = Action::Left, .right = Action::Right}),
        .interact = in.justPressed(Action::Interact),
    };
}

struct State {
    ActorKit::Actors actors;
    WorldKit::Camera camera{Layout::Farm, Area::Yard};
    int eggs = 0;
    bool gateOpen = false;
    bool bellRung = false;
};

// ── The actors: values, made from the art ────────────────────────────────────────────────────────

ActorKit::Actor Hero(const Art& art) {
    return {.slot = "actors",
            .solid = true,
            .mover = ActorKit::movedBy<TopDownMovement::InputWalker>(
                Walk(art, kHeroWalk, art.hero, {.x = 3, .y = 10, .width = 10, .height = 6}), &Input::move),
            .actions = {{Action::Interact, &Input::interact}},
            .setsOff = {ActorKit::On::Touch, ActorKit::On::Near}};
}

ActorKit::Actor Bell(const Art& art) {
    return {.slot = "actors",
            .box = {.x = 0, .y = 8, .width = 16, .height = 8},
            .solid = true,
            .look = {{BellRung, LookOf(art, kBellSwinging, art.bell)}, {LookOf(art, kBellStill, art.bell)}},
            .scripts = {&ring}};
}

// Asleep, the dog shows its sleeping frame; awake, its walker's.
ActorKit::Actor Dog(const Art& art) {
    return {.slot = "actors",
            .solid = true,
            .ownValues = {{Awake, false}, {Heading, retropp::Dir::None}},
            .look = {{!Awake, LookOf(art, kDogAsleep, art.dog)}},
            .scripts = {&wake, &answer},
            .mover = ActorKit::movedBy<TopDownMovement::Walker>(
                Walk(art, kDogWalk, art.dog, {.x = 2, .y = 8, .width = 12, .height = 8}), Heading)};
}

ActorKit::Actor Gate(const Art& art) {
    return {.slot = "actors",
            .box = {.x = 0, .y = 0, .width = 8, .height = 16},
            .solid = !GateOpen,
            .look = {{GateOpen, LookOf(art, kGateOpen, art.gate)}, {LookOf(art, kGateShut, art.gate)}},
            .scripts = {toggle(GateOpen)}};
}

ActorKit::Actor Puddle(const Art& art) {
    return {.slot = "underfoot",
            .box = {.x = 0, .y = 8, .width = 16, .height = 8},
            .ownValues = {{Splashing, false}},
            .look = {{Splashing, LookOf(art, kPuddleSplashing, art.puddle)}, {LookOf(art, kPuddleStill, art.puddle)}},
            .scripts = {pressWhile(Splashing, ActorKit::Touched)}};
}

ActorKit::Actor Egg(const Art& art) {
    return {.slot = "actors",
            .box = {.x = 4, .y = 6, .width = 8, .height = 8},
            .look = LookOf(art, kEgg, art.egg),
            .scripts = {&pickUp}};
}

// Any hen, in any colors, fleeing any way: it scatters that way, then wanders, setting off that way first.
ActorKit::Actor Hen(const Art& art, retropp::PaletteId colors, retropp::Dir flees) {
    return {.slot = "actors",
            .solid = true,
            .ownValues = {{Heading, retropp::Dir::None}, {Laid, 0}},
            .scripts = {scatter(flees), wander(flees), lay(Egg(art))},
            .mover = ActorKit::movedBy<TopDownMovement::Walker>(
                Walk(art, kHenWalk, colors, {.x = 3, .y = 9, .width = 10, .height = 6}), Heading)};
}

// ── The egg row ──────────────────────────────────────────────────────────────────────────────────

// One egg along the top of the screen for each egg picked up.
LayerKit::Sprites EggRow(int eggs, const Art& art) {
    LayerKit::Sprites row;
    const retropp::AssetSlot& slot = art.sprites[kEgg];
    for (int n = 0; n < eggs; ++n) {
        row.push_back(retropp::Sprite{.key = "egg-row:" + std::to_string(n),
                                      .x = 2 + n * 10,
                                      .y = -2,
                                      .size = slot.dimensions,
                                      .atlas = art.sprites.atlasId,
                                      .tile = slot.tile,
                                      .palette = art.egg});
    }
    return row;
}

}  // namespace

int main() {
    const retropp::EngineConfig engine{
        .identity = {.organization = "Retro++", .application = "ActorKit Getting Started"},
        .window = {.title = "ActorKit — getting started"},
        .timing = retropp::TimingProfile{.tickPeriodNs = retropp::TickPeriodNs::Hz60},
        .enhancements = {.windowScale = 4},
    };
    retropp::EngineConfig::setActive(engine);

    retropp::SteadyClock clock;
    retropp::SdlPlatform platform;
    retropp::Renderer renderer{platform.device(), platform.sdlWindow()};
    retropp::RunLoop loop{clock};
    retropp::ActionMap actions{{Action::Interact, {SDL_SCANCODE_X, retropp::PadButton::FaceLabelA}}};
    actions.add(retropp::presets::directional(Action::Up, Action::Down, Action::Left, Action::Right));
    platform.actions(actions);

    const Art art = loadArt(renderer);
    const Maps maps;
    const WorldKit::World world = FarmWorld(maps, art);
    WorldKit::validate(world);

    // Handed over once: the world, which input action is Interact, the farm's values, and what the farm
    // does for its one command.
    const ScriptKit::Bindings<State> bindings{
        {GateOpen, &State::gateOpen},
        {BellRung, &State::bellRung},
    };
    const ActorKit::Handlers<State> handlers{
        {Collect, [](State& state, ActorKit::Id, const ScriptKit::Arguments&) { ++state.eggs; }},
    };
    const ActorKit::Config config{
        .world = &world, .interactAction = Action::Interact, .bindings = &bindings, .handlers = &handlers};

    // The starting actors: ordinary spawns. The hero is the followed actor, and the camera starts on it.
    State state{.actors = ActorKit::Actors{engine}};
    const ActorKit::Id hero = state.actors.spawn(Hero(art), {Layout::Farm, Area::Yard}, {72, 96});
    state.actors.follow(hero);
    state.camera.place(world, state.actors.box(hero));
    state.actors.spawn(Bell(art), {Layout::Farm, Area::Yard}, {kBellX, 16});
    state.actors.spawn(Dog(art), {Layout::Farm, Area::Yard}, {kKennelX, 16});
    state.actors.spawn(Gate(art), {Layout::Farm, Area::Yard}, {152, 56});
    state.actors.spawn(Puddle(art), {Layout::Farm, Area::Yard}, {32, 96});
    struct HenAt {
        retropp::PaletteId colors;
        retropp::Dir flees;
        retropp::Vec2i at;
    };
    for (const HenAt& hen : std::array<HenAt, 4>{{{art.henBrown, retropp::Dir::Left, {80, 56}},
                                                  {art.henWhite, retropp::Dir::Up, {136, 112}},
                                                  {art.henBlack, retropp::Dir::Right, {216, 80}},
                                                  {art.henGold, retropp::Dir::Down, {176, 88}}}}) {
        state.actors.spawn(Hen(art, hen.colors, hen.flees), {Layout::Farm, Area::Field}, hen.at);
    }

    loop.simTick([&](const retropp::InputState& in) {
        ActorKit::tick(state.actors, config, state.camera, readInput(in), state);

        // The camera follows the hero's box, and the hero goes where the camera says: carried through
        // the gate while the screen scrolls, and into the next area's pixels when it gets there.
        const WorldKit::Step step = state.camera.follow(world, state.actors.box(hero));
        const retropp::Vec2i at = state.actors.position(hero);
        state.actors.moveTo(hero, step.entered.value_or(state.actors.place(hero)),
                            {.x = at.x + step.move.x, .y = at.y + step.move.y});
    });

    retropp::FrameDrawState frame;
    loop.renderLoop([&] {
        // The world's layers, then the actors of the camera's area at each of its two slots.
        const WorldKit::Place here{.layout = state.camera.layout(), .area = state.camera.area()};
        frame.layers = WorldKit::WorldLayers(state.camera, world);
        frame.layers.push_back(WorldKit::Actors(state.camera, world, "underfoot",
                                                {ActorKit::Sprites(state.actors, config, here, "underfoot", state)}));
        frame.layers.push_back(WorldKit::Actors(state.camera, world, "actors",
                                                {ActorKit::Sprites(state.actors, config, here, "actors", state)}));
        frame.layers.push_back(LayerKit::SpriteLayer("hud:eggs", kHudZ, {EggRow(state.eggs, art)}));
        frame.regions = WorldKit::Fade(state.camera, world);
        renderer.renderFrame(frame);
    });

    retropp::WindowedHost{loop, platform}.run();
    return 0;
}
