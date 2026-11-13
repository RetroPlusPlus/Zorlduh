// Animation — a window opening and closing over time.
//
//     WindowKit::Animation menu;                     // in the game's state
//     menu.duration = std::chrono::milliseconds{150};
//     menu.open();                                   // when the window is wanted
//     menu.advance();                                // once per tick, by whatever owns the window
//     WindowKit::Window("menu", kMenuZ, box, border, menu, WindowKit::Style::grow(), {...});   // each frame
//
// A Dialog holds one of its own and advances it from Dialog::advance. An Animation is a value a game's
// simulation holds; it names no draw type and returns none. The engine's retropp::Animation, a run of
// sprite frames, is another type; outside this namespace this one is written WindowKit::Animation.
#pragma once

#include <chrono>
#include <cstdint>

#include "retropp/timing.h"
#include "retropp/tween.h"

namespace WindowKit {

// What a window's animation is doing.
enum class Phase : std::uint8_t { Closed, Opening, Open, Closing };

// A window opening and closing over time: how long each takes, the curve it follows, and how far along
// it is. Whatever owns the window advances it once per tick.
//
// The length is a time or a count of ticks. A time resolves to ticks through `timing`, to the nearest
// tick. `timing` is retropp::TweenPlayer<float>::defaultTiming as it stands when the value is built,
// which EngineConfig::setActive sets to the game's own; build the animation after setActive runs. With
// no length the animation is instant: open() is open at once and close() closed at once.
struct Animation {
    std::chrono::nanoseconds duration{};  // how long an opening takes, and a closing, unless `ticks` is set
    std::uint32_t ticks = 0;              // the ticks an opening takes, and a closing; not 0, `duration` is not read
    retropp::Easing easing = retropp::Easing::InOutQuad;  // the curve progress follows, as a Tween's segment
    retropp::TimingProfile timing = retropp::TweenPlayer<float>::defaultTiming;  // resolves `duration` to ticks

    // Starts opening: from closed, or from as far as a closing has got. Open at once when the length is
    // 0. Nothing while open or opening.
    void open() noexcept {
        switch (phase_) {
            case Phase::Closed:
                elapsed_ = 0;
                break;
            case Phase::Closing:
                elapsed_ = elapsed_ >= length() ? 0 : length() - elapsed_;
                break;
            case Phase::Opening:
            case Phase::Open:
                return;
        }
        phase_ = Phase::Opening;
        settleIfDone();
    }

    // Starts closing: from open, or from as far as an opening has got. Closed at once when the length is
    // 0. Nothing while closed or closing.
    void close() noexcept {
        switch (phase_) {
            case Phase::Open:
                elapsed_ = 0;
                break;
            case Phase::Opening:
                elapsed_ = elapsed_ >= length() ? 0 : length() - elapsed_;
                break;
            case Phase::Closing:
            case Phase::Closed:
                return;
        }
        phase_ = Phase::Closing;
        settleIfDone();
    }

    // One tick: an opening or closing moves on, and is open or closed on the tick it reaches the length.
    // Nothing while open or closed.
    void advance() noexcept {
        if (!moving()) {
            return;
        }
        ++elapsed_;
        settleIfDone();
    }

    // Finishes the opening or closing in progress at once. Nothing while open or closed.
    void skip() noexcept {
        if (phase_ == Phase::Opening) {
            phase_ = Phase::Open;
        } else if (phase_ == Phase::Closing) {
            phase_ = Phase::Closed;
        }
        elapsed_ = 0;
    }

    [[nodiscard]] Phase phase() const noexcept { return phase_; }

    // Whether it is opening or closing.
    [[nodiscard]] bool moving() const noexcept { return phase_ == Phase::Opening || phase_ == Phase::Closing; }

    // How open the window is: 0 closed, 1 open, and between them the ticks gone over the length, shaped
    // by `easing`. A closing is an opening played backward, so a closing started partway through an
    // opening carries on from the same progress.
    [[nodiscard]] float progress() const noexcept {
        switch (phase_) {
            case Phase::Closed:
                return 0.0f;
            case Phase::Open:
                return 1.0f;
            case Phase::Opening:
                return retropp::ease(easing, fraction());
            case Phase::Closing:
                return retropp::ease(easing, 1.0f - fraction());
        }
        return 0.0f;
    }

    // The ticks an opening takes, and a closing: `ticks` when it is not 0, else `duration` through
    // `timing`. 0 is instant.
    [[nodiscard]] std::uint64_t length() const noexcept {
        return ticks != 0 ? ticks : timing.ticksForDuration(duration);
    }

private:
    [[nodiscard]] float fraction() const noexcept {
        const std::uint64_t total = length();
        return total == 0 ? 1.0f : static_cast<float>(elapsed_) / static_cast<float>(total);
    }

    void settleIfDone() noexcept {
        if (elapsed_ < length()) {
            return;
        }
        phase_ = phase_ == Phase::Opening ? Phase::Open : Phase::Closed;
        elapsed_ = 0;
    }

    Phase phase_ = Phase::Closed;
    std::uint64_t elapsed_ = 0;
};

}  // namespace WindowKit
