// A complete program built on the MenuKit module: a line across the top saying what the last tick
// reported, and a menu that cancel does not close — PLAY; LOAD, which cannot be chosen; COLORS, opening
// six colors three across; NUMBERS, opening ONE to TWELVE four at a time in pages; and LETTERS, opening
// ALPHA to HOTEL three at a time, scrolling and stopping at either end. Each window opens and closes over
// 200 ms, and a direction held moves the cursor again after 400 ms, then every 100 ms.
//
// The arrow keys, WASD or a pad's directions move the cursor; Z, Space, Return or a pad's A button
// confirms; X, Backspace or a pad's B button cancels. The border is assets/border.png, a sheet of nine box
// pieces and an arrow; the font is assets/font.png; their colors are the palette images in
// assets/palettes/. docs/getting-started.md walks through it part by part.

#include <chrono>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
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
#include "retropp/windowed_host.h"

#include "LayerKit/sprite_layer.h"
#include "MenuKit/input.h"
#include "MenuKit/menu.h"
#include "MenuKit/menu_config.h"
#include "MenuKit/menu_windows.h"
#include "TextKit/font.h"
#include "TextKit/text.h"
#include "WindowKit/animation.h"
#include "WindowKit/border.h"

namespace {

enum class Action : std::uint8_t { Up, Down, Left, Right, Confirm, Cancel };

// The characters of assets/font.png, one per cell, in the sheet's read order. The sheet's last row is
// padded with blank cells, which are not loaded.
constexpr std::string_view kGlyphs = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?'-:";

// The bytes of a file under this example's assets/ directory.
std::vector<std::uint8_t> readAsset(const std::string& name) {
    std::ifstream in(std::string(MENUKIT_EXAMPLE_ASSETS) + "/" + name, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open assets/" + name);
    }
    return {std::istreambuf_iterator<char>(in), {}};
}

// A palette image from this example's assets/ directory, uploaded.
retropp::PaletteId loadPalette(retropp::Renderer& renderer, const std::string& name) {
    const std::vector<retropp::Rgba16> colors =
        retropp::slicePaletteImage(retropp::loadPngFromMemory(readAsset(name)));
    return renderer.uploadPalette(std::span<const retropp::Rgba16>(colors));
}

}  // namespace

int main() {
    retropp::EngineConfig::setActive(retropp::EngineConfig{
        .identity = {.organization = "Retro++", .application = "MenuKit Getting Started"},
        .window = {.title = "MenuKit — getting started"},
        .timing = retropp::TimingProfile{.tickPeriodNs = retropp::TickPeriodNs::Hz60},
        .enhancements = {.windowScale = 4},
    });

    retropp::SteadyClock clock;
    retropp::SdlPlatform platform;
    retropp::Renderer renderer{platform.device(), platform.sdlWindow()};
    retropp::RunLoop loop{clock};
    retropp::ActionMap actions{
        {Action::Confirm, {SDL_SCANCODE_Z, SDL_SCANCODE_SPACE, SDL_SCANCODE_RETURN, retropp::PadButton::FaceLabelA}},
        {Action::Cancel, {SDL_SCANCODE_X, SDL_SCANCODE_BACKSPACE, retropp::PadButton::FaceLabelB}},
    };
    actions.add(retropp::presets::directional(Action::Up, Action::Down, Action::Left, Action::Right));
    platform.actions(actions);

    // The border sheet: its cells are its pieces in the order of WindowKit::Piece, then the arrow — the
    // cursor and the arrows that say more rows lie above or below. Index 0 is transparent.
    const WindowKit::Border border{
        .sheet = renderer.loadAtlasFromMemory(readAsset("border.png"), retropp::AssetDimensions{.width = 8, .height = 8},
                                              retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
                                              /*count=*/static_cast<int>(WindowKit::kArrowSlot + 1),
                                              retropp::TransparentIndices::of({0})),
        .palette = loadPalette(renderer, "palettes/window.png"),
    };
    // One glyph sheet, three palettes: a label, the label under the cursor, and one that cannot be chosen.
    const TextKit::Font font{
        .sheet = renderer.loadAtlasFromMemory(readAsset("font.png"), retropp::AssetDimensions{.width = 8, .height = 8},
                                              retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
                                              /*count=*/static_cast<int>(kGlyphs.size()),
                                              retropp::TransparentIndices::of({0})),
        .glyphs = kGlyphs,
        .palette = loadPalette(renderer, "palettes/text.png"),
    };
    TextKit::Font selected = font;
    selected.palette = loadPalette(renderer, "palettes/text_selected.png");
    TextKit::Font dim = font;
    dim.palette = loadPalette(renderer, "palettes/text_dim.png");
    const MenuKit::Look look{.border = border, .font = font, .selected = selected, .disabled = dim};

    // Every label that can be chosen, in the order the items were made: an item's value is its place
    // here, so the status line can name what was chosen.
    std::vector<std::string> labels;

    // A menu in `box` holding one item per name, opening and closing over 200 ms and repeating a held
    // direction after 400 ms, then every 100 ms. Built after setActive, so 200 ms resolves at the
    // program's 60 Hz: 12 ticks.
    const auto menu = [&](retropp::IntRect box, std::initializer_list<std::string_view> names) {
        MenuKit::MenuConfig config{
            .box = box,
            .repeat = {.delay = std::chrono::milliseconds{400}, .every = std::chrono::milliseconds{100}},
        };
        config.animation.duration = std::chrono::milliseconds{200};
        for (const std::string_view name : names) {
            config.items.push_back(MenuKit::Item{.label = std::string(name), .value = labels.size()});
            labels.emplace_back(name);
        }
        return config;
    };

    // The menus, declared once.
    MenuKit::MenuConfig colors = menu({.x = 0, .y = 64, .width = 160, .height = 32},
                                      {"RED", "GREEN", "BLUE", "GOLD", "PINK", "GRAY"});
    colors.columns = 3;
    MenuKit::MenuConfig numbers = menu({.x = 40, .y = 48, .width = 80, .height = 48},
                                       {"ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE",
                                        "TEN", "ELEVEN", "TWELVE"});
    numbers.rows = 4;
    MenuKit::MenuConfig letters = menu({.x = 56, .y = 56, .width = 88, .height = 40},
                                       {"ALPHA", "BRAVO", "CHARLIE", "DELTA", "ECHO", "FOXTROT", "GOLF", "HOTEL"});
    letters.rows = 3;
    letters.overflow = MenuKit::Overflow::Scroll;
    letters.wraps = false;

    MenuKit::MenuConfig root = menu({.x = 8, .y = 24, .width = 80, .height = 56}, {"PLAY", "LOAD"});
    root.closesOnCancel = false;
    root.items[1].enabled = false;
    root.items.push_back(MenuKit::Item{.label = "COLORS", .opens = std::move(colors)});
    root.items.push_back(MenuKit::Item{.label = "NUMBERS", .opens = std::move(numbers)});
    root.items.push_back(MenuKit::Item{.label = "LETTERS", .opens = std::move(letters)});

    // The program's state: the open menus, and what the last tick reported.
    MenuKit::Menu open;
    open.open(root);
    std::string status;
    loop.simTick([&](const retropp::InputState& input) {
        const MenuKit::Step step = open.advance(
            root, MenuKit::readMenuInput(input, {.up = Action::Up, .down = Action::Down, .left = Action::Left,
                                                 .right = Action::Right, .confirm = Action::Confirm,
                                                 .cancel = Action::Cancel}));
        if (step.chosen) {
            status = "CHOSE " + labels[step.chosen->id];
        } else if (step.refused) {
            status = "REFUSED";
        } else if (step.cancelled) {
            status = "CANCELLED";
        }
    });

    retropp::FrameDrawState frame;
    loop.renderLoop([&] {
        frame.layers = {LayerKit::SpriteLayer("status", 0, {TextKit::Text("status", status, {.x = 8, .y = 8}, font)})};
        for (retropp::DrawLayer& layer : MenuKit::MenuWindows("menu", 1, root, open, look)) {
            frame.layers.push_back(std::move(layer));
        }
        renderer.renderFrame(frame);
    });

    retropp::WindowedHost{loop, platform}.run();
    return 0;
}
