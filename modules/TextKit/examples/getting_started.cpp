// A complete program built on the TextKit module: a title centered across the screen with a drop
// shadow, and beneath it a passage typed out a character at a time. Space, Return or a pad's A button
// shows the rest of the passage at once; pressed again, it starts the next passage.
//
// The font is assets/font.png, a sheet of 16×16 glyphs, and its colors are the palette images in
// assets/palettes/. docs/getting-started.md walks through it part by part.

#include <array>
#include <cstddef>
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
#include "TextKit/reveal.h"
#include "TextKit/text.h"

namespace {

enum class Action : std::uint8_t { Next };

// The characters of assets/font.png, one per cell, in the sheet's read order. The cell after 'Z' is
// blank: it is listed as a space so the cells after it keep their places. The sheet's last three
// cells are not characters, and are not loaded.
constexpr std::string_view kGlyphs = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ _.-!?<>()*&^$#@%+[]\"'";

// The passages, typed out one after another. The font has uppercase letters only, so the lowercase
// ones are drawn with their uppercase glyphs.
constexpr std::array<std::string_view, 3> kPassages{
    "Each glyph is typed out in turn.",
    "Press Space to show the rest at once.",
    "Press again for the next one!",
};

constexpr int kScreenWidth = 160;
constexpr int kPassageColumns = 9;   // 9 glyphs of 16 px: 144 px, with 8 px either side

// The bytes of a file under this example's assets/ directory.
std::vector<std::uint8_t> readAsset(const std::string& name) {
    std::ifstream in(std::string(TEXTKIT_EXAMPLE_ASSETS) + "/" + name, std::ios::binary);
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
        .identity = {.organization = "Retro++", .application = "TextKit Getting Started"},
        .window = {.title = "TextKit — getting started"},
        .timing = retropp::TimingProfile{.tickPeriodNs = retropp::TickPeriodNs::Hz60},
        .enhancements = {.windowScale = 4},
    });

    retropp::SteadyClock clock;
    retropp::SdlPlatform platform;
    retropp::Renderer renderer{platform.device(), platform.sdlWindow()};
    retropp::RunLoop loop{clock};
    platform.actions(retropp::ActionMap{
        {Action::Next, {SDL_SCANCODE_SPACE, SDL_SCANCODE_RETURN, retropp::PadButton::FaceLabelA}},
    });

    // The glyph sheet: 16×16 cells, one slot per character of kGlyphs. Index 0 is transparent, so only
    // the glyphs draw.
    const retropp::AtlasManifest sheet = renderer.loadAtlasFromMemory(
        readAsset("font.png"), retropp::AssetDimensions{.width = 16, .height = 16},
        retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
        /*count=*/static_cast<int>(kGlyphs.size()), retropp::TransparentIndices::of({0}));
    const retropp::PaletteId white = loadPalette(renderer, "palettes/text_white.png");
    const retropp::PaletteId gold = loadPalette(renderer, "palettes/text_gold.png");
    const retropp::PaletteId gray = loadPalette(renderer, "palettes/text_gray.png");

    // Two fonts over the one sheet: the title in gold over a gray shadow, the passages in white.
    const TextKit::Font title{.sheet = sheet, .glyphs = kGlyphs, .palette = gold, .shadow = gray,
                              .advance = {.x = 16, .y = 16}};
    const TextKit::Font body{.sheet = sheet, .glyphs = kGlyphs, .palette = white,
                             .advance = {.x = 16, .y = 16}};

    // Each passage wrapped once, up front: the sprites are built from these strings every frame.
    std::array<std::string, kPassages.size()> passages;
    for (std::size_t i = 0; i < kPassages.size(); ++i) {
        passages[i] = TextKit::wrap(kPassages[i], kPassageColumns);
    }

    // The program's state: which passage, and how much of it is typed out.
    std::size_t passage = 0;
    TextKit::Reveal reveal;
    loop.simTick([&](const retropp::InputState& input) {
        reveal.advance();
        if (input.justPressed(Action::Next)) {
            if (!reveal.done(passages[passage].size())) {
                reveal.skip();
            } else {
                passage = (passage + 1) % passages.size();
                reveal = TextKit::Reveal{};
            }
        }
    });

    retropp::FrameDrawState frame;
    std::vector<retropp::Sprite> sprites;
    loop.renderLoop([&] {
        sprites = TextKit::Text("title", "TextKit", {.x = 0, .y = 8}, title,
                                {.align = TextKit::Align::Center, .width = kScreenWidth});
        const std::vector<retropp::Sprite> typed =
            TextKit::Text("passage", passages[passage], {.x = 8, .y = 40}, body, {.shown = reveal.shown()});
        sprites.insert(sprites.end(), typed.begin(), typed.end());
        frame.layers = {
            retropp::DrawLayer{.key = "text",
                               .z = 0,
                               .size = retropp::Renderer::defaultViewport.size(),
                               .content = retropp::SpriteContent{.sprites = sprites}},
        };
        renderer.renderFrame(frame);
    });

    retropp::WindowedHost{loop, platform}.run();
    return 0;
}
