#include "game/sim.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <vector>

#include "retropp/geometry.h"

#include "WorldKit/solids.h"

#include "game/player.h"

namespace zorlduh {

namespace {

// Applies the option `value` names: a TEXT speed or a WINDOWS style. False when it names neither.
bool applyOption(GameState& state, MenuKit::Value value) {
    switch (static_cast<MenuItem>(value.id)) {
        case MenuItem::TextSlow:
            state.messageTicksPerGlyph = 4;
            return true;
        case MenuItem::TextNormal:
            state.messageTicksPerGlyph = 2;
            return true;
        case MenuItem::TextFast:
            state.messageTicksPerGlyph = 1;
            return true;
        case MenuItem::StyleGrow:
            state.menuStyle = MessageStyle::Grow;
            return true;
        case MenuItem::StyleClip:
            state.menuStyle = MessageStyle::Clip;
            return true;
        case MenuItem::StyleSquash:
            state.menuStyle = MessageStyle::Squash;
            return true;
        case MenuItem::StyleFade:
            state.menuStyle = MessageStyle::Fade;
            return true;
        case MenuItem::StyleSlide:
            state.menuStyle = MessageStyle::Slide;
            return true;
        case MenuItem::StylePop:
            state.menuStyle = MessageStyle::Pop;
            return true;
        default:
            return false;
    }
}

// Puts the player in `warp`'s area at the open place nearest its middle: over the area's cells in order
// of distance from the middle cell (the sum of the two distances, ties by row then column), the first
// whose pixel position leaves the player's collision box clear of the area's solids. The camera is
// placed on it, the area's name types from its start, and the entry is an AreaChanged event.
void warpTo(GameState& state, const Config& config, const Warp& warp) {
    state.camera = WorldKit::Camera{warp.layout, warp.area};
    const WorldKit::Layout& layout = config.world.layouts[static_cast<std::size_t>(warp.layout)];
    const retropp::IndexGrid& map = *layout.areas[static_cast<std::size_t>(warp.area)].collision;
    const retropp::Vec2i middle{.x = map.width / 2, .y = map.height / 2};

    std::vector<retropp::Vec2i> cells;
    cells.reserve(static_cast<std::size_t>(map.width * map.height));
    for (int y = 0; y < map.height; ++y) {
        for (int x = 0; x < map.width; ++x) {
            cells.push_back({.x = x, .y = y});
        }
    }
    const auto distance = [&](retropp::Vec2i cell) { return std::abs(cell.x - middle.x) + std::abs(cell.y - middle.y); };
    std::stable_sort(cells.begin(), cells.end(),
                     [&](retropp::Vec2i a, retropp::Vec2i b) { return distance(a) < distance(b); });

    const WorldKit::AreaSolids solids = state.camera.solids(config.world);
    for (const retropp::Vec2i cell : cells) {
        state.player.place({.x = cell.x * layout.cellSize, .y = cell.y * layout.cellSize});
        if (!solids.solidBox(state.player.collision(config.player).bounds())) {
            break;
        }
    }
    state.camera.place(config.world, state.player.collision(config.player).bounds());
    state.events.push_back(AreaChanged{.layout = warp.layout, .area = warp.area});
    state.nameReveal = TextKit::Reveal{};
}

// The title screen's tick: the title menu opens when it is not showing and takes the menu input.
void titleTick(GameState& state, const Input& input, const Config& config) {
    if (!state.title.showing()) {
        state.title.open(config.title);
    }
    const MenuKit::Step step = state.title.advance(config.title, input.menu);
    if (step.chosen) {
        if (*step.chosen == MenuKit::Value{MenuItem::Start}) {
            state.title.close();
        } else if (applyOption(state, *step.chosen)) {
            state.title.back();
        }
    }
    if (step.closed) {
        state.screen = Screen::Playing;
    }
}

// The pause menu's tick, the world held still under it. False when the tick ended at the title screen.
bool pauseTick(GameState& state, const Input& input, const Config& config) {
    if (input.start) {
        state.pause.close();
    }
    const MenuKit::Step step = state.pause.advance(config.pause, input.menu);
    if (step.chosen) {
        const MenuKit::Value chosen = *step.chosen;
        if (chosen == MenuKit::Value{MenuItem::Resume}) {
            state.pause.close();
        } else if (chosen == MenuKit::Value{MenuItem::Title}) {
            returnToTitle(state, config);
            return false;
        } else if (const std::optional<std::size_t> warp = warpOf(chosen)) {
            warpTo(state, config, warps()[*warp]);
            state.pause.close();
        } else if (applyOption(state, chosen)) {
            state.pause.back();
        }
    }
    return true;
}

}  // namespace

Config gameConfig(const Assets& assets, CameraKit::View view, const AreaDepths& depths, retropp::IntRect messageBox,
                  const MenuBoxes& menuBoxes) {
    WindowKit::Animation messageAnimation;
    messageAnimation.duration = kMessageAnimationTime;
    WindowKit::Animation menuAnimation;
    menuAnimation.duration = kMenuAnimationTime;
    return Config{
        .player = PlayerConfig(assets),
        .world = gameWorld(assets, view, depths),
        .font = assets.font,
        .borders = assets.borders,
        .messageBox = messageBox,
        .messageAnimation = messageAnimation,
        .title = titleMenu(menuBoxes, menuAnimation),
        .pause = pauseMenu(menuBoxes, menuAnimation),
    };
}

const WindowKit::Border& borderOf(const Borders& borders, Material material) noexcept {
    return material == Material::Stone ? borders.stone : borders.wood;
}

void returnToTitle(GameState& state, const Config& config) {
    state.player = TopDownMovement::Walker{kPlayerStart};
    state.camera = WorldKit::Camera{kStartLayout, kStartArea};
    state.camera.place(config.world, state.player.collision(config.player).bounds());
    state.nameReveal = TextKit::Reveal{};
    state.dialog = WindowKit::Dialog{};
    state.title = MenuKit::Menu{};
    state.pause = MenuKit::Menu{};
    state.screen = Screen::Title;
}

void tick(GameState& state, const Input& input, const Config& config) {
    state.events.clear();

    if (state.screen == Screen::Title) {
        titleTick(state, input, config);
        return;
    }

    if (state.pause.showing()) {
        if (pauseTick(state, input, config)) {
            state.nameReveal.advance();
        }
        return;
    }

    // A message holds the world still. The press that opens one is never also its first confirm: the
    // tick that opens it does not advance it.
    if (state.dialog.showing()) {
        state.dialog.advance(input.item);
        state.nameReveal.advance();
        return;
    }

    if (!state.camera.transitioning()) {
        state.player.walk(config.player, input.move, state.camera.solids(config.world));
    }

    const WorldKit::Step step = state.camera.follow(config.world, state.player.collision(config.player).bounds());
    if (step.move != retropp::Vec2i{}) {
        const retropp::Vec2i at = state.player.position();
        state.player.place({.x = at.x + step.move.x, .y = at.y + step.move.y});
    }
    if (step.entered) {
        state.events.push_back(AreaChanged{.layout = static_cast<Layout>(step.entered->layout), .area = step.entered->area});
        state.nameReveal = TextKit::Reveal{};
    }

    if (input.item && !state.camera.transitioning()) {
        const Sign* sign = signFaced(static_cast<Layout>(state.camera.layout()), state.camera.area(),
                                     state.player.collision(config.player).bounds(), state.player.facing());
        if (sign != nullptr) {
            const retropp::IntRect room = WindowKit::inside(config.messageBox, borderOf(config.borders, sign->material));
            state.reading = sign->material;
            state.readingStyle = sign->style;
            state.dialog.animation = config.messageAnimation;
            state.dialog.ticksPerGlyph = state.messageTicksPerGlyph;
            state.dialog.open(sign->text, {.width = room.width, .height = room.height}, config.font);
        }
    }

    // The press that opens the pause menu is never also read by it: the tick that opens it does not
    // advance it.
    if (input.start && !state.camera.transitioning() && !state.dialog.showing()) {
        state.pause.open(config.pause);
    }

    state.nameReveal.advance();
}

}  // namespace zorlduh
