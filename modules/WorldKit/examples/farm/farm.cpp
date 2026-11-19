// A complete program built on ActorKit, the WorldKit module's actors: a farmyard of three areas — a yard one screen big, a
// field larger than the screen through a gate in the yard's east fence, and a pond one screen big through
// a gap in its south fence — and the actors in them.
//
//   - The hero is steered with the arrow keys, WASD or a d-pad; X or a pad's A button is Interact.
//   - The bell rings when the hero presses Interact standing below it; from any other side it stays still.
//   - The dog sleeps by its kennel until the hero comes near, and once it is awake the bell sends it
//     trotting over to the bell and back.
//   - The gate opens and shuts with Interact, and is solid while it is shut.
//   - The puddle splashes while the hero stands in it, drawn under the hero's feet.
//   - South of the yard lies a pond. With every step the hero takes in it, the pond's sprite ripples out
//     from the hero's feet, and the rings of several steps spread over each other.
//   - Four hens in the field scatter when the camera comes into it, then wander, and each lays three
//     eggs, one every four seconds. Walking onto an egg picks it up; the eggs picked up are the row along
//     the top.
//
// Every actor is spawned from a value made by a function below. ActorKit::tick runs them all; the camera
// and the drawing are this program's own WorldKit calls, made with what the actors report: the hero's box
// for the camera to follow, and each slot's sprites in each area on screen, drawn on a layer of that
// area's own.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
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
#include "scripts/splash.h"
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

// The three maps: the yard, with its gate in the east fence at rows 7 and 8, a gap in its south fence at
// columns 9 and 10, and the kennel in its top-left; the field, with its gate in the west fence at rows 11
// and 12; the pond's meadow, with its gap in the north fence at columns 9 and 10.
struct Maps {
    retropp::IndexGrid yard = Pen(20, 16, {{19, 7}, {19, 8}, {9, 15}, {10, 15}}, {{2, 2}, {3, 2}, {2, 3}, {3, 3}});
    retropp::IndexGrid field = Pen(40, 24, {{0, 11}, {0, 12}}, {});
    retropp::IndexGrid pond = Pen(20, 16, {{9, 0}, {10, 0}}, {});
};

// ── The art ──────────────────────────────────────────────────────────────────────────────────────

// The bytes of a file under this example's assets/ directory.
std::vector<std::uint8_t> readAsset(const std::string& name) {
    std::ifstream in(std::string(WORLDKIT_FARM_ASSETS) + "/" + name, std::ios::binary);
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
    retropp::AtlasManifest pond;   // one 128×128 slot: the pond, 128×80, in its middle rows, drawn through the
                                   // puddle's palette. Square, so its ripples are round.
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
        .pond = renderer.loadAtlasFromMemory(readAsset("pond.png"), retropp::AssetDimensions{.width = 128, .height = 128},
                                             retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
                                             /*count=*/1, retropp::TransparentIndices::of({0})),
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
                                         .leadsTo = {.area = Area::Field, .opening = "gate"}},
                                        {.name = "gap", .side = Side::South, .span = {9, 10},
                                         .transition = Transition::Scroll,
                                         .leadsTo = {.area = Area::Pond, .opening = "gap"}}}},
            WorldKit::Area{.layers = {{.name = "ground", .tiles = &maps.field, .art = Ground(art, art.field), .z = kGroundZ}},
                           .actorSlots = {{.name = "underfoot", .z = kUnderfootZ}, {.name = "actors", .z = kActorsZ}},
                           .collision = &maps.field,
                           .openings = {{.name = "gate", .side = Side::West, .span = {11, 12},
                                         .transition = Transition::Scroll,
                                         .leadsTo = {.area = Area::Yard, .opening = "gate"}}}},
            WorldKit::Area{.layers = {{.name = "ground", .tiles = &maps.pond, .art = Ground(art, art.grass), .z = kGroundZ}},
                           .actorSlots = {{.name = "underfoot", .z = kUnderfootZ}, {.name = "actors", .z = kActorsZ}},
                           .collision = &maps.pond,
                           .openings = {{.name = "gap", .side = Side::North, .span = {9, 10},
                                         .transition = Transition::Scroll,
                                         .leadsTo = {.area = Area::Yard, .opening = "gap"}}}},
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

// A ripple running over one actor's sprite: from `center`, in the sprite's art pixels, for kRippleTicks.
struct Rippling {
    ActorKit::Id actor = ActorKit::kNoId;
    retropp::Point center{};
    int age = 0;   // ticks since it started
};

struct State {
    ActorKit::Actors actors;
    WorldKit::Camera camera{Layout::Farm, Area::Yard};
    int eggs = 0;
    bool gateOpen = false;
    bool bellRung = false;
    std::vector<Rippling> ripples;
};

// How a ripple looks, in the sprite's art pixels (shaders/ripple_ring.frag.hlsl draws it): a band
// of rings 6 pixels apart, reaching 5 pixels either side of a front that starts 2 pixels from where the foot
// came down and travels outward a pixel a tick. A crest pushes the art up to 1.5 pixels and lightens it
// by a quarter; both fade out over 60 ticks. Up to kRipplesAtOnce run on one sprite — the shader's
// four ripple slots; a newer one takes the oldest one's place.
constexpr int kRippleTicks = 60;
constexpr float kRippleStart = 2.0f;
constexpr float kRippleSpeed = 1.0f;
constexpr float kRippleWidth = 5.0f;
constexpr float kRippleSpacing = 6.0f;
constexpr float kRipplePush = 1.5f;
constexpr float kRippleShade = 0.25f;
constexpr std::size_t kRipplesAtOnce = 4;

// One tick of every ripple: each ages, and one that has run its course is dropped.
void AdvanceRipples(std::vector<Rippling>& ripples) {
    for (Rippling& ripple : ripples) {
        ++ripple.age;
    }
    std::erase_if(ripples, [](const Rippling& ripple) { return ripple.age >= kRippleTicks; });
}

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

// The pond: drawn under the hero's feet, touched anywhere on its water, rippling where the hero steps. The
// water is the middle 80 rows of its 128-pixel art.
ActorKit::Actor PondActor(const Art& art) {
    return {.slot = "underfoot",
            .box = {.x = 8, .y = 32, .width = 112, .height = 64},
            .ownValues = {{SteppedX, 0}, {SteppedY, 0}},
            .look = ActorKit::lookOf(art.pond, 0, art.puddle),
            .scripts = {&splash}};
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

// ── What is drawn under the actors' feet ─────────────────────────────────────────────────────────

// One ripple as the shader takes it: where the foot came down, how far the front has travelled, and how
// strong the ripple still is.
retropp::Vec4 RippleSlot(const Rippling& ripple) {
    const float age = static_cast<float>(ripple.age);
    return {.x = ripple.center.x, .y = ripple.center.y, .z = kRippleStart + age * kRippleSpeed,
            .w = 1.0f - age / kRippleTicks};
}

// Every ripple running on `actor` as one effect on its sprite of art `size`, drawn by the ripple shader
// `shader`; empty once every ripple on it has faded out. One effect carries them all: the shader adds them together.
std::optional<retropp::ScreenSpaceEffect> RipplesOn(const State& state, ActorKit::Id actor,
                                                    retropp::AssetDimensions size, retropp::PostProcessStageId shader) {
    std::array<retropp::Vec4, kRipplesAtOnce> slots{};
    std::size_t used = 0;
    for (const Rippling& ripple : state.ripples) {
        if (ripple.actor == actor && used < slots.size()) {
            slots[used++] = RippleSlot(ripple);
        }
    }
    if (used == 0) {
        return std::nullopt;
    }
    retropp::ScreenSpaceEffect effect{.kind = retropp::ScreenSpaceEffectKind::Custom, .customShader = shader};
    effect.ringA = slots[0];
    effect.ringB = slots[1];
    effect.ringC = slots[2];
    effect.ringD = slots[3];
    effect.ringArt = {static_cast<float>(size.width), static_cast<float>(size.height)};
    effect.ringWidth = kRippleWidth;
    effect.ringSpacing = kRippleSpacing;
    effect.ringPush = kRipplePush;
    effect.ringShade = kRippleShade;
    return effect;
}

// The sprites at the "underfoot" slot of `here`, each carrying the ripples running on its actor, drawn by
// the ripple shader `shader`.
LayerKit::Sprites Underfoot(const State& state, const ActorKit::Config& config, WorldKit::Place here,
                            retropp::PostProcessStageId shader) {
    LayerKit::Sprites sprites = ActorKit::Sprites(state.actors, config, here, "underfoot", state);
    for (const ActorKit::Id id : state.actors.ids()) {
        const std::string key = state.actors.key(id);
        for (retropp::Sprite& sprite : sprites) {
            if (sprite.key.value != key) {
                continue;
            }
            if (const std::optional<retropp::ScreenSpaceEffect> ripples = RipplesOn(state, id, sprite.size, shader)) {
                sprite.effects.push_back(*ripples);
            }
        }
    }
    return sprites;
}

// ── An area's actors on screen ───────────────────────────────────────────────────────────────────

// The layer of `sprites`, the actors of the area `shown` at its slot `slot`: at that slot's depth, with that
// area's scroll, kept inside the view. Each area has a layer of its own, keyed by the area, so a layer's
// scroll moves only with the camera, and an area scrolling in brings its actors with it.
retropp::DrawLayer AreaSlot(const WorldKit::World& world, const WorldKit::Shown& shown, std::string_view slot,
                            LayerKit::Sprites sprites) {
    const WorldKit::Place place{.layout = shown.layout, .area = shown.area};
    const std::string key =
        "farm:" + std::to_string(shown.layout) + ":" + std::to_string(shown.area) + ":" + std::string(slot);
    const CameraKit::View& view = world.layouts[static_cast<std::size_t>(shown.layout)].view;
    return LayerKit::SpriteLayer(key, WorldKit::ActorZ(world, place, slot), {std::move(sprites)},
                                 {.scroll = shown.scroll, .regions = WorldKit::ViewStencil(view)});
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
    // The pond's ripple: a shader of the example's own, compiled and embedded by the build from its path.
    const retropp::PostProcessStageId rippleShader =
        renderer.registerPostProcessStage("examples/farm/shaders/ripple_ring.frag.hlsl");
    const Maps maps;
    const WorldKit::World world = FarmWorld(maps, art);
    WorldKit::validate(world);

    // Handed over once: the world, which input action is Interact, the farm's values, and what the farm
    // does for its two commands.
    const ScriptKit::Bindings<State> bindings{
        {GateOpen, &State::gateOpen},
        {BellRung, &State::bellRung},
    };
    const ActorKit::Handlers<State> handlers{
        {Collect, [](State& state, ActorKit::Id, const ScriptKit::Arguments&) { ++state.eggs; }},
        // A new ripple over the asking actor's sprite. With kRipplesAtOnce already running there, the oldest
        // of them makes room.
        {Ripple, [](State& state, ActorKit::Id asker, const ScriptKit::Arguments& arguments) {
             const auto mine = [asker](const Rippling& ripple) { return ripple.actor == asker; };
             if (static_cast<std::size_t>(std::count_if(state.ripples.begin(), state.ripples.end(), mine)) >=
                 kRipplesAtOnce) {
                 state.ripples.erase(std::find_if(state.ripples.begin(), state.ripples.end(), mine));
             }
             state.ripples.push_back({.actor = asker,
                                      .center = {.x = static_cast<float>(arguments.number(0)),
                                                 .y = static_cast<float>(arguments.number(1))}});
         }},
    };
    const ActorKit::Config config{
        .world = &world, .interactAction = Action::Interact, .bindings = &bindings, .handlers = &handlers};

    // The starting actors: ordinary spawns. The hero is the player, and the camera starts on it.
    State state{.actors = ActorKit::Actors{engine}};
    const ActorKit::Id hero = state.actors.spawn(Hero(art), {Layout::Farm, Area::Yard}, {72, 96});
    state.actors.player(hero);
    state.camera.place(world, state.actors.box(hero));
    state.actors.spawn(Bell(art), {Layout::Farm, Area::Yard}, {kBellX, 16});
    state.actors.spawn(Dog(art), {Layout::Farm, Area::Yard}, {kKennelX, 16});
    state.actors.spawn(Gate(art), {Layout::Farm, Area::Yard}, {152, 56});
    state.actors.spawn(Puddle(art), {Layout::Farm, Area::Yard}, {32, 96});
    state.actors.spawn(PondActor(art), {Layout::Farm, Area::Pond}, {16, 4});
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
        AdvanceRipples(state.ripples);
        ActorKit::tick(state.actors, config, state.camera, readInput(in), state);

        // The camera follows the hero's box, and the hero goes where the camera says: carried through
        // the gate while the screen scrolls, and into the next area's pixels when it gets there.
        const WorldKit::Step step = state.camera.follow(world, state.actors.box(hero));
        const retropp::Vec2i at = state.actors.position(hero);
        state.actors.moveTo(hero, step.entered.value_or(state.actors.place(hero)),
                            {.x = at.x + step.move.x, .y = at.y + step.move.y});
        // On the tick the hero enters the next area its position changes to that area's pixels and its sprite
        // to that area's layer: shown fresh, the sprite mounts where it is, in place of easing across.
        if (step.entered) {
            state.actors.fresh(hero);
        }
    });

    retropp::FrameDrawState frame;
    loop.renderLoop([&] {
        // The world's layers, then, for every area on screen — the camera's, and during a scroll the one
        // scrolling in — its actors at each of its two slots.
        frame.layers = WorldKit::WorldLayers(state.camera, world);
        for (const WorldKit::Shown& shown : state.camera.shown(world)) {
            const WorldKit::Place place{.layout = shown.layout, .area = shown.area};
            frame.layers.push_back(AreaSlot(world, shown, "underfoot", Underfoot(state, config, place, rippleShader)));
            frame.layers.push_back(
                AreaSlot(world, shown, "actors", ActorKit::Sprites(state.actors, config, place, "actors", state)));
        }
        frame.layers.push_back(LayerKit::SpriteLayer("hud:eggs", kHudZ, {EggRow(state.eggs, art)}));
        frame.regions = WorldKit::Fade(state.camera, world);
        renderer.renderFrame(frame);
    });

    retropp::WindowedHost{loop, platform}.run();
    return 0;
}
