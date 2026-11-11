// What a walker collides with: its CollisionShape, the pixels it covers, and the solid queries those
// pixels are tested against.
//
//     const TopDownMovement::TileCollision walls{.map = &collisionMap, .cellSize = 8, .open = Cell::Open};
//
//     TopDownMovement::CollisionShape box = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8};
//     TopDownMovement::CollisionShape round = retropp::ShapePoints::circle({8, 12}, 4);
//     box.hits(walls, {72, 56});   // would the box, moved to (72, 56), touch a solid cell?
//
// A shape is written in its owner's own pixels, relative to the owner's top-left, and moved to where the
// owner stands when it is tested. It covers a pixel when the pixel's center is inside it.
#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

#include "retropp/curve.h"
#include "retropp/draw_state.h"
#include "retropp/geometry.h"
#include "retropp/image.h"

namespace TopDownMovement {

// Anything that answers whether a rectangle of pixels touches something solid: an object with a
// `solidBox(retropp::IntRect)` member, such as TileCollision, or a callable taking a retropp::IntRect.
template <class Q>
concept SolidQuery =
    requires(const Q& query, retropp::IntRect box) {
        { query.solidBox(box) } -> std::convertible_to<bool>;
    } || std::predicate<const Q&, retropp::IntRect>;

namespace detail {

template <SolidQuery Q>
bool isSolid(const Q& query, retropp::IntRect box) {
    if constexpr (requires { { query.solidBox(box) } -> std::convertible_to<bool>; }) {
        return query.solidBox(box);
    } else {
        return query(box);
    }
}

}  // namespace detail

// The pixels a walker collides with. Built from a rectangle, from any retropp::ShapePoints made of points
// or a curve, or from the polygon through named anchors in the walker's art (fromAnchors). Each converts
// implicitly, so a WalkerConfig takes any of them as `.collision`.
class CollisionShape {
public:
    // Covers nothing, so it never hits anything.
    CollisionShape() = default;

    // Every pixel of `box`. An empty box covers nothing.
    CollisionShape(retropp::IntRect box) {
        if (box.width <= 0 || box.height <= 0) {
            return;
        }
        rectangle_ = true;
        bounds_ = box;
        for (int y = box.y; y < box.y + box.height; ++y) {
            runs_.push_back(Run{.y = y, .x = box.x, .width = box.width});
        }
    }

    // Every pixel whose center `shape.contains`: the same test a region drawn with this shape uses, so a
    // collision shape covers what it would color on screen. `radius` and `strokeWidth` apply. Throws
    // std::invalid_argument for a shape with no points and no curve, an inverted shape (its outside is
    // infinite), or a shape with a non-identity `transform` (move its points instead).
    CollisionShape(const retropp::ShapePoints& shape) {
        if (!shape.hasRegion()) {
            throw std::invalid_argument("CollisionShape: the shape has no points and no curve");
        }
        if (shape.invert) {
            throw std::invalid_argument("CollisionShape: an inverted shape covers everything outside it");
        }
        if (!shape.transform.isIdentity()) {
            throw std::invalid_argument("CollisionShape: a transformed shape; move its points instead");
        }

        // Every point of a curve segment lies inside the hull of its live control points, and every point
        // of a polygon is one of its points, so these bound the boundary; radius and half the stroke reach
        // past it.
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();
        const auto include = [&](float x, float y) {
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        };
        if (shape.curve.empty()) {
            for (const retropp::Point& p : shape.points) {
                include(p.x, p.y);
            }
        } else {
            for (const retropp::CurveSegment& s : shape.curve) {
                include(s.p0.x, s.p0.y);
                include(s.p1.x, s.p1.y);
                if (s.degree != retropp::CurveDegree::Linear) {
                    include(s.p2.x, s.p2.y);
                }
                if (s.degree == retropp::CurveDegree::Cubic) {
                    include(s.p3.x, s.p3.y);
                }
            }
        }
        const float reach = std::max(shape.radius, 0.0f) + std::max(shape.strokeWidth, 0.0f) / 2.0f;
        const int firstX = static_cast<int>(std::floor(minX - reach));
        const int firstY = static_cast<int>(std::floor(minY - reach));
        const int lastX = static_cast<int>(std::ceil(maxX + reach));
        const int lastY = static_cast<int>(std::ceil(maxY + reach));

        for (int y = firstY; y <= lastY; ++y) {
            int runStart = 0;
            bool inRun = false;
            for (int x = firstX; x <= lastX + 1; ++x) {
                const bool covered =
                    x <= lastX &&
                    shape.contains(retropp::Point{static_cast<float>(x) + 0.5f,
                                                  static_cast<float>(y) + 0.5f});
                if (covered && !inRun) {
                    runStart = x;
                    inRun = true;
                } else if (!covered && inRun) {
                    addRun(Run{.y = y, .x = runStart, .width = x - runStart});
                    inRun = false;
                }
            }
        }
    }

    // The polygon through the anchors named in `labels`, in that order, read from `anchors` (the art's
    // anchor table). Throws std::out_of_range when a label is not in the table and std::invalid_argument
    // for fewer than three labels.
    [[nodiscard]] static CollisionShape fromAnchors(std::span<const retropp::Anchor> anchors,
                                                    std::initializer_list<std::string_view> labels) {
        if (labels.size() < 3) {
            throw std::invalid_argument("CollisionShape::fromAnchors: a polygon needs three anchors");
        }
        retropp::ShapePoints polygon;
        for (const std::string_view label : labels) {
            const auto found = std::find_if(anchors.begin(), anchors.end(),
                                            [&](const retropp::Anchor& a) { return a.label == label; });
            if (found == anchors.end()) {
                throw std::out_of_range("CollisionShape::fromAnchors: no anchor with that label");
            }
            polygon.points.push_back(retropp::Point{found->x, found->y});
        }
        return CollisionShape{polygon};
    }

    // Whether the shape covers no pixel.
    [[nodiscard]] bool empty() const noexcept { return runs_.empty(); }

    // The smallest rectangle holding every covered pixel; all zeros when the shape is empty.
    [[nodiscard]] retropp::IntRect bounds() const noexcept { return bounds_; }

    // Whether the shape covers `pixel`.
    [[nodiscard]] bool covers(retropp::Vec2i pixel) const noexcept {
        return std::any_of(runs_.begin(), runs_.end(), [&](const Run& run) {
            return run.y == pixel.y && pixel.x >= run.x && pixel.x < run.x + run.width;
        });
    }

    // The same shape moved by `offset`.
    [[nodiscard]] CollisionShape at(retropp::Vec2i offset) const {
        CollisionShape moved = *this;
        for (Run& run : moved.runs_) {
            run.x += offset.x;
            run.y += offset.y;
        }
        if (!moved.empty()) {
            moved.bounds_.x += offset.x;
            moved.bounds_.y += offset.y;
        }
        return moved;
    }

    // Whether the shape, moved by `offset`, covers any pixel `solids` calls solid. A shape built from an
    // IntRect is one query; any other shape is one query per row of covered pixels.
    template <SolidQuery Q>
    [[nodiscard]] bool hits(const Q& solids, retropp::Vec2i offset = {}) const {
        if (rectangle_) {
            return detail::isSolid(solids, retropp::IntRect{.x = bounds_.x + offset.x,
                                                            .y = bounds_.y + offset.y,
                                                            .width = bounds_.width,
                                                            .height = bounds_.height});
        }
        return std::any_of(runs_.begin(), runs_.end(), [&](const Run& run) {
            return detail::isSolid(solids, retropp::IntRect{.x = run.x + offset.x,
                                                            .y = run.y + offset.y,
                                                            .width = run.width,
                                                            .height = 1});
        });
    }

private:
    // One row's covered pixels: [x, x + width) on row y.
    struct Run {
        int y = 0;
        int x = 0;
        int width = 0;
    };

    void addRun(Run run) {
        if (runs_.empty()) {
            bounds_ = retropp::IntRect{.x = run.x, .y = run.y, .width = run.width, .height = 1};
        } else {
            const int right = std::max(bounds_.x + bounds_.width, run.x + run.width);
            const int bottom = std::max(bounds_.y + bounds_.height, run.y + 1);
            bounds_.x = std::min(bounds_.x, run.x);
            bounds_.y = std::min(bounds_.y, run.y);
            bounds_.width = right - bounds_.x;
            bounds_.height = bottom - bounds_.y;
        }
        runs_.push_back(run);
    }

    std::vector<Run> runs_;
    retropp::IntRect bounds_{};
    bool rectangle_ = false;
};

// A value held by a cell of a collision map, written as the map's own enumerator or as a number.
struct CellValue {
    std::uint16_t value = 0;

    constexpr CellValue() = default;
    constexpr CellValue(std::uint16_t number) : value(number) {}
    template <class E>
        requires std::is_enum_v<E>
    constexpr CellValue(E cell) : value(static_cast<std::uint16_t>(cell)) {}
};

// Solid cells on a collision map: every cell of `map` whose value is not `open` is solid, and so is
// every pixel off the map. Pixels are the map's cells scaled up by `cellSize`; negative pixels lie off
// the map. With no map, everything is solid.
struct TileCollision {
    const retropp::IndexGrid* map = nullptr;
    int cellSize = 8;
    CellValue open{};

    // Whether `pixel` is in a solid cell.
    [[nodiscard]] bool solidAt(retropp::Vec2i pixel) const noexcept {
        return solidCell(cellOf(pixel.x), cellOf(pixel.y));
    }

    // Whether any pixel of `box` is in a solid cell. An empty box covers no cell and is never solid.
    [[nodiscard]] bool solidBox(retropp::IntRect box) const noexcept {
        if (box.width <= 0 || box.height <= 0) {
            return false;
        }
        const int lastColumn = cellOf(box.x + box.width - 1);
        const int lastRow = cellOf(box.y + box.height - 1);
        for (int cy = cellOf(box.y); cy <= lastRow; ++cy) {
            for (int cx = cellOf(box.x); cx <= lastColumn; ++cx) {
                if (solidCell(cx, cy)) {
                    return true;
                }
            }
        }
        return false;
    }

private:
    // The cell holding pixel coordinate `pixel`, rounding toward negative infinity.
    [[nodiscard]] int cellOf(int pixel) const noexcept {
        return pixel >= 0 ? pixel / cellSize : -((-pixel + cellSize - 1) / cellSize);
    }

    [[nodiscard]] bool solidCell(int cx, int cy) const noexcept {
        if (map == nullptr || cx < 0 || cy < 0 || cx >= map->width || cy >= map->height) {
            return true;
        }
        return map->at(cx, cy) != open.value;
    }
};

}  // namespace TopDownMovement
