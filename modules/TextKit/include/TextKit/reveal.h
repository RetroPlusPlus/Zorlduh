// Reveal — how much of a passage has been typed out so far.
//
//     TextKit::Reveal reveal;                                     // in the game's state
//     reveal.advance();                                           // once per tick
//     TextKit::Text("message", message, at, font, {.shown = reveal.shown()});   // each frame
//
// The header includes no draw type, so a game's simulation can hold a Reveal.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace TextKit {

// A typewriter: how many characters of a passage are shown so far. The game advances it once per
// tick and hands `shown()` to Text; `done(length)` says the whole passage is out; `skip` shows all of
// it at once. A new passage is a new Reveal.
//
// Every character counts, spaces and '\n' included, the way Text counts them. A `ticksPerGlyph` of 0
// is read as 1: one character per advance.
struct Reveal {
    std::uint32_t ticksPerGlyph = 2;   // the ticks each character takes to appear

    // One tick more.
    void advance() noexcept { ++ticks_; }

    // Shows the whole passage from here on: `shown()` is std::string_view::npos.
    void skip() noexcept { skipped_ = true; }

    // The characters shown: the ticks advanced divided by `ticksPerGlyph`, or npos after `skip`.
    [[nodiscard]] std::size_t shown() const noexcept {
        if (skipped_) {
            return std::string_view::npos;
        }
        const std::uint64_t per = ticksPerGlyph == 0 ? 1 : ticksPerGlyph;
        return static_cast<std::size_t>(ticks_ / per);
    }

    // Whether a passage of `length` characters is all shown.
    [[nodiscard]] bool done(std::size_t length) const noexcept { return shown() >= length; }

private:
    std::uint64_t ticks_ = 0;
    bool skipped_ = false;
};

}  // namespace TextKit
