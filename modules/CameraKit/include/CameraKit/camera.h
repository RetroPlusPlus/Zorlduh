// A camera: where the screen looks, in world pixels, and the scroll that puts it on a layer.
//
//     CameraKit::Camera camera;
//     camera.place(CameraKit::follow(mapBounds, kView.size, playerCenter));   // each tick
//     LayerKit::TileLayer("map", 0, cells, w, h, {.scroll = camera.scroll(kView)});
//
//     camera.pan({320, 0}, 60);   // then camera.advance() each tick; panning() says when it arrives
//
// The camera is a plain value a game keeps in its state. Where it should be is a law's decision
// (CameraKit/laws.h); the camera only goes there, at once with `place` or over a number of ticks with
// `pan`.
#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>

#include "retropp/draw_state.h"
#include "retropp/geometry.h"
#include "retropp/timing.h"
#include "retropp/tween.h"

namespace CameraKit {

// The screen the camera shows: its size, and where it sits on a layer. A game with a 16 px HUD strip
// across the top of a 160×144 screen has a view of {.size = {160, 128}, .origin = {0, 16}}.
struct View {
    retropp::PixelSize size{};
    retropp::Vec2i origin{};

    [[nodiscard]] constexpr bool operator==(const View&) const noexcept = default;
};

class Camera {
public:
    // A camera at (0, 0).
    Camera() = default;

    // A camera showing world pixel `position` at the view's top-left.
    explicit Camera(retropp::Vec2i position) noexcept : position_(position) {}

    // Be at `position` now. A pan in progress is dropped.
    void place(retropp::Vec2i position) noexcept {
        position_ = position;
        pan_.reset();
    }

    // Move from here to `to` in a straight line over `ticks` ticks, arriving on the last; `advance`
    // takes each tick. With no ticks, the camera is placed at `to` at once.
    void pan(retropp::Vec2i to, std::uint32_t ticks) {
        if (ticks == 0) {
            place(to);
            return;
        }
        const retropp::TimingProfile timing = retropp::TweenPlayer<retropp::Vec2>::defaultTiming;
        pan_ = Pan{
            .path = retropp::Tween<retropp::Vec2>::of(toVec2(position_), toVec2(to),
                                                      ticks * timing.tickPeriod(), retropp::Easing::Linear),
            .timing = timing,
            .to = to,
            .ticks = ticks,
        };
    }

    // One tick of a pan. Nothing happens when the camera is not panning.
    void advance() noexcept {
        if (!pan_) {
            return;
        }
        ++pan_->elapsed;
        if (pan_->elapsed >= pan_->ticks) {
            position_ = pan_->to;
            pan_.reset();
            return;
        }
        const retropp::Vec2 at = retropp::sampleTweenValue(pan_->path, pan_->elapsed, pan_->timing,
                                                           retropp::PlaybackMode::single());
        position_ = retropp::Vec2i{.x = static_cast<int>(std::lround(at.x)),
                                   .y = static_cast<int>(std::lround(at.y))};
    }

    // The world pixel shown at the view's top-left.
    [[nodiscard]] retropp::Vec2i position() const noexcept { return position_; }

    // Whether a pan is in progress.
    [[nodiscard]] bool panning() const noexcept { return pan_.has_value(); }

    // The scroll a layer takes to show the camera's position at `view.origin`: the position less the
    // origin. For a layer whose pixels are world pixels, screen pixel (x, y) then shows world pixel
    // (x + scroll.x, y + scroll.y).
    [[nodiscard]] retropp::LayerScroll scroll(const View& view) const noexcept {
        return retropp::LayerScroll{.x = position_.x - view.origin.x, .y = position_.y - view.origin.y};
    }

private:
    struct Pan {
        retropp::Tween<retropp::Vec2> path;
        retropp::TimingProfile timing;
        retropp::Vec2i to;
        std::uint32_t ticks = 0;
        std::uint32_t elapsed = 0;
    };

    [[nodiscard]] static retropp::Vec2 toVec2(retropp::Vec2i v) noexcept {
        return retropp::Vec2{.x = static_cast<float>(v.x), .y = static_cast<float>(v.y)};
    }

    retropp::Vec2i position_{};
    std::optional<Pan> pan_;
};

}  // namespace CameraKit
