// Where in the world the screen is: which layout and area, where the camera looks in it, and the
// scroll or fade to the next area when something goes out through an opening — or, through a
// seamless join, the switch to the next area with nothing in between.
//
//     WorldKit::Camera camera{Layout::Overworld, Area::Meadow};
//     camera.place(world, playerBox);                                    // at the start
//
//     if (!camera.transitioning()) { /* move the player against camera.solids(world) */ }
//     const WorldKit::Step step = camera.follow(world, playerBox);        // each tick
//     player.place(player.position() + step.move);
//     if (step.entered) { /* the player is now in area step.entered->area of layout step.entered->layout */ }
//
// A game keeps one Camera in its state. Each tick it hands the camera the box of the thing the screen
// follows, and applies the move the camera hands back: nothing while the thing walks on its own, the
// carry into the next area during a scroll, the jump to the entrance at the middle of a fade, and at the
// end of either — or at once, across a seamless join — the change into the next area's pixels. The
// camera holds a CameraKit::Camera and places it by the layout's camera law; the game never touches it.
#pragma once

#include <algorithm>
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

// What one tick of following asks of the thing followed: how far to move it, and the place it has just
// entered, on the tick the camera's area changes.
struct Step {
    retropp::Vec2i move{};
    std::optional<Place> entered{};
};

// An area on screen, and the scroll that places a layer of its pixels.
struct Shown {
    int layout = 0;
    int area = 0;
    retropp::LayerScroll scroll{};
};

class Camera {
public:
    // A camera in area `area` of layout `layout`, looking at its top-left.
    Camera(int layout, int area) noexcept : layout_(layout), area_(area) {}

    // The layout the camera is in. It changes when the area does.
    [[nodiscard]] int layout() const noexcept { return layout_; }

    // The area the camera is in. During a scroll it is still the area being left; it changes on the
    // scroll's last tick. During a fade it changes at the middle, when the screen is dark. Through a
    // seamless join it changes on the tick the followed box's center crosses the wall.
    [[nodiscard]] int area() const noexcept { return area_; }

    // Whether a scroll or a fade is running. A game moves nothing of its own while one is. A seamless
    // join runs nothing.
    [[nodiscard]] bool transitioning() const noexcept {
        return !std::holds_alternative<std::monostate>(transition_);
    }

    // What is solid in the current area.
    [[nodiscard]] AreaSolids solids(const World& world) const noexcept {
        return AreaSolids{.world = &world, .layout = layout_, .area = area_};
    }

    // Looks at `box` at once, by the layout's camera law, with no transition: for placing the thing
    // followed at the start or after a jump. A transition in progress is dropped.
    void place(const World& world, retropp::IntRect box) {
        transition_ = std::monostate{};
        camera_.place(lawPlace(world, center(box)));
        focus_ = center(box);
        inside_ = spotHolding(world.layouts[static_cast<std::size_t>(layout_)], center(box));
    }

    // One tick. While nothing is running, the camera follows `box` by the law on each axis along which
    // the area — with whatever is joined to it seamlessly — is at least as big as the view: it moves
    // only when the box moves, with the box's own step, toward where the law puts it and no further,
    // and never against the box's motion. In an area on its own that is the law exactly; after a
    // seamless join or a scroll it means the camera stays where it was and comes to the law's place
    // only as the box walks that way. On an axis along which the area is smaller than the view, the
    // camera holds still until the next transition. The box's center going out through an opening that leads somewhere
    // starts that opening's transition: out through a wall's gap, or into a spot from outside it.
    // While a scroll runs, the camera slides along the exit's axis — and only that axis, unless the
    // layout's `scrollAligns` — until the next area's wall is at the edge of the view, and the box is
    // carried the layout's `carry` pixels in; on the last tick the move also takes the box into the
    // next area's pixels, and following goes on from where the camera is. While a fade runs, the
    // screen darkens over the layout's `fadeTicks`, the box
    // jumps to the entrance (just inside a wall's gap, or onto a spot) and the next area becomes
    // current, and the screen lightens over the next layout's `fadeTicks`. Out through a seamless
    // join, the next area becomes current on the same tick: the move is the change into its pixels,
    // the camera makes the same change so nothing moves on screen, and nothing runs.
    [[nodiscard]] Step follow(const World& world, retropp::IntRect box) {
        if (auto* scroll = std::get_if<Scrolling>(&transition_)) {
            return scrolling(world, *scroll, box);
        }
        if (auto* fade = std::get_if<Fading>(&transition_)) {
            return fading(world, *fade, box);
        }

        const Layout& here = world.layouts[static_cast<std::size_t>(layout_)];
        const retropp::Vec2i focus = center(box);
        const retropp::IntRect bounds = lawBounds(world);
        const retropp::Vec2i want = here.camera(bounds, here.view.size, focus);
        if (!focus_) {
            camera_.place(want);
        } else {
            // On an axis where the area is smaller than the view, the camera holds still until a
            // transition.
            retropp::Vec2i at = camera_.position();
            if (bounds.width >= here.view.size.width) {
                at.x = pushed(at.x, want.x, focus.x - focus_->x);
            }
            if (bounds.height >= here.view.size.height) {
                at.y = pushed(at.y, want.y, focus.y - focus_->y);
            }
            camera_.place(at);
        }
        focus_ = focus;

        const Area& area = here.areas[static_cast<std::size_t>(area_)];
        const std::optional<int> nowInside = spotHolding(here, focus);
        for (std::size_t i = 0; i < area.openings.size(); ++i) {
            const Opening& opening = area.openings[i];
            if (opening.leadsTo.area == kNoArea) {
                continue;
            }
            const bool through = detail::isEdge(opening)
                                     ? outThrough(here, opening, focus)
                                     : nowInside == static_cast<int>(i) && inside_ != nowInside;
            if (through) {
                return start(world, opening, box);
            }
        }
        inside_ = nowInside;
        return Step{};
    }

    // The camera's position in the current area's pixels (in the area being left, during a scroll).
    [[nodiscard]] retropp::Vec2i position() const noexcept { return camera_.position(); }

    // The scroll for a layer of things placed in the current area's pixels.
    [[nodiscard]] retropp::LayerScroll scroll(const World& world) const noexcept {
        return camera_.scroll(world.layouts[static_cast<std::size_t>(layout_)].view);
    }

    // The areas on screen with the scroll each one's layer takes: the current area; every area joined
    // to it seamlessly whose attached rectangle is in view, each once; and during a scroll the next
    // area, lined up with the opening being gone through.
    [[nodiscard]] std::vector<Shown> shown(const World& world) const {
        const Layout& here = world.layouts[static_cast<std::size_t>(layout_)];
        const retropp::LayerScroll mine = scroll(world);
        std::vector<Shown> result{Shown{.layout = layout_, .area = area_, .scroll = mine}};
        const auto add = [&](Place to, retropp::Vec2i origin) {
            for (const Shown& already : result) {
                if (already.layout == to.layout && already.area == to.area) {
                    return;
                }
            }
            result.push_back(Shown{.layout = to.layout,
                                   .area = to.area,
                                   .scroll = {.x = mine.x - origin.x, .y = mine.y - origin.y}});
        };
        const retropp::IntRect view{.x = camera_.position().x,
                                    .y = camera_.position().y,
                                    .width = here.view.size.width,
                                    .height = here.view.size.height};
        detail::eachSeamless(world, layout_, area_, [&](const Opening&, const detail::Link& link) {
            const retropp::IntRect next = attached(world, link);
            if (detail::overlap(view, next)) {
                add(link.to, link.origin);
            }
        });
        if (const auto* scroll = std::get_if<Scrolling>(&transition_)) {
            add(scroll->to, scroll->origin);
        }
        return result;
    }

    // How dark the screen is, 0 to 1: 0 except during a fade.
    [[nodiscard]] float fade(const World& world) const noexcept {
        const auto* fade = std::get_if<Fading>(&transition_);
        if (fade == nullptr) {
            return 0.0f;
        }
        const std::uint32_t half = world.layouts[static_cast<std::size_t>(layout_)].fadeTicks;
        if (half == 0) {
            return 0.0f;
        }
        const std::uint32_t from = fade->switched ? half - std::min(fade->elapsed, half) : std::min(fade->elapsed, half);
        return static_cast<float>(from) / static_cast<float>(half);
    }

private:
    // A scroll in progress: to which place, out through which wall, where that area's top-left sits in
    // this area's pixels, and how far along it is.
    struct Scrolling {
        Place to{};
        Side side = Side::None;
        retropp::Vec2i origin{};
        std::uint32_t elapsed = 0;
        int carried = 0;
    };

    // A fade in progress: to which place and which of its openings; how far along the half that is
    // running is, and whether the switch has been made.
    struct Fading {
        Place to{};
        int entrance = 0;
        std::uint32_t elapsed = 0;
        bool switched = false;
    };

    [[nodiscard]] static retropp::Vec2i center(retropp::IntRect box) noexcept {
        return retropp::Vec2i{.x = box.x + box.width / 2, .y = box.y + box.height / 2};
    }

    // The rectangle a link's area occupies in the current area's pixels.
    [[nodiscard]] static retropp::IntRect attached(const World& world, const detail::Link& link) noexcept {
        const retropp::IntRect there =
            detail::bounds(world.layouts[static_cast<std::size_t>(link.to.layout)], link.to.area);
        return retropp::IntRect{.x = link.origin.x, .y = link.origin.y, .width = there.width, .height = there.height};
    }

    // The rectangle the law may show: the current area, extended along the axis of each seamless
    // opening's wall by the area attached there, so the camera follows straight across the seam.
    [[nodiscard]] retropp::IntRect lawBounds(const World& world) const noexcept {
        const retropp::IntRect mine = detail::bounds(world.layouts[static_cast<std::size_t>(layout_)], area_);
        int left = mine.x;
        int top = mine.y;
        int right = mine.x + mine.width;
        int bottom = mine.y + mine.height;
        detail::eachSeamless(world, layout_, area_, [&](const Opening& opening, const detail::Link& link) {
            const retropp::IntRect next = attached(world, link);
            if (detail::runsAcross(opening.side)) {
                top = std::min(top, next.y);
                bottom = std::max(bottom, next.y + next.height);
            } else {
                left = std::min(left, next.x);
                right = std::max(right, next.x + next.width);
            }
        });
        return retropp::IntRect{.x = left, .y = top, .width = right - left, .height = bottom - top};
    }

    // Where the law puts the camera for `focus` in the current area.
    [[nodiscard]] retropp::Vec2i lawPlace(const World& world, retropp::Vec2i focus) const {
        const Layout& here = world.layouts[static_cast<std::size_t>(layout_)];
        return here.camera(lawBounds(world), here.view.size, focus);
    }

    // One axis of following: where the camera goes from `at` when the box has moved `step` and the
    // law wants it at `want` — with the step, toward `want` and no further, or nowhere.
    [[nodiscard]] static int pushed(int at, int want, int step) noexcept {
        const int gap = want - at;
        if (step == 0 || gap == 0 || (gap > 0) != (step > 0)) {
            return at;
        }
        return at + (step > 0 ? std::min(gap, step) : std::max(gap, step));
    }

    // The spot opening of the current area whose cells hold `point`, if any.
    [[nodiscard]] std::optional<int> spotHolding(const Layout& here, retropp::Vec2i point) const noexcept {
        const Area& area = here.areas[static_cast<std::size_t>(area_)];
        for (std::size_t i = 0; i < area.openings.size(); ++i) {
            if (detail::isSpot(area.openings[i]) &&
                detail::contains(detail::spotPixels(here, area.openings[i]), point)) {
                return static_cast<int>(i);
            }
        }
        return std::nullopt;
    }

    // Whether `point` is past the wall `opening` is in, within its span.
    [[nodiscard]] bool outThrough(const Layout& here, const Opening& opening, retropp::Vec2i point) const noexcept {
        const retropp::IntRect area = detail::bounds(here, area_);
        const bool past = (opening.side == Side::East && point.x >= area.width) ||
                          (opening.side == Side::West && point.x < 0) ||
                          (opening.side == Side::South && point.y >= area.height) ||
                          (opening.side == Side::North && point.y < 0);
        const int along = detail::runsAcross(opening.side) ? point.x : point.y;
        return past && detail::holds(opening.span, detail::cellOf(along, here.cellSize));
    }

    // Goes out through `exit`: starts its scroll or fade, or makes the seamless switch at once.
    [[nodiscard]] Step start(const World& world, const Opening& exit, retropp::IntRect box) {
        const detail::Link link = detail::link(world, layout_, area_, exit);
        if (exit.transition == Transition::Fade) {
            transition_ = Fading{.to = link.to, .entrance = link.entrance};
            inside_ = spotHolding(world.layouts[static_cast<std::size_t>(layout_)], center(box));
            return Step{};
        }
        if (exit.transition == Transition::Seamless) {
            rebase(link.to, link.origin);
            focus_ = retropp::Vec2i{.x = center(box).x - link.origin.x, .y = center(box).y - link.origin.y};
            return Step{.move = {.x = -link.origin.x, .y = -link.origin.y}, .entered = link.to};
        }

        // The camera slides along the exit's axis until the next area's wall is at the edge of the
        // view; across that axis it does not move, unless the layout's scrolls align and the next area
        // is at least as big as the view there.
        const Layout& here = world.layouts[static_cast<std::size_t>(layout_)];
        const retropp::IntRect next = attached(world, link);
        const retropp::PixelSize view = here.view.size;
        retropp::Vec2i to = camera_.position();
        if (detail::runsAcross(exit.side)) {
            to.y = exit.side == Side::South ? next.y : next.y + next.height - view.height;
            if (here.scrollAligns && next.width >= view.width) {
                to.x = std::clamp(to.x, next.x, next.x + next.width - view.width);
            }
        } else {
            to.x = exit.side == Side::East ? next.x : next.x + next.width - view.width;
            if (here.scrollAligns && next.height >= view.height) {
                to.y = std::clamp(to.y, next.y, next.y + next.height - view.height);
            }
        }
        camera_.pan(to, here.scrollTicks);
        transition_ = Scrolling{.to = link.to, .side = exit.side, .origin = link.origin};
        inside_ = spotHolding(here, center(box));
        return Step{};
    }

    // Makes `to` current, with the camera in its pixels: the same point on screen, `origin` away. The
    // next tick of following starts afresh unless the caller says where the box is.
    void rebase(Place to, retropp::Vec2i origin) noexcept {
        camera_.place({.x = camera_.position().x - origin.x, .y = camera_.position().y - origin.y});
        layout_ = to.layout;
        area_ = to.area;
        inside_ = std::nullopt;
        focus_ = std::nullopt;
        transition_ = std::monostate{};
    }

    [[nodiscard]] Step scrolling(const World& world, Scrolling& scroll, retropp::IntRect box) {
        const Layout& here = world.layouts[static_cast<std::size_t>(layout_)];
        ++scroll.elapsed;
        camera_.advance();
        const std::uint32_t ticks = here.scrollTicks == 0 ? 1 : here.scrollTicks;
        const int carriedNow = static_cast<int>(static_cast<std::int64_t>(here.carry) * scroll.elapsed / ticks);
        const retropp::Vec2i out = detail::outward(scroll.side);
        Step step{.move = {.x = out.x * (carriedNow - scroll.carried), .y = out.y * (carriedNow - scroll.carried)}};
        scroll.carried = carriedNow;
        if (scroll.elapsed >= ticks) {
            step.move.x -= scroll.origin.x;
            step.move.y -= scroll.origin.y;
            step.entered = scroll.to;
            rebase(scroll.to, scroll.origin);
            // Following goes on from here: the box is where this move puts it.
            focus_ = retropp::Vec2i{.x = center(box).x + step.move.x, .y = center(box).y + step.move.y};
        }
        return step;
    }

    [[nodiscard]] Step fading(const World& world, Fading& fade, retropp::IntRect box) {
        ++fade.elapsed;
        Step step{};
        if (!fade.switched && fade.elapsed >= world.layouts[static_cast<std::size_t>(layout_)].fadeTicks) {
            fade.switched = true;
            fade.elapsed = 0;
            const Layout& there = world.layouts[static_cast<std::size_t>(fade.to.layout)];
            const retropp::IntRect target = arrival(there, fade.to.area, fade.entrance, box);
            step.move = {.x = target.x - box.x, .y = target.y - box.y};
            step.entered = fade.to;
            layout_ = fade.to.layout;
            area_ = fade.to.area;
            camera_.place(lawPlace(world, center(target)));
            focus_ = center(target);
            inside_ = spotHolding(there, center(target));
        }
        if (fade.switched && fade.elapsed >= world.layouts[static_cast<std::size_t>(layout_)].fadeTicks) {
            transition_ = std::monostate{};
        }
        return step;
    }

    // Where a box the size of `box` lands coming in through opening `entrance` of `area` in `there`:
    // centered on a spot, or just inside a wall's gap, centered along it.
    [[nodiscard]] static retropp::IntRect arrival(const Layout& there, int area, int entrance,
                                                  retropp::IntRect box) noexcept {
        const Area& next = there.areas[static_cast<std::size_t>(area)];
        const Opening& into = next.openings[static_cast<std::size_t>(entrance)];
        const int cell = there.cellSize;
        if (detail::isSpot(into)) {
            const retropp::IntRect spot = detail::spotPixels(there, into);
            return retropp::IntRect{.x = spot.x + (spot.width - box.width) / 2,
                                    .y = spot.y + (spot.height - box.height) / 2,
                                    .width = box.width,
                                    .height = box.height};
        }
        const retropp::IntRect bounds = detail::bounds(there, area);
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

    int layout_ = 0;
    int area_ = 0;
    CameraKit::Camera camera_;
    std::variant<std::monostate, Scrolling, Fading> transition_{};
    std::optional<int> inside_;             // the spot opening the followed box's center is in
    std::optional<retropp::Vec2i> focus_;   // the followed box's center last tick, in the current area's pixels
};

}  // namespace WorldKit
