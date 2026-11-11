// What a world is made of: areas, each with its own maps and art, joined only by the openings in them.
//
//     const WorldKit::Layout layout{
//         .areas = {
//             WorldKit::Area{.tiles = &meadowTiles, .art = overworldArt, .collision = &meadowCollision,
//                            .openings = {{.name = "east door", .side = WorldKit::Side::East, .span = {6, 9},
//                                          .transition = WorldKit::Transition::Scroll,
//                                          .leadsTo = {.area = Area::Lake, .opening = "west door"}},
//                                         {.name = "cave mouth", .at = {.x = 12, .y = 4, .width = 2, .height = 1},
//                                          .transition = WorldKit::Transition::Fade,
//                                          .leadsTo = {.area = Area::Cave, .opening = "cave exit"}}}},
//             ...
//         },
//         .cellSize = 8, .open = Cell::Open, .view = kView};
//     WorldKit::validate(layout);
//
// An area is a dungeon room, an overworld screen, a cave: any place with a tile map, the art that map is
// drawn through, and a collision map. Areas have no position in a larger world. An opening in one leads
// to an opening in another, and that link is the only thing joining them; the two areas sit side by side
// only for as long as a scroll between them runs. Every position inside an area is in that area's own
// pixels, from its map's top-left.
#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/palette.h"
#include "retropp/tilemap.h"

#include "CameraKit/camera.h"
#include "CameraKit/laws.h"

namespace WorldKit {

// A side of an area: the wall an edge opening is in. None marks an opening that is not in a wall.
enum class Side : std::uint8_t { None, North, South, East, West };

// How an opening is gone through: the next area scrolls in, lined up with the opening, or the screen
// fades out and in on the next area.
enum class Transition : std::uint8_t { Scroll, Fade };

// A range of cells along a side, inclusive at both ends: down the east and west walls, across the
// north and south walls.
struct Span {
    int first = 0;
    int last = 0;

    [[nodiscard]] constexpr bool operator==(const Span&) const noexcept = default;
};

// No area: the value of Destination::area for an opening that leads nowhere.
inline constexpr int kNoArea = -1;

// Where an opening leads: an area, by its index in the layout, and the name of an opening in it. A game
// names its areas with an enum, whose enumerators are the indices.
struct Destination {
    int area = kNoArea;
    std::string opening;
};

// An opening in an area. An edge opening is a gap in a wall: `side` says which wall and `span` which
// cells of it. A spot opening is a rectangle of cells inside the area, `at`, such as stairs or a cave
// mouth. Exactly one of the two is set. The opening's `transition` is how it is gone through, and
// `leadsTo` is where; an opening that leads nowhere is only an entrance.
struct Opening {
    std::string name;
    Side side = Side::None;
    Span span{};
    retropp::IntRect at{};
    Transition transition = Transition::Scroll;
    Destination leadsTo{};
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

// One area: its tile map, the catalog its map values are drawn through, its collision map and its
// openings. The maps are the game's and must outlive the layout; the two are the same size.
struct Area {
    const retropp::IndexGrid* tiles = nullptr;
    retropp::TileCatalog art{};
    const retropp::IndexGrid* collision = nullptr;
    std::vector<Opening> openings;
};

// A camera law: where the camera should be, given the rectangle it may show (an area, in that area's
// pixels), the view's size and the point it is looking at. CameraKit::follow is one; any function of
// this shape fits.
using CameraLaw = std::function<retropp::Vec2i(retropp::IntRect bounds, retropp::PixelSize view,
                                               retropp::Vec2i focus)>;

// The whole world. Area indices are positions in `areas`.
struct Layout {
    std::vector<Area> areas;

    // The side of one map cell, in pixels.
    int cellSize = 8;

    // The one collision-map value that is not solid.
    CellValue open{};

    // The screen the camera shows and where it sits on a layer.
    CameraKit::View view{};

    // Where the camera is within an area.
    CameraLaw camera = CameraKit::follow;

    // A scroll: how many ticks it takes, and how far into the new area the thing that went through the
    // opening is carried while it runs.
    std::uint32_t scrollTicks = 32;
    int carry = 16;

    // A fade: how many ticks the screen takes to go dark, and the same again to come back; its color.
    std::uint32_t fadeTicks = 16;
    retropp::Rgba8 fadeColor{};
};

namespace detail {

[[nodiscard]] inline bool isEdge(const Opening& opening) noexcept {
    return opening.side != Side::None;
}

[[nodiscard]] inline bool isSpot(const Opening& opening) noexcept {
    return opening.at.width > 0 && opening.at.height > 0;
}

// The side across from `side`: the wall an entrance is in when its exit is in `side`.
[[nodiscard]] inline Side facing(Side side) noexcept {
    switch (side) {
        case Side::North: return Side::South;
        case Side::South: return Side::North;
        case Side::East: return Side::West;
        case Side::West: return Side::East;
        case Side::None:
        default: return Side::None;
    }
}

// One pixel outward through `side`.
[[nodiscard]] inline retropp::Vec2i outward(Side side) noexcept {
    switch (side) {
        case Side::North: return {.x = 0, .y = -1};
        case Side::South: return {.x = 0, .y = 1};
        case Side::East: return {.x = 1, .y = 0};
        case Side::West: return {.x = -1, .y = 0};
        case Side::None:
        default: return {};
    }
}

// Whether a side runs across the area (north and south) rather than down it (east and west).
[[nodiscard]] inline bool runsAcross(Side side) noexcept {
    return side == Side::North || side == Side::South;
}

// The cells along `side` of an area.
[[nodiscard]] inline int sideLength(const Area& area, Side side) noexcept {
    return runsAcross(side) ? area.collision->width : area.collision->height;
}

// The area's rectangle in its own pixels: from (0, 0), its map's size times the cell size.
[[nodiscard]] inline retropp::IntRect bounds(const Layout& layout, int area) noexcept {
    const retropp::IndexGrid& map = *layout.areas[static_cast<std::size_t>(area)].collision;
    return retropp::IntRect{.x = 0,
                            .y = 0,
                            .width = map.width * layout.cellSize,
                            .height = map.height * layout.cellSize};
}

// A spot opening's cells as a pixel rectangle.
[[nodiscard]] inline retropp::IntRect spotPixels(const Layout& layout, const Opening& opening) noexcept {
    return retropp::IntRect{.x = opening.at.x * layout.cellSize,
                            .y = opening.at.y * layout.cellSize,
                            .width = opening.at.width * layout.cellSize,
                            .height = opening.at.height * layout.cellSize};
}

[[nodiscard]] inline bool contains(retropp::IntRect rect, retropp::Vec2i point) noexcept {
    return point.x >= rect.x && point.x < rect.x + rect.width && point.y >= rect.y &&
           point.y < rect.y + rect.height;
}

[[nodiscard]] inline bool holds(const Span& span, int cell) noexcept {
    return cell >= span.first && cell <= span.last;
}

[[nodiscard]] inline bool overlap(const Span& a, const Span& b) noexcept {
    return a.first <= b.last && b.first <= a.last;
}

[[nodiscard]] inline bool overlap(retropp::IntRect a, retropp::IntRect b) noexcept {
    return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}

// The cell holding pixel coordinate `pixel`, rounding toward negative infinity.
[[nodiscard]] inline int cellOf(int pixel, int cellSize) noexcept {
    return pixel >= 0 ? pixel / cellSize : -((-pixel + cellSize - 1) / cellSize);
}

// The opening named `name` in `area`, or -1.
[[nodiscard]] inline int findOpening(const Area& area, const std::string& name) noexcept {
    for (std::size_t i = 0; i < area.openings.size(); ++i) {
        if (area.openings[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

[[nodiscard]] inline std::string where(int area, const Opening& opening) {
    return "WorldKit: area " + std::to_string(area) + " opening '" + opening.name + "'";
}

}  // namespace detail

// Checks the layout and throws std::invalid_argument, naming the area and opening, for: an area with no
// map, or maps of two sizes; an opening with no name or a name used twice in its area; an opening that is
// neither in a wall nor a spot, or both; a span off its wall or a spot off the map; two openings that
// overlap; a destination naming an area or an opening that does not exist; a scroll that is not from a
// wall to the facing wall of the next area through a span of the same length. A game calls it once,
// after building the layout.
inline void validate(const Layout& layout) {
    if (layout.cellSize <= 0) {
        throw std::invalid_argument("WorldKit: the cell size must be positive");
    }
    for (std::size_t a = 0; a < layout.areas.size(); ++a) {
        const Area& area = layout.areas[a];
        const std::string here = "WorldKit: area " + std::to_string(a);
        if (area.tiles == nullptr || area.collision == nullptr) {
            throw std::invalid_argument(here + " has no tile map or no collision map");
        }
        if (area.tiles->width != area.collision->width || area.tiles->height != area.collision->height) {
            throw std::invalid_argument(here + "'s tile map and collision map differ in size");
        }
        for (std::size_t i = 0; i < area.openings.size(); ++i) {
            const Opening& opening = area.openings[i];
            const std::string at = detail::where(static_cast<int>(a), opening);
            if (opening.name.empty()) {
                throw std::invalid_argument(here + " has an opening with no name");
            }
            for (std::size_t j = 0; j < i; ++j) {
                if (area.openings[j].name == opening.name) {
                    throw std::invalid_argument(at + " is named twice");
                }
            }
            const bool edge = detail::isEdge(opening);
            const bool spot = detail::isSpot(opening);
            if (edge == spot) {
                throw std::invalid_argument(at + " must be in one wall or at one spot");
            }
            if (edge) {
                if (opening.span.first > opening.span.last || opening.span.first < 0 ||
                    opening.span.last >= detail::sideLength(area, opening.side)) {
                    throw std::invalid_argument(at + "'s span is off its wall");
                }
            } else if (opening.at.x < 0 || opening.at.y < 0 ||
                       opening.at.x + opening.at.width > area.collision->width ||
                       opening.at.y + opening.at.height > area.collision->height) {
                throw std::invalid_argument(at + " is off the map");
            }
            for (std::size_t j = 0; j < i; ++j) {
                const Opening& other = area.openings[j];
                const bool clash = edge ? other.side == opening.side && detail::overlap(other.span, opening.span)
                                        : detail::isSpot(other) && detail::overlap(other.at, opening.at);
                if (clash) {
                    throw std::invalid_argument(at + " overlaps opening '" + other.name + "'");
                }
            }
            if (opening.leadsTo.area == kNoArea) {
                continue;
            }
            if (opening.leadsTo.area < 0 ||
                static_cast<std::size_t>(opening.leadsTo.area) >= layout.areas.size()) {
                throw std::invalid_argument(at + " leads to area " + std::to_string(opening.leadsTo.area) +
                                            ", which does not exist");
            }
            const Area& next = layout.areas[static_cast<std::size_t>(opening.leadsTo.area)];
            const int entrance = detail::findOpening(next, opening.leadsTo.opening);
            if (entrance < 0) {
                throw std::invalid_argument(at + " leads to '" + opening.leadsTo.opening + "' in area " +
                                            std::to_string(opening.leadsTo.area) + ", which has no such opening");
            }
            if (opening.transition == Transition::Scroll) {
                const Opening& into = next.openings[static_cast<std::size_t>(entrance)];
                if (!edge || into.side != detail::facing(opening.side)) {
                    throw std::invalid_argument(at + " scrolls, so it and '" + into.name +
                                                "' must be in facing walls");
                }
                if (into.span.last - into.span.first != opening.span.last - opening.span.first) {
                    throw std::invalid_argument(at + " scrolls into '" + into.name +
                                                "', whose span is a different length");
                }
            }
        }
    }
}

}  // namespace WorldKit
