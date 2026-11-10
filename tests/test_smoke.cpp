// The build links the engine, and the engine accepts the game's startup configuration.

#include <gtest/gtest.h>

#include "retropp/engine_config.h"
#include "retropp/version.h"

#include "config.h"

namespace {

TEST(Smoke, EngineVersionIsNonEmpty) {
    EXPECT_FALSE(retropp::version().empty());
}

TEST(Smoke, SetActiveAcceptsTheGameConfig) {
    EXPECT_NO_THROW(retropp::EngineConfig::setActive(zorlduh::engineConfig()));
    EXPECT_EQ(retropp::EngineConfig::active.identity.organization, "Retro++");
    EXPECT_EQ(retropp::EngineConfig::active.identity.application, "Zorlduh");
}

TEST(Smoke, GameConfigIsTheGameBoyColorScreenAt60Hz) {
    const retropp::EngineConfig config = zorlduh::engineConfig();
    EXPECT_EQ(config.viewport.width, 160);
    EXPECT_EQ(config.viewport.height, 144);
    EXPECT_EQ(config.timing.tickPeriodNs, retropp::TickPeriodNs::Hz60);
    EXPECT_FALSE(config.timing.cpu.has_value());
    EXPECT_EQ(config.enhancements.windowScale, 4);
    EXPECT_EQ(config.window.title, "Zorlduh");
}

}  // namespace
