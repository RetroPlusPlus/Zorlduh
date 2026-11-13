// The styles a window opens and closes in: each a function from where its animation is to what a box or
// a layer takes, and Animate, which applies one.
//
//     const WindowKit::Animated drawn = WindowKit::Animate(box, state.menu, WindowKit::Style::grow());
//     frame.layers.push_back(WindowKit::Window("menu", kMenuZ, drawn.box, border, {}, drawn.options));
//
// Window has an overload that does this itself (window.h). At progress 1 every style is its identity:
// Grow gives the box, Clip no regions, Squash the identity transform, Fade 1 and Slide no scroll.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "retropp/draw_state.h"
#include "retropp/geometry.h"
#include "retropp/transform.h"

#include "LayerKit/layer_options.h"

#include "WindowKit/animation.h"

namespace WindowKit {

// The rectangle `box` has grown to: from `origin` — a point in the layer's pixels — at progress 0 to
// `box` at 1. With p the animation's progress, x is origin.x + (box.x − origin.x)·p and the width
// box.width·p, each rounded to the nearest pixel, and likewise down. At 1 it is `box`; at 0 a rectangle
// of no size at `origin`, which Box draws as nothing.
[[nodiscard]] inline retropp::IntRect Grow(retropp::IntRect box, const Animation& animation,
                                           retropp::Vec2i origin) noexcept {
    const float p = animation.progress();
    const auto scaled = [p](int length) { return static_cast<int>(std::lround(static_cast<float>(length) * p)); };
    return retropp::IntRect{
        .x = origin.x + scaled(box.x - origin.x),
        .y = origin.y + scaled(box.y - origin.y),
        .width = scaled(box.width),
        .height = scaled(box.height),
    };
}

// The middle of `box`: {box.x + box.width / 2, box.y + box.height / 2}, rounded toward its top-left.
[[nodiscard]] inline retropp::Vec2i middle(retropp::IntRect box) noexcept {
    return retropp::Vec2i{.x = box.x + box.width / 2, .y = box.y + box.height / 2};
}

// The rectangle `box` has grown to from its middle.
[[nodiscard]] inline retropp::IntRect Grow(retropp::IntRect box, const Animation& animation) noexcept {
    return Grow(box, animation, middle(box));
}

// Regions that leave only the rectangle Grow gives drawn, the rest of the layer see-through: none once
// the window is open; at progress 0 one region keyed "clip" with no shape and a Transparency effect, the
// whole layer see-through; between them retropp::stencil over the grown rectangle, TransparentOutside at
// Layer scope, each region keyed "clip". The box is drawn whole and shown through the rectangle.
[[nodiscard]] inline std::vector<retropp::Region> Clip(retropp::IntRect box, const Animation& animation,
                                                       retropp::Vec2i origin) {
    if (animation.phase() == Phase::Open) {
        return {};
    }
    if (animation.progress() <= 0.0f) {
        return {retropp::Region{
            .key = "clip",
            .effects = {retropp::ScreenSpaceEffect{.kind = retropp::ScreenSpaceEffectKind::Transparency}},
        }};
    }
    const retropp::IntRect shown = Grow(box, animation, origin);
    std::vector<retropp::Region> regions = retropp::stencil(
        retropp::ShapePoints::rectangle(retropp::Point{static_cast<float>(shown.x), static_cast<float>(shown.y)},
                                        static_cast<float>(shown.width), static_cast<float>(shown.height)),
        retropp::StencilMode::TransparentOutside, 0.0f, retropp::ScreenSpaceEffectScope::Layer);
    for (retropp::Region& region : regions) {
        region.key = "clip";
    }
    return regions;
}

// Clip from the box's middle.
[[nodiscard]] inline std::vector<retropp::Region> Clip(retropp::IntRect box, const Animation& animation) {
    return Clip(box, animation, middle(box));
}

// The layer scaled about the middle of `box` along `axis` to the animation's progress: a vertical squash
// presses the box to a line across its middle and opens it back out. The scale is never below one pixel
// of the box on that axis, so the transform always has an inverse; the other axis keeps a scale of 1.
// The identity once the window is open.
[[nodiscard]] inline retropp::Transform Squash(retropp::IntRect box, const Animation& animation,
                                               retropp::Axis axis = retropp::Axis::Vertical) noexcept {
    if (animation.phase() == Phase::Open) {
        return retropp::Transform::identity();
    }
    const int extent = axis == retropp::Axis::Vertical ? box.height : box.width;
    const float least = extent > 0 ? 1.0f / static_cast<float>(extent) : 0.0f;
    const float scale = std::max(animation.progress(), least);
    const float centerX = static_cast<float>(box.x) + static_cast<float>(box.width) / 2.0f;
    const float centerY = static_cast<float>(box.y) + static_cast<float>(box.height) / 2.0f;
    return axis == retropp::Axis::Vertical ? retropp::Transform::scale(1.0f, scale, centerX, centerY)
                                           : retropp::Transform::scale(scale, 1.0f, centerX, centerY);
}

// The layer's alpha: the animation's progress.
[[nodiscard]] inline float Fade(const Animation& animation) noexcept { return animation.progress(); }

// The layer's scroll that shows the top-left of `box` at `from` at progress 0 and in its own place at 1:
// (box − from)·(1 − p) on each axis, rounded to the nearest pixel. A layer's scroll moves its content by
// the negative, so the box travels from `from` to where it is.
[[nodiscard]] inline retropp::LayerScroll Slide(retropp::IntRect box, const Animation& animation,
                                                retropp::Vec2i from) noexcept {
    const float rest = 1.0f - animation.progress();
    return retropp::LayerScroll{
        .x = static_cast<int>(std::lround(static_cast<float>(box.x - from.x) * rest)),
        .y = static_cast<int>(std::lround(static_cast<float>(box.y - from.y) * rest)),
    };
}

// Which style a window is drawn in while its animation moves, and the style's own parameters.
// Style{} is grow from the box's middle. Each named constructor fills the fields its kind reads.
struct Style {
    enum class Kind : std::uint8_t { Pop, Grow, Clip, Squash, Fade, Slide };

    Kind kind = Kind::Grow;
    std::optional<retropp::Vec2i> origin;           // Grow, Clip: the point grown from; none is the box's middle
    retropp::Axis axis = retropp::Axis::Vertical;   // Squash: the axis pressed
    retropp::Vec2i from{};                          // Slide: where the box's top-left starts, in the layer's pixels

    // No style: the box as it is while it opens and closes.
    [[nodiscard]] static Style pop() noexcept { return Style{.kind = Kind::Pop}; }
    [[nodiscard]] static Style grow() noexcept { return Style{.kind = Kind::Grow}; }
    [[nodiscard]] static Style grow(retropp::Vec2i origin) noexcept {
        return Style{.kind = Kind::Grow, .origin = origin};
    }
    [[nodiscard]] static Style clip() noexcept { return Style{.kind = Kind::Clip}; }
    [[nodiscard]] static Style clip(retropp::Vec2i origin) noexcept {
        return Style{.kind = Kind::Clip, .origin = origin};
    }
    [[nodiscard]] static Style squash(retropp::Axis axis = retropp::Axis::Vertical) noexcept {
        return Style{.kind = Kind::Squash, .axis = axis};
    }
    [[nodiscard]] static Style fade() noexcept { return Style{.kind = Kind::Fade}; }
    [[nodiscard]] static Style slide(retropp::Vec2i from) noexcept { return Style{.kind = Kind::Slide, .from = from}; }
};

// A box and a layer's options with a style applied.
struct Animated {
    retropp::IntRect box;
    LayerKit::LayerOptions options;
};

// `box` and `options` with `style` applied for where `animation` is. Pop returns them as they are; Grow
// replaces the box with the grown one; Clip appends its regions after the caller's; Squash puts its
// transform first and the caller's after it; Fade multiplies the caller's alpha; Slide adds to the
// caller's scroll. While the window is open every style returns them as they are.
[[nodiscard]] inline Animated Animate(retropp::IntRect box, const Animation& animation, const Style& style,
                                      LayerKit::LayerOptions options = {}) {
    if (animation.phase() == Phase::Open) {
        return Animated{.box = box, .options = std::move(options)};
    }
    const retropp::Vec2i origin = style.origin.value_or(middle(box));
    switch (style.kind) {
        case Style::Kind::Pop:
            break;
        case Style::Kind::Grow:
            box = Grow(box, animation, origin);
            break;
        case Style::Kind::Clip: {
            std::vector<retropp::Region> clip = Clip(box, animation, origin);
            options.regions.insert(options.regions.end(), clip.begin(), clip.end());
            break;
        }
        case Style::Kind::Squash:
            options.transform = Squash(box, animation, style.axis).then(options.transform);
            break;
        case Style::Kind::Fade:
            options.alpha *= Fade(animation);
            break;
        case Style::Kind::Slide: {
            const retropp::LayerScroll slide = Slide(box, animation, style.from);
            options.scroll.x += slide.x;
            options.scroll.y += slide.y;
            break;
        }
    }
    return Animated{.box = box, .options = std::move(options)};
}

}  // namespace WindowKit
