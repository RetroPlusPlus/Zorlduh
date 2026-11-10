#include "config.h"

namespace zorlduh {

retropp::EngineConfig engineConfig() {
    return retropp::EngineConfig{
        .identity = {.organization = "Retro++", .application = "Zorlduh"},
        .window = {.title = "Zorlduh"},
        .viewport = retropp::ViewportResolution::GameBoyColor,
        .timing = retropp::TimingProfile{.tickPeriodNs = retropp::TickPeriodNs::Hz60},
        .enhancements = {.windowScale = 4},
    };
}

}  // namespace zorlduh
