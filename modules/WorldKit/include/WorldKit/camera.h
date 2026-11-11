// Where in the world the screen is: which area, where the camera looks in it, and the scroll or fade
// to the next area when something goes out through an opening.
//
//     WorldKit::Camera camera{Area::Meadow};
//     camera.place(layout, playerBox);                                   // at the start
//
//     if (!camera.transitioning()) { /* move the player against camera.solids(layout) */ }
//     const WorldKit::Step step = camera.follow(layout, playerBox);       // each tick
//     player.place(player.position() + step.move);
//     if (step.entered) { /* the player is now in area *step.entered */ }
//
// A game keeps one Camera in its state. Each tick it hands the camera the box of the thing the screen
// follows, and applies the move the camera hands back: nothing while the thing walks on its own, the
// carry into the next area during a scroll, the jump to the entrance at the middle of a fade, and at the
// end of either the change into the next area's pixels. The camera holds a CameraKit::Camera and
// places it by the layout's camera law; the game never touches it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "retropp/draw_state.h"
#include "retropp/geometry.h"

#include "CameraKit/camera.h"

#include "WorldKit/area.h"
#include "WorldKit/solids.h"

namespace WorldKit {

// What one tick of following asks of the thing followed: how far to move it, and the area it has just
// entered, on the tick the camera's area changes.
struct Step {
    retropp::Vec2i move{};
    std::optional<int> entered{};
};

// An area on screen, and the scroll that places a layer of its pixels.
struct Shown {
    int area = 0;
    retropp::LayerScroll scroll{};
};

class Camera {
public:
    // A camera in `area`, looking at its top-left.
    explicit Camera(int area) noexcept : area_(area) {}

    // The area the camera is in. During a scroll it is still the area being left; it changes on the
    // scroll's last tick. During a fade it changes at the middle, when the screen is dark.
    [[nodiscard]] int area() const noexcept { return area_; }

    // Whether a scroll or a fade is running. A game moves nothing of its own while one is.
    [[nodiscard]] bool transitioning() const noexcept {
        return !std::holds_alternative<std::monostate>(transition_);
    }

    // What is solid in the current area.
    [[nodiscard]] AreaSolids solids(const Layout& layout) const noexcept {
        return AreaSolids{.layout = &layout, .area = area_};
    }

    // Looks at `box` at once, by the layout's camera law, with no transition: for placing the thing
    // followed at the start or after a jump. A transition in progress is dropped.
    void place(const Layout& layout, retropp::IntRect box) {
        transition_ = std::monostate{};
        camera_.place(layout.camera(detail::bounds(layout, area_), layout.view.size, center(box)));
        inside_ = spotHolding(layout, center(box));
    }

    // One tick. While nothing is running, the camera looks at `box` by the law, and the box's center
    // going out through an opening that leads somewhere starts that opening's transition: out through
    // a wall's gap, or into a spot from outside it. While a scroll runs, the camera pans to where the
    // law puts it in the next area and the box is carried `layout.carry` pixels in; on the last tick the
    // move also takes the box into the next area's pixels. While a fade runs, the screen darkens over
    // `layout.fadeTicks`, the box jumps to the entrance (just inside a wall's gap, or onto a spot) and
    // the next area becomes current, and the screen lightens over the same again.
    [[nodiscard]] Step follow(const Layout& layout, retropp::IntRect box) {
        if (auto* scroll = std::get_if<Scrolling>(&transition_)) {
            return scrolling(layout, *scroll);
        }
        if (auto* fade = std::get_if<Fading>(&transition_)) {
            return fading(layout, *fade, box);
        }

        const retropp::Vec2i focus = center(box);
        camera_.place(layout.camera(detail::bounds(layout, area_), layout.view.size, focus));

        const Area& here = layout.areas[static_cast<std::size_t>(area_)];
        const std::optional<int> nowInside = spotHolding(layout, focus);
        for (std::size_t i = 0; i < here.openings.size(); ++i) {
            const Opening& opening = here.openings[i];
            if (opening.leadsTo.area == kNoArea) {
                continue;
            }
            const bool through = detail::isEdge(opening)
                                     ? outThrough(layout, opening, focus)
                                     : nowInside == static_cast<int>(i) && inside_ != nowInside;
            if (through) {
                start(layout, opening, box);
                break;
            }
        }
        inside_ = nowInside;
        return Step{};
    }

    // The camera's position in the current area's pixels (in the area being left, during a scroll).
    [[nodiscard]] retropp::Vec2i position() const noexcept { return camera_.position(); }

    // The scroll for a layer of things placed in the current area's pixels.
    [[nodiscard]] retropp::LayerScroll scroll(const Layout& layout) const noexcept {
        return camera_.scroll(layout.view);
    }

    // The areas on screen with the scroll each one's layer takes: the current area, and during a
    // scroll the next one too, lined up with the opening being gone through.
    [[nodiscard]] std::vector<Shown> shown(const Layout& layout) const {
        std::vector<Shown> result{Shown{.area = area_, .scroll = scroll(layout)}};
        if (const auto* scroll = std::get_if<Scrolling>(&transition_)) {
            const retropp::LayerScroll here = result.front().scroll;
            result.push_back(Shown{.area = scroll->to,
                                   .scroll = {.x = here.x - scroll->origin.x, .y = here.y - scroll->origin.y}});
        }
        return result;
    }

    // How dark the screen is, 0 to 1: 0 except during a fade.
    [[nodiscard]] float fade(const Layout& layout) const noexcept {
        const auto* fade = std::get_if<Fading>(&transition_);
        if (fade == nullptr || layout.fadeTicks == 0) {
            return 0.0f;
        }
        const std::uint32_t half = layout.fadeTicks;
        const std::uint32_t from = fade->elapsed <= half ? fade->elapsed : 2 * half - fade->elapsed;
        return static_cast<float>(from) / static_cast<float>(half);
    }

private:
    // A scroll in progress: to which area, out through which wall, where that area's top-left sits in
    // this area's pixels, and how far along it is.
    struct Scrolling {
        int to = 0;
        Side side = Side::None;
        retropp::Vec2i origin{};
        std::uint32_t elapsed = 0;
        int carried = 0;
    };

    // A fade in progress: to which area and which of its openings, and how far along it is.
    struct Fading {
        int to = 0;
        int entrance = 0;
        std::uint32_t elapsed = 0;
        bool switched = false;
    };

    [[nodiscard]] static retropp::Vec2i center(retropp::IntRect box) noexcept {
        return retropp::Vec2i{.x = box.x + box.width / 2, .y = box.y + box.height / 2};
    }

    // The spot opening of the current area whose cells hold `point`, if any.
    [[nodiscard]] std::optional<int> spotHolding(const Layout& layout, retropp::Vec2i point) const noexcept {
        const Area& here = layout.areas[static_cast<std::size_t>(area_)];
        for (std::size_t i = 0; i < here.openings.size(); ++i) {
            if (detail::isSpot(here.openings[i]) &&
                detail::contains(detail::spotPixels(layout, here.openings[i]), point)) {
                return static_cast<int>(i);
            }
        }
        return std::nullopt;
    }

    // Whether `point` is past the wall `opening` is in, within its span.
    [[nodiscard]] bool outThrough(const Layout& layout, const Opening& opening,
                                  retropp::Vec2i point) const noexcept {
        const retropp::IntRect area = detail::bounds(layout, area_);
        const bool past = (opening.side == Side::East && point.x >= area.width) ||
                          (opening.side == Side::West && point.x < 0) ||
                          (opening.side == Side::South && point.y >= area.height) ||
                          (opening.side == Side::North && point.y < 0);
        const int along = detail::runsAcross(opening.side) ? point.x : point.y;
        return past && detail::holds(opening.span, detail::cellOf(along, layout.cellSize));
    }

    void start(const Layout& layout, const Opening& exit, retropp::IntRect box) {
        const int to = exit.leadsTo.area;
        const Area& next = layout.areas[static_cast<std::size_t>(to)];
        const int entrance = detail::findOpening(next, exit.leadsTo.opening);
        if (exit.transition == Transition::Fade) {
            transition_ = Fading{.to = to, .entrance = entrance};
            return;
        }

        // The next area sits against this one's wall, its entrance span level with the exit's.
        const Opening& into = next.openings[static_cast<std::size_t>(entrance)];
        const retropp::IntRect here = detail::bounds(layout, area_);
        const retropp::IntRect there = detail::bounds(layout, to);
        const int level = (exit.span.first - into.span.first) * layout.cellSize;
        retropp::Vec2i origin{};
        switch (exit.side) {
            case Side::East: origin = {.x = here.width, .y = level}; break;
            case Side::West: origin = {.x = -there.width, .y = level}; break;
            case Side::South: origin = {.x = level, .y = here.height}; break;
            case Side::North: origin = {.x = level, .y = -there.height}; break;
            case Side::None:
            default: break;
        }

        const retropp::Vec2i out = detail::outward(exit.side);
        const retropp::Vec2i landing{.x = center(box).x + out.x * layout.carry,
                                     .y = center(box).y + out.y * layout.carry};
        const retropp::IntRect nextHere{.x = origin.x, .y = origin.y, .width = there.width, .height = there.height};
        camera_.pan(layout.camera(nextHere, layout.view.size, landing), layout.scrollTicks);
        transition_ = Scrolling{.to = to, .side = exit.side, .origin = origin};
    }

    [[nodiscard]] Step scrolling(const Layout& layout, Scrolling& scroll) {
        ++scroll.elapsed;
        camera_.advance();
        const std::uint32_t ticks = layout.scrollTicks == 0 ? 1 : layout.scrollTicks;
        const int carriedNow = static_cast<int>(static_cast<std::int64_t>(layout.carry) * scroll.elapsed / ticks);
        const retropp::Vec2i out = detail::outward(scroll.side);
        Step step{.move = {.x = out.x * (carriedNow - scroll.carried), .y = out.y * (carriedNow - scroll.carried)}};
        scroll.carried = carriedNow;
        if (scroll.elapsed >= ticks) {
            step.move.x -= scroll.origin.x;
            step.move.y -= scroll.origin.y;
            camera_.place({.x = camera_.position().x - scroll.origin.x, .y = camera_.position().y - scroll.origin.y});
            step.entered = scroll.to;
            area_ = scroll.to;
            inside_ = std::nullopt;
            transition_ = std::monostate{};
        }
        return step;
    }

    [[nodiscard]] Step fading(const Layout& layout, Fading& fade, retropp::IntRect box) {
        ++fade.elapsed;
        Step step{};
        if (!fade.switched && fade.elapsed >= layout.fadeTicks) {
            fade.switched = true;
            const retropp::IntRect target = arrival(layout, fade.to, fade.entrance, box);
            step.move = {.x = target.x - box.x, .y = target.y - box.y};
            step.entered = fade.to;
            area_ = fade.to;
            camera_.place(layout.camera(detail::bounds(layout, area_), layout.view.size, center(target)));
            inside_ = spotHolding(layout, center(target));
        }
        if (fade.elapsed >= 2 * layout.fadeTicks) {
            transition_ = std::monostate{};
        }
        return step;
    }

    // Where a box the size of `box` lands coming in through opening `entrance` of `area`: centered on
    // a spot, or just inside a wall's gap, centered along it.
    [[nodiscard]] static retropp::IntRect arrival(const Layout& layout, int area, int entrance,
                                                  retropp::IntRect box) noexcept {
        const Area& there = layout.areas[static_cast<std::size_t>(area)];
        const Opening& into = there.openings[static_cast<std::size_t>(entrance)];
        const int cell = layout.cellSize;
        if (detail::isSpot(into)) {
            const retropp::IntRect spot = detail::spotPixels(layout, into);
            return retropp::IntRect{.x = spot.x + (spot.width - box.width) / 2,
                                    .y = spot.y + (spot.height - box.height) / 2,
                                    .width = box.width,
                                    .height = box.height};
        }
        const retropp::IntRect bounds = detail::bounds(layout, area);
        const int along = (into.span.first + into.span.last + 1) * cell / 2;
        retropp::IntRect at{.x = 0, .y = 0, .width = box.width, .height = box.height};
        switch (into.side) {
            case Side::West: at.x = cell; at.y = along - box.height / 2; break;
            case Side::East: at.x = bounds.width - cell - box.width; at.y = along - box.height / 2; break;
            case Side::North: at.y = cell; at.x = along - box.width / 2; break;
            case Side::South: at.y = bounds.height - cell - box.height; at.x = along - box.width / 2; break;
            case Side::None:
            default: break;
        }
        return at;
    }

    int area_ = 0;
    CameraKit::Camera camera_;
    std::variant<std::monostate, Scrolling, Fading> transition_{};
    std::optional<int> inside_;   // the spot opening the followed box's center is in
};

}  // namespace WorldKit
