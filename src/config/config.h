#pragma once

#include "retropp/engine_config.h"

namespace zorlduh {

// The game's startup configuration: its identity (organization "Retro++", application "Zorlduh"),
// the window title, the Game Boy Color viewport (160×144), a 60 Hz tick with no CPU model, and a
// window four times the viewport. The host passes it to retropp::EngineConfig::setActive once,
// before constructing any engine object; the identity also names the per-user save directory.
[[nodiscard]] retropp::EngineConfig engineConfig();

}  // namespace zorlduh
