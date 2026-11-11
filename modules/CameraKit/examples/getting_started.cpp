// A complete program built on the CameraKit module: a map twice the screen in both directions, a
// marker steered with the arrow keys, WASD or a d-pad, and a camera that follows it and stops at the
// map's edges. X (or the pad's B) pans the camera to the map's center over a second; Return (or
// Start) pans it back to the marker.
//
// docs/getting-started.md walks through it part by part.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "retropp/clock.h"
#include "retropp/draw_state.h"
#include "retropp/engine_config.h"
#include "retropp/geometry.h"
#include "retropp/input.h"
#include "retropp/input_actions.h"
#include "retropp/palette.h"
#include "retropp/renderer.h"
#include "retropp/run_loop.h"
#include "retropp/sdl_platform.h"
#include "retropp/timing.h"
#include "retropp/windowed_host.h"

#include "CameraKit/camera.h"
#include "CameraKit/laws.h"

namespace {

enum class Action : std::uint8_t { Up, Down, Left, Right, PanToCenter, PanBack };

// The map: 40×36 cells of 8 px, twice the Game Boy Color screen each way.
constexpr int kCell = 8;
constexpr int kMapWidth = 40;
constexpr int kMapHeight = 36;
constexpr retropp::IntRect kMapBounds{.x = 0, .y = 0, .width = kMapWidth * kCell, .height = kMapHeight * kCell};

// The whole screen is the view: nothing sits above or beside the map.
constexpr CameraKit::View kView{.size = {.width = 160, .height = 144}};

// The art: one sheet of three 8×8 tiles (floor, wall, the marker) and one palette.
struct Art {
    retropp::AtlasId tiles{};
    retropp::PaletteId palette{};
};

Art uploadArt(retropp::Renderer& renderer) {
    std::array<std::uint8_t, 24 * 8> sheet{};
    for (int y = 0; y < kCell; ++y) {
        for (int x = 0; x < kCell; ++x) {
            const bool rim = x == 0 || y == 0 || x == kCell - 1 || y == kCell - 1;
            const bool dot = (x + y) % 4 == 0;
            sheet[static_cast<std::size_t>(y) * 24 + x] = dot ? 2 : 1;                 // floor
            sheet[static_cast<std::size_t>(y) * 24 + kCell + x] = rim ? 3 : 2;         // wall
            sheet[static_cast<std::size_t>(y) * 24 + 2 * kCell + x] = rim ? 3 : 0;     // the marker
        }
    }
    const std::array<retropp::Rgba8, 4> colors{
        {{20, 24, 40}, {60, 90, 70}, {110, 140, 120}, {240, 230, 200}}};
    return Art{.tiles = renderer.uploadAtlas(sheet.data(), 24, 8).atlasId,
               .palette = renderer.uploadPalette(std::span<const retropp::Rgba8>(colors))};
}

// The map's cells: wall round the edge and on a grid of pillars, floor elsewhere.
std::vector<retropp::TileCell> MapCells(const Art& art) {
    std::vector<retropp::TileCell> cells;
    cells.reserve(static_cast<std::size_t>(kMapWidth) * kMapHeight);
    for (int y = 0; y < kMapHeight; ++y) {
        for (int x = 0; x < kMapWidth; ++x) {
            const bool edge = x == 0 || y == 0 || x == kMapWidth - 1 || y == kMapHeight - 1;
            const bool pillar = x % 8 == 4 && y % 8 == 4;
            cells.push_back(retropp::TileCell{.atlas = art.tiles,
                                              .tile = static_cast<std::uint16_t>(edge || pillar ? 1 : 0),
                                              .palette = art.palette});
        }
    }
    return cells;
}

}  // namespace

int main() {
    retropp::EngineConfig::setActive(retropp::EngineConfig{
        .identity = {.organization = "Retro++", .application = "CameraKit Getting Started"},
        .window = {.title = "CameraKit — getting started"},
        .timing = retropp::TimingProfile{.tickPeriodNs = retropp::TickPeriodNs::Hz60},
        .enhancements = {.windowScale = 4},
    });

    retropp::SteadyClock clock;
    retropp::SdlPlatform platform;
    retropp::Renderer renderer{platform.device(), platform.sdlWindow()};
    retropp::RunLoop loop{clock};
    retropp::ActionMap actions{
        {Action::PanToCenter, {SDL_SCANCODE_X, retropp::PadButton::FaceLabelB}},
        {Action::PanBack, {SDL_SCANCODE_RETURN, retropp::PadButton::Start}},
    };
    actions.add(retropp::presets::directional(Action::Up, Action::Down, Action::Left, Action::Right));
    platform.actions(actions);

    const Art art = uploadArt(renderer);
    const std::vector<retropp::TileCell> mapCells = MapCells(art);

    // The marker's top-left, kept inside the wall ring; the camera looks at its center.
    retropp::Vec2i marker{.x = 76, .y = 68};
    CameraKit::Camera camera;
    const auto markerCenter = [&] { return retropp::Vec2i{.x = marker.x + kCell / 2, .y = marker.y + kCell / 2}; };
    camera.place(CameraKit::follow(kMapBounds, kView.size, markerCenter()));

    loop.simTick([&](const retropp::InputState& input) {
        const int dx = (input.isHeld(retropp::actionId(Action::Right)) ? 1 : 0) -
                       (input.isHeld(retropp::actionId(Action::Left)) ? 1 : 0);
        const int dy = (input.isHeld(retropp::actionId(Action::Down)) ? 1 : 0) -
                       (input.isHeld(retropp::actionId(Action::Up)) ? 1 : 0);
        marker.x = std::clamp(marker.x + dx, kCell, kMapBounds.width - 2 * kCell);
        marker.y = std::clamp(marker.y + dy, kCell, kMapBounds.height - 2 * kCell);

        if (input.justPressed(retropp::actionId(Action::PanToCenter))) {
            camera.pan(CameraKit::follow(kMapBounds, kView.size,
                                         {.x = kMapBounds.width / 2, .y = kMapBounds.height / 2}),
                       60);
        }
        if (input.justPressed(retropp::actionId(Action::PanBack))) {
            camera.pan(CameraKit::follow(kMapBounds, kView.size, markerCenter()), 60);
        }

        // While a pan runs it owns the camera; otherwise the law does.
        if (camera.panning()) {
            camera.advance();
        } else {
            camera.place(CameraKit::follow(kMapBounds, kView.size, markerCenter()));
        }
    });

    retropp::FrameDrawState frame;
    std::vector<retropp::Sprite> sprites;
    loop.renderLoop([&] {
        sprites = {retropp::Sprite{.key = "marker",
                                   .x = marker.x,
                                   .y = marker.y,
                                   .size = {.width = kCell, .height = kCell},
                                   .atlas = art.tiles,
                                   .tile = 2,
                                   .palette = art.palette}};
        frame.layers = {
            retropp::DrawLayer{.key = "map",
                               .z = 0,
                               .size = retropp::Renderer::defaultViewport.size(),
                               .scroll = camera.scroll(kView),
                               .content = retropp::TileContent{.widthInTiles = kMapWidth,
                                                               .heightInTiles = kMapHeight,
                                                               .cells = mapCells,
                                                               .wrap = retropp::TileWrap::Blank}},
            retropp::DrawLayer{.key = "marker",
                               .z = 10,
                               .size = retropp::Renderer::defaultViewport.size(),
                               .scroll = camera.scroll(kView),
                               .content = retropp::SpriteContent{.sprites = sprites}},
        };
        renderer.renderFrame(frame);
    });

    retropp::WindowedHost{loop, platform}.run();
    return 0;
}
