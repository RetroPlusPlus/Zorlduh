// Zorlduh's entry point. It activates the game's engine configuration, creates the four engine
// objects, and runs the loop until the window is closed. Each displayed frame submits an empty
// FrameDrawState: the window shows the viewport cleared, with nothing on it.

#include "retropp/clock.h"
#include "retropp/draw_state.h"
#include "retropp/engine_config.h"
#include "retropp/renderer.h"
#include "retropp/run_loop.h"
#include "retropp/sdl_platform.h"
#include "retropp/windowed_host.h"

#include "config.h"

int main() {
    retropp::EngineConfig::setActive(zorlduh::engineConfig());

    retropp::SteadyClock clock;
    retropp::SdlPlatform platform;
    retropp::Renderer renderer{platform.device(), platform.sdlWindow()};
    retropp::RunLoop loop{clock};

    const retropp::FrameDrawState frame;
    loop.renderLoop([&]() { renderer.renderFrame(frame); });

    retropp::WindowedHost{loop, platform}.run();
    return 0;
}
