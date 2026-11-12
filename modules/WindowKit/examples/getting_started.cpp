// A complete program built on the WindowKit module: a window at the top with its own size written
// inside it, which the arrow keys or a pad's directions grow and shrink a pixel at a time, and a
// message box along the bottom with a three-page message typed out a character at a time. Space,
// Return or a pad's A button shows the page at once, turns it, and closes the message after the last;
// pressed again, it opens the message again.
//
// The border is assets/border.png, a sheet of nine box pieces and an arrow; the font is
// assets/font.png; their colors are the palette images in assets/palettes/. docs/getting-started.md
// walks through it part by part.

#include <algorithm>
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
#include "retropp/windowed_host.h"

#include "TextKit/font.h"
#include "TextKit/text.h"
#include "WindowKit/border.h"
#include "WindowKit/dialog.h"
#include "WindowKit/dialog_box.h"
#include "WindowKit/window.h"

namespace {

enum class Action : std::uint8_t { Wider, Narrower, Taller, Shorter, Next };

// The characters of assets/font.png, one per cell, in the sheet's read order. The sheet's last row is
// padded with blank cells, which are not loaded.
constexpr std::string_view kGlyphs = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?'-:";

// Three pages in the message box's room: 18 characters by 4 lines.
constexpr std::string_view kMessage =
    "A message box wraps its text to the room inside its border and pages it. "
    "Each page types out, and an arrow blinks while more follows. "
    "Press again to turn the page, or to close it after the last.";

// Where the two windows sit on the screen.
constexpr retropp::IntRect kMessageBox{.x = 0, .y = 96, .width = 160, .height = 48};
constexpr retropp::Vec2i kResizableAt{.x = 8, .y = 8};

// The bytes of a file under this example's assets/ directory.
std::vector<std::uint8_t> readAsset(const std::string& name) {
    std::ifstream in(std::string(WINDOWKIT_EXAMPLE_ASSETS) + "/" + name, std::ios::binary);
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
        .identity = {.organization = "Retro++", .application = "WindowKit Getting Started"},
        .window = {.title = "WindowKit — getting started"},
        .timing = retropp::TimingProfile{.tickPeriodNs = retropp::TickPeriodNs::Hz60},
        .enhancements = {.windowScale = 4},
    });

    retropp::SteadyClock clock;
    retropp::SdlPlatform platform;
    retropp::Renderer renderer{platform.device(), platform.sdlWindow()};
    retropp::RunLoop loop{clock};
    platform.actions(retropp::ActionMap{
        {Action::Wider, {SDL_SCANCODE_RIGHT, retropp::PadButton::DpadRight}},
        {Action::Narrower, {SDL_SCANCODE_LEFT, retropp::PadButton::DpadLeft}},
        {Action::Taller, {SDL_SCANCODE_DOWN, retropp::PadButton::DpadDown}},
        {Action::Shorter, {SDL_SCANCODE_UP, retropp::PadButton::DpadUp}},
        {Action::Next, {SDL_SCANCODE_SPACE, SDL_SCANCODE_RETURN, retropp::PadButton::FaceLabelA}},
    });

    // The border sheet: its cells are its pieces in the order of WindowKit::Piece, then the arrow.
    // Index 0 is transparent, so what is under a rounded corner shows.
    const WindowKit::Border border{
        .sheet = renderer.loadAtlasFromMemory(readAsset("border.png"), retropp::AssetDimensions{.width = 8, .height = 8},
                                              retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
                                              /*count=*/static_cast<int>(WindowKit::kArrowSlot + 1),
                                              retropp::TransparentIndices::of({0})),
        .palette = loadPalette(renderer, "palettes/window.png"),
    };
    const TextKit::Font font{
        .sheet = renderer.loadAtlasFromMemory(readAsset("font.png"), retropp::AssetDimensions{.width = 8, .height = 8},
                                              retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
                                              /*count=*/static_cast<int>(kGlyphs.size()),
                                              retropp::TransparentIndices::of({0})),
        .glyphs = kGlyphs,
        .palette = loadPalette(renderer, "palettes/text.png"),
    };

    // The room the message has: the message box with the border's thickness taken off each side.
    const retropp::IntRect messageRoom = WindowKit::inside(kMessageBox, border);

    // The program's state: the resizable window's size, and the message being read.
    retropp::PixelSize size{.width = 93, .height = 41};
    WindowKit::Dialog dialog;
    dialog.open(kMessage, {.width = messageRoom.width, .height = messageRoom.height}, font);
    loop.simTick([&](const retropp::InputState& input) {
        if (input.justPressed(Action::Wider)) {
            size.width = std::min(size.width + 1, 160 - 2 * kResizableAt.x);
        }
        if (input.justPressed(Action::Narrower)) {
            size.width = std::max(size.width - 1, 0);
        }
        if (input.justPressed(Action::Taller)) {
            size.height = std::min(size.height + 1, kMessageBox.y - 2 * kResizableAt.y);
        }
        if (input.justPressed(Action::Shorter)) {
            size.height = std::max(size.height - 1, 0);
        }
        const bool next = input.justPressed(Action::Next);
        if (!dialog.showing()) {
            if (next) {
                dialog.open(kMessage, {.width = messageRoom.width, .height = messageRoom.height}, font);
            }
            return;
        }
        dialog.advance(next);
    });

    retropp::FrameDrawState frame;
    loop.renderLoop([&] {
        const retropp::IntRect box{.x = kResizableAt.x, .y = kResizableAt.y, .width = size.width, .height = size.height};
        const retropp::IntRect room = WindowKit::inside(box, border);
        const std::string label = std::to_string(size.width) + " X " + std::to_string(size.height);
        frame.layers = {
            WindowKit::Window("resizable", 0, box, border,
                              {TextKit::Text("resizable:size", label, {.x = room.x, .y = room.y}, font)}),
            WindowKit::DialogBox("message", 1, kMessageBox, border, font, dialog),
        };
        renderer.renderFrame(frame);
    });

    retropp::WindowedHost{loop, platform}.run();
    return 0;
}
