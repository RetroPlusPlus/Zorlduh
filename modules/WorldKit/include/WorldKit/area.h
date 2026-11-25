// What a world is made of: layouts of areas, each area with its own maps and art, joined only by the
// openings in them.
//
//     const WorldKit::World world{.layouts = {
//         WorldKit::Layout{
//             .name = "overworld",
//             .areas = {
//                 WorldKit::Area{.name = "meadow",
//                                .layers = {{.name = "ground", .tiles = &meadowTiles, .art = overworldArt,
//                                            .z = depths.ground},
//                                           {.name = "overhang", .tiles = &meadowRock, .art = overlayArt,
//                                            .z = depths.overhead},
//                                           WorldKit::ReservedLayer{.name = "rain", .z = depths.rain}},
//                                .actorSlots = {{.name = "actors", .z = depths.actors}},
//                                .collision = &meadowCollision,
//                                .openings = {{.name = "east door", .side = WorldKit::Side::East, .span = {6, 9},
//                                              .transition = WorldKit::Transition::Scroll,
//                                              .leadsTo = {.area = Area::Lake, .opening = "west door"}},
//                                             {.name = "cave mouth", .at = {.x = 12, .y = 4, .width = 2, .height = 1},
//                                              .transition = WorldKit::Transition::Fade,
//                                              .leadsTo = {.area = Area::Cave, .opening = "cave exit"}}},
//                                .actors = {{.actor = Sheep(art), .at = {40, 64}}}},   // stays in the meadow
//                 ...
//             },
//             .actors = {{.actor = Fox(art), .in = Area::Lake, .at = {96, 40}}},     // roams the overworld
//             .cellSize = 8, .open = Cell::Open, .view = kView},
//         WorldKit::Layout{ ... the dungeon ... }},
//         .actors = {{.name = "hero", .actor = Hero(art), .in = {Layout::Overworld, Area::Meadow}, .at = {72, 56}}},
//     };
//     WorldKit::validate(world);
//     const WorldKit::Place lake = WorldKit::placeOf(world, "overworld", "lake");
//
// An area is a dungeon room, an overworld screen, a cave: any place with a name, a collision map, one or more
// tile maps each drawn through its own art on a layer of its own, the depths it reserves for layers the
// game builds itself, and the slots its moving things are drawn at, every one at a depth the game declares.
// An area alone on screen is drawn at those depths; with more areas on screen, every lower depth is
// drawn under every higher one across all of them (camera.h, detail::spread). A layout is a named set of
// areas that share a cell size, a view and the settings of their transitions; a world is one or more
// layouts. An area, a layout and the world each list the actors that start there, and the level an actor
// is written at is how far it may roam; a WorldKit::Scene (scene.h) spawns and runs them. A place is
// found by its index or, through placeOf, by the names of its layout and area. Areas have
// no position in a larger world. An opening in one leads to an opening in another, in the same layout
// or in any other, and that link is the only thing joining them; two areas sit side by side only while
// a scroll between them runs, or for as long as they are joined seamlessly. Every position inside an
// area is in that area's own pixels, from its map's top-left.
#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
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

// How an opening is gone through: the next area scrolls in, lined up with the opening; the screen
// fades out and in on the next area; or the two areas are joined seamlessly — the next area is drawn
// attached beyond the wall whenever it is in view, and going through the opening only changes which
// area is current.
enum class Transition : std::uint8_t { Scroll, Fade, Seamless };

// A range of cells along a side, inclusive at both ends: down the east and west walls, across the
// north and south walls.
struct Span {
    int first = 0;
    int last = 0;

    [[nodiscard]] constexpr bool operator==(const Span&) const noexcept = default;
};

// No area: the value of Destination::area for an opening that leads nowhere.
inline constexpr int kNoArea = -1;

// The same layout: the value of Destination::layout for an opening that leads within its own.
inline constexpr int kSameLayout = -1;

// Where an opening leads: a layout by its index in the world (or the opening's own), an area by its
// index in that layout, and the name of an opening in it. A game names its layouts and areas with
// enums whose enumerators are the indices. `offset` shifts the next area, in cells, from where the
// opening pair lines it up (positive right and down); a scroll or a seamless join takes it, a fade has
// nothing to shift.
struct Destination {
    int layout = kSameLayout;
    int area = kNoArea;
    std::string opening;
    retropp::Vec2i offset{};
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

// One layer of an area: its name, which is part of the layer's key; the map, one value per cell, the
// size of the area's collision map; the catalog its values are drawn through; and the depth the game
// declares for the layer on the engine's z axis, where lower is below. A map that covers only part of
// an area draws nothing in its other cells: their catalog entries name a slot whose every pixel is in
// the sheet's transparent-index set, and what is below shows through. A layer whose `tiles` is null is a
// depth the area reserves for a layer the game builds itself (ReservedLayer); WorldLayers skips it.
struct MapLayer {
    std::string name;
    const retropp::IndexGrid* tiles = nullptr;
    retropp::TileCatalog art{};
    std::int32_t z = 0;
};

// A depth an area reserves, under a name, for a layer the game builds itself, of any kind LayerKit
// builds: rain over the actors, a shadow under them, an effect between two map layers. It is written
// among the area's layers and becomes a MapLayer whose `tiles` is null:
//
//     .layers = {{.name = "ground", .tiles = &yard, .art = groundArt, .z = 0},
//                WorldKit::ReservedLayer{.name = "rain", .z = 12}},
//
// WorldLayers skips it. The game builds its layer for each area on screen at the depth LayerZ
// gives for that area (camera.h), and the layer lands among the area's other layers and actor slots by
// its declared depth.
struct ReservedLayer {
    std::string name;
    std::int32_t z = 0;

    // The entry among the area's layers: this name and depth, `tiles` null.
    operator MapLayer() const { return MapLayer{.name = name, .z = z}; }
};

// A named depth in an area for the things that move in it — the player, the people and creatures in
// it. A slot holds nothing: a game draws its sprites there through Actors (world_layers.h), or on a
// layer of its own at the depth ActorZ gives for the area on screen (camera.h), and they land among
// the area's map layers, over those below the slot and under those above it. An area has as many
// slots as it needs, or none.
struct ActorSlot {
    std::string name;
    std::int32_t z = 0;
};

// A place in the world: a layout and an area in it.
struct Place {
    int layout = 0;
    int area = 0;

    [[nodiscard]] constexpr bool operator==(const Place&) const noexcept = default;
};

// How far an actor may roam: its area, its layout, or the whole world. An actor written on an area has
// Reach::Area, on a layout Reach::Layout, on the world Reach::World; SceneActor::reach changes it
// (scene.h).
enum class Reach : std::uint8_t { Area, Layout, World };

// What an actor does at an opening that leads outside its reach.
//
//   Stop     It is held at the opening. A spot it walks across, and stays in its area. The default.
//   Despawn  It goes out through the opening and is removed: its id is retired.
//   Hide     It goes out through the opening and is put away. A hidden actor is out of the world: not
//            drawn, not ticked, not solid, not touchable, until the game puts it somewhere again with
//            Actors::moveTo. It keeps its id, its values and where its scripts are.
//
// At a spot, Despawn and Hide act on the tick the actor's center steps onto it.
enum class AtEdge : std::uint8_t { Stop, Despawn, Hide };

// When an actor is awake, taking part in each tick: always; while any area of the layout it is in is on
// screen; while the area it is in is on screen. On screen is what the camera shows: its own area, the
// areas across seams that are in view, and the area scrolling in. The rest of the time it is frozen: it
// holds exactly as it is, still drawn and still solid, and carries on from there when it wakes.
enum class AwakeWhen : std::uint8_t { Always, InLayout, OnScreen };

// An actor value of any type, held for a scene: written as `.actor = Guard(art)`. A WorldKit::Scene reads
// it back as the actor value it was made from (scene.h). Copies share the one value they hold.
class AnyActor {
public:
    // An empty holder.
    AnyActor() = default;

    // Holds a copy of `actor`.
    template <class T>
        requires(!std::same_as<std::remove_cvref_t<T>, AnyActor>)
    AnyActor(T actor) : held_(std::make_shared<const T>(std::move(actor))), type_(&typeid(T)) {}

    // Whether it holds a value.
    [[nodiscard]] explicit operator bool() const noexcept { return held_ != nullptr; }

    // The value it holds when that value is a T; nullptr when it holds another type or is empty.
    template <class T>
    [[nodiscard]] const T* as() const noexcept {
        return type_ != nullptr && *type_ == typeid(T) ? static_cast<const T*>(held_.get()) : nullptr;
    }

private:
    std::shared_ptr<const void> held_;
    const std::type_info* type_ = nullptr;
};

// An actor that starts in an area: `{.actor = Guard(art), .at = {40, 64}}` among the area's `actors`. Its
// reach is the area. `name` finds it in the scene (Scene::actor) and is unique across the whole world, or
// empty; `at` is its top-left in the area's pixels, put exactly there, walls included.
struct AreaActor {
    std::string name;
    AnyActor actor;
    retropp::Vec2i at{};
    AtEdge atEdge = AtEdge::Stop;
    AwakeWhen awakeWhen = AwakeWhen::Always;
};

// An actor that starts in an area of a layout and roams the layout: `{.actor = Hound(art), .in =
// CastleArea::WideHall, .at = {120, 80}}` among the layout's `actors`. `in` is the index of an area of that
// layout; the rest is as AreaActor's.
struct LayoutActor {
    std::string name;
    AnyActor actor;
    int in = 0;
    retropp::Vec2i at{};
    AtEdge atEdge = AtEdge::Stop;
    AwakeWhen awakeWhen = AwakeWhen::Always;
};

// An actor that starts in a place of the world and goes anywhere: `{.name = "marker", .actor =
// Marker(art), .in = {Castle, Hall}, .at = {76, 68}}` among the world's `actors`. The rest is as
// AreaActor's.
struct WorldActor {
    std::string name;
    AnyActor actor;
    Place in{};
    retropp::Vec2i at{};
    AtEdge atEdge = AtEdge::Stop;
    AwakeWhen awakeWhen = AwakeWhen::Always;
};

// One area: its name, unique within its layout; its layers, its actor slots, its collision map, its
// openings and the actors that start in it. The layers and slots are in any order; each one's `z` places
// it. The maps are the game's and must outlive the world; every one is the size of the collision map,
// which is the area's size.
struct Area {
    std::string name;
    std::vector<MapLayer> layers;
    std::vector<ActorSlot> actorSlots;
    const retropp::IndexGrid* collision = nullptr;
    std::vector<Opening> openings;
    std::vector<AreaActor> actors;
};

// A camera law: where the camera should be, given the rectangle it may show (an area, in that area's
// pixels, reaching straight across any seamless join into the area beyond), the view's size and the
// point it is looking at. CameraKit::follow is one; any function of this shape fits.
using CameraLaw = std::function<retropp::Vec2i(retropp::IntRect bounds, retropp::PixelSize view,
                                               retropp::Vec2i focus)>;

// A named set of areas that share one cell size, one open value, one view and the settings below, and
// the actors that start in its areas and roam it. Its name is unique within the world. Area indices are
// positions in `areas`.
struct Layout {
    std::string name;
    std::vector<Area> areas;
    std::vector<LayoutActor> actors;

    // The side of one map cell, in pixels.
    int cellSize = 8;

    // The one collision-map value that is not solid.
    CellValue open{};

    // The screen the camera shows and where it sits on a layer.
    CameraKit::View view{};

    // Where the camera is within an area.
    CameraLaw camera = CameraKit::follow;

    // A scroll out of one of these areas: how many ticks it takes, and how far into the new area the
    // thing that went through the opening is carried while it runs. The camera slides along the exit's
    // axis; with `scrollAligns` it also moves across that axis, as it slides, the least it must for the
    // view to lie within the next area when that area is at least as big as the view there — a
    // smaller area lands where the opening lines it up either way.
    std::uint32_t scrollTicks = 32;
    int carry = 16;
    bool scrollAligns = false;

    // A fade: how many ticks the screen takes to go dark leaving one of these areas, or to come back
    // arriving in one; the color it goes to.
    std::uint32_t fadeTicks = 16;
    retropp::Rgba8 fadeColor{};

    // How many seams away from the current area an area may be and still be drawn when it is in view:
    // 0 for any number, 1 for the areas joined to the current one, 2 for those and the areas joined to
    // them, and so on. The camera's law and the solid query reach one seam whatever this is.
    std::uint32_t seamDepth = 0;
};

// The whole world, and the actors that start in it and go anywhere. Layout indices are positions in
// `layouts`.
struct World {
    std::vector<Layout> layouts;
    std::vector<WorldActor> actors;
};

namespace detail {

[[nodiscard]] inline bool isEdge(const Opening& opening) noexcept {
    return opening.side != Side::None;
}

[[nodiscard]] inline bool isSpot(const Opening& opening) noexcept {
    return opening.at.width > 0 && opening.at.height > 0;
}

// Whether an opening is attached to the next area, so that the next area has a place in this one's
// pixels: a scroll or a seamless join.
[[nodiscard]] inline bool attaches(const Opening& opening) noexcept {
    return opening.transition != Transition::Fade;
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

// `a / b` rounded toward negative infinity, for a positive `b`.
[[nodiscard]] inline int floorDiv(int a, int b) noexcept {
    return a >= 0 ? a / b : -((-a + b - 1) / b);
}

// The cell holding pixel coordinate `pixel`.
[[nodiscard]] inline int cellOf(int pixel, int cellSize) noexcept {
    return floorDiv(pixel, cellSize);
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

// The place a destination names, from an opening in `layout`.
[[nodiscard]] inline Place resolve(const Destination& destination, int layout) noexcept {
    return Place{.layout = destination.layout == kSameLayout ? layout : destination.layout,
                 .area = destination.area};
}

// Where the next area's top-left sits in this area's pixels when `exit`, in `here`, attaches it:
// against the wall the exit is in, with the entrance span level with the exit span, then shifted by
// the destination's offset in this layout's cells.
[[nodiscard]] inline retropp::Vec2i attachment(const Layout& here, int area, const Opening& exit,
                                               const Layout& there, int to, const Opening& into) noexcept {
    const retropp::IntRect hereBounds = bounds(here, area);
    const retropp::IntRect thereBounds = bounds(there, to);
    const int level = exit.span.first * here.cellSize - into.span.first * there.cellSize;
    retropp::Vec2i origin{};
    switch (exit.side) {
        case Side::East: origin = {.x = hereBounds.width, .y = level}; break;
        case Side::West: origin = {.x = -thereBounds.width, .y = level}; break;
        case Side::South: origin = {.x = level, .y = hereBounds.height}; break;
        case Side::North: origin = {.x = level, .y = -thereBounds.height}; break;
        case Side::None:
        default: break;
    }
    origin.x += exit.leadsTo.offset.x * here.cellSize;
    origin.y += exit.leadsTo.offset.y * here.cellSize;
    return origin;
}

// Where an opening of a validated world leads: the place, the entrance's index in that area, and for
// an exit that attaches the next area, where that area's top-left sits in this one's pixels.
struct Link {
    Place to{};
    int entrance = 0;
    retropp::Vec2i origin{};
};

[[nodiscard]] inline Link link(const World& world, int layout, int area, const Opening& exit) noexcept {
    const Place to = resolve(exit.leadsTo, layout);
    const Layout& here = world.layouts[static_cast<std::size_t>(layout)];
    const Layout& there = world.layouts[static_cast<std::size_t>(to.layout)];
    const Area& next = there.areas[static_cast<std::size_t>(to.area)];
    const int entrance = findOpening(next, exit.leadsTo.opening);
    retropp::Vec2i origin{};
    if (attaches(exit) && isEdge(exit)) {
        origin = attachment(here, area, exit, there, to.area, next.openings[static_cast<std::size_t>(entrance)]);
    }
    return Link{.to = to, .entrance = entrance, .origin = origin};
}

// Calls `fn(opening, link)` for each seamless opening of an area that leads somewhere.
template <class Fn>
void eachSeamless(const World& world, int layout, int area, Fn&& fn) {
    const Area& here = world.layouts[static_cast<std::size_t>(layout)].areas[static_cast<std::size_t>(area)];
    for (const Opening& opening : here.openings) {
        if (opening.transition == Transition::Seamless && opening.leadsTo.area != kNoArea) {
            fn(opening, link(world, layout, area, opening));
        }
    }
}

// A layout, an area and an opening of a world as a message names them: by index and by name.
[[nodiscard]] inline std::string where(const World& world, int layout) {
    return "WorldKit: layout " + std::to_string(layout) + " '" +
           world.layouts[static_cast<std::size_t>(layout)].name + "'";
}

[[nodiscard]] inline std::string where(const World& world, int layout, int area) {
    return where(world, layout) + " area " + std::to_string(area) + " '" +
           world.layouts[static_cast<std::size_t>(layout)].areas[static_cast<std::size_t>(area)].name + "'";
}

[[nodiscard]] inline std::string where(const World& world, int layout, int area, const Opening& opening) {
    return where(world, layout, area) + " opening '" + opening.name + "'";
}

[[nodiscard]] inline const char* verb(Transition transition) noexcept {
    return transition == Transition::Scroll ? "scrolls" : "joins seamlessly";
}

}  // namespace detail

// Checks the world and throws std::invalid_argument, naming the layout, the area and the layer, slot
// or opening, for: a layout or an area whose name is empty or is the name of another layout of the world
// or another area of its layout; a cell size of zero or less; an area with no collision map or no layers; a layer or
// an actor slot with no name, or with the name of another layer or slot of its area; a layer that sets
// `art` and leaves `tiles` null (a map layer missing its map); a layer whose map is of another size than
// the collision map; a layer or slot at the depth of another of its area, or
// at a depth `z` for which `z × N` or `z × N + N − 1` leaves the range of std::int32_t, N being the
// number of areas in the world (the lowest and highest depths it can be drawn at); an opening with no
// name or a name used twice in its area; an opening that is
// neither in a wall nor a spot, or both; a span off its wall or a spot off the map; two openings that
// overlap; a destination naming a layout, an area or an opening that does not exist; a fade with an
// offset; a scroll that is not from a wall to the facing wall of the next area through a span of the
// same length; a seamless join from a spot, or that is not wall to facing wall through spans of one
// length, into a layout of another cell size, or whose far end is not seamless, does not lead back to
// it, or does not carry the negated offset; an actor entry whose `actor` is empty; an actor name
// used twice anywhere in the world, every level together; a layout's actor in an area the layout lacks;
// a world's actor in a place the world lacks. An actor entry is named by its level, its index and its
// name: `WorldKit: layout 0 'castle' actor 1 'hound' …`, `WorldKit: actor 0 'marker' …`. The type an
// entry's `actor` holds is the scene's to check. A game calls it once, after building the world.
inline void validate(const World& world) {
    std::int64_t areaCount = 0;
    for (const Layout& layout : world.layouts) {
        areaCount += static_cast<std::int64_t>(layout.areas.size());
    }
    // Every actor entry's name so far, in the order an entry is spawned: each area's, each layout's, the
    // world's.
    std::vector<const std::string*> actorNames;
    const auto checkActor = [&](const std::string& where, std::size_t n, const std::string& name, const AnyActor& actor) {
        const std::string named = where + " actor " + std::to_string(n) + " '" + name + "'";
        if (!actor) {
            throw std::invalid_argument(named + " has no actor");
        }
        if (name.empty()) {
            return;
        }
        for (const std::string* other : actorNames) {
            if (*other == name) {
                throw std::invalid_argument(named + " is named twice");
            }
        }
        actorNames.push_back(&name);
    };
    for (std::size_t l = 0; l < world.layouts.size(); ++l) {
        const Layout& layout = world.layouts[l];
        const std::string thisLayout = detail::where(world, static_cast<int>(l));
        if (layout.name.empty()) {
            throw std::invalid_argument("WorldKit: layout " + std::to_string(l) + " has no name");
        }
        for (std::size_t other = 0; other < l; ++other) {
            if (world.layouts[other].name == layout.name) {
                throw std::invalid_argument(thisLayout + " is named twice");
            }
        }
        if (layout.cellSize <= 0) {
            throw std::invalid_argument(thisLayout + "'s cell size must be positive");
        }
        for (std::size_t a = 0; a < layout.areas.size(); ++a) {
            const Area& area = layout.areas[a];
            const std::string here = detail::where(world, static_cast<int>(l), static_cast<int>(a));
            if (area.name.empty()) {
                throw std::invalid_argument(thisLayout + " area " + std::to_string(a) + " has no name");
            }
            for (std::size_t other = 0; other < a; ++other) {
                if (layout.areas[other].name == area.name) {
                    throw std::invalid_argument(here + " is named twice");
                }
            }
            if (area.collision == nullptr) {
                throw std::invalid_argument(here + " has no collision map");
            }
            if (area.layers.empty()) {
                throw std::invalid_argument(here + " has no layers");
            }
            // Every layer and actor slot of the area, by name and depth.
            std::vector<std::pair<const std::string*, std::int32_t>> seen;
            const auto add = [&](const std::string& name, std::int32_t z) {
                const std::string named = here + " layer '" + name + "'";
                for (const auto& [otherName, otherZ] : seen) {
                    if (*otherName == name) {
                        throw std::invalid_argument(named + " is named twice");
                    }
                    if (otherZ == z) {
                        throw std::invalid_argument(named + " is at the same depth as layer '" + *otherName + "'");
                    }
                }
                const std::int64_t lowest = std::int64_t{z} * areaCount;
                const std::int64_t highest = lowest + areaCount - 1;
                if (lowest < std::numeric_limits<std::int32_t>::min() || highest > std::numeric_limits<std::int32_t>::max()) {
                    throw std::invalid_argument(named + " is at a depth out of range for a world of " +
                                                std::to_string(areaCount) + " areas");
                }
                seen.emplace_back(&name, z);
            };
            for (const MapLayer& layer : area.layers) {
                if (layer.name.empty()) {
                    throw std::invalid_argument(here + " has a layer with no name");
                }
                const std::string named = here + " layer '" + layer.name + "'";
                // A reserved layer sets neither; a map layer sets both.
                if (layer.tiles == nullptr && !layer.art.entries.empty()) {
                    throw std::invalid_argument(named + " has art but no map");
                }
                if (layer.tiles != nullptr &&
                    (layer.tiles->width != area.collision->width || layer.tiles->height != area.collision->height)) {
                    throw std::invalid_argument(named + "'s map differs in size from the collision map");
                }
                add(layer.name, layer.z);
            }
            for (const ActorSlot& slot : area.actorSlots) {
                if (slot.name.empty()) {
                    throw std::invalid_argument(here + " has an actor slot with no name");
                }
                add(slot.name, slot.z);
            }
            for (std::size_t i = 0; i < area.openings.size(); ++i) {
                const Opening& opening = area.openings[i];
                const std::string at = detail::where(world, static_cast<int>(l), static_cast<int>(a), opening);
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
                const Place to = detail::resolve(opening.leadsTo, static_cast<int>(l));
                if (to.layout < 0 || static_cast<std::size_t>(to.layout) >= world.layouts.size()) {
                    throw std::invalid_argument(at + " leads to layout " + std::to_string(opening.leadsTo.layout) +
                                                ", which does not exist");
                }
                const Layout& there = world.layouts[static_cast<std::size_t>(to.layout)];
                const std::string thereName = "layout " + std::to_string(to.layout);
                if (to.area < 0 || static_cast<std::size_t>(to.area) >= there.areas.size()) {
                    throw std::invalid_argument(at + " leads to area " + std::to_string(to.area) + " of " + thereName +
                                                ", which does not exist");
                }
                const Area& next = there.areas[static_cast<std::size_t>(to.area)];
                const int entrance = detail::findOpening(next, opening.leadsTo.opening);
                if (entrance < 0) {
                    throw std::invalid_argument(at + " leads to '" + opening.leadsTo.opening + "' in area " +
                                                std::to_string(to.area) + " of " + thereName +
                                                ", which has no such opening");
                }
                if (opening.transition == Transition::Fade) {
                    if (opening.leadsTo.offset != retropp::Vec2i{}) {
                        throw std::invalid_argument(at + " fades, so it takes no offset");
                    }
                    continue;
                }
                const Opening& into = next.openings[static_cast<std::size_t>(entrance)];
                const char* verb = detail::verb(opening.transition);
                if (opening.transition == Transition::Seamless && !edge) {
                    throw std::invalid_argument(at + " joins seamlessly, so it must be in a wall");
                }
                if (!edge || into.side != detail::facing(opening.side)) {
                    throw std::invalid_argument(at + " " + verb + ", so it and '" + into.name +
                                                "' must be in facing walls");
                }
                if (into.span.last - into.span.first != opening.span.last - opening.span.first) {
                    throw std::invalid_argument(at + " " + verb + " into '" + into.name +
                                                "', whose span is a different length");
                }
                if (opening.transition != Transition::Seamless) {
                    continue;
                }
                if (there.cellSize != layout.cellSize) {
                    throw std::invalid_argument(at + " joins seamlessly into " + thereName +
                                                ", whose cell size differs");
                }
                if (into.transition != Transition::Seamless) {
                    throw std::invalid_argument(at + " joins seamlessly into '" + into.name + "', which is not seamless");
                }
                const Place back = detail::resolve(into.leadsTo, to.layout);
                if (into.leadsTo.area == kNoArea || back != Place{.layout = static_cast<int>(l), .area = static_cast<int>(a)} ||
                    into.leadsTo.opening != opening.name) {
                    throw std::invalid_argument(at + " joins seamlessly into '" + into.name +
                                                "', which does not lead back to it");
                }
                if (into.leadsTo.offset != retropp::Vec2i{.x = -opening.leadsTo.offset.x, .y = -opening.leadsTo.offset.y}) {
                    throw std::invalid_argument(at + " joins seamlessly into '" + into.name +
                                                "', whose offset is not the negation of its own");
                }
            }
            for (std::size_t n = 0; n < area.actors.size(); ++n) {
                checkActor(here, n, area.actors[n].name, area.actors[n].actor);
            }
        }
        for (std::size_t n = 0; n < layout.actors.size(); ++n) {
            const LayoutActor& entry = layout.actors[n];
            checkActor(thisLayout, n, entry.name, entry.actor);
            if (entry.in < 0 || static_cast<std::size_t>(entry.in) >= layout.areas.size()) {
                throw std::invalid_argument(thisLayout + " actor " + std::to_string(n) + " '" + entry.name +
                                            "' is in area " + std::to_string(entry.in) + ", which the layout lacks");
            }
        }
    }
    for (std::size_t n = 0; n < world.actors.size(); ++n) {
        const WorldActor& entry = world.actors[n];
        checkActor("WorldKit:", n, entry.name, entry.actor);
        const bool inWorld = entry.in.layout >= 0 && static_cast<std::size_t>(entry.in.layout) < world.layouts.size() &&
                             entry.in.area >= 0 &&
                             static_cast<std::size_t>(entry.in.area) <
                                 world.layouts[static_cast<std::size_t>(entry.in.layout)].areas.size();
        if (!inWorld) {
            throw std::invalid_argument("WorldKit: actor " + std::to_string(n) + " '" + entry.name + "' is in layout " +
                                        std::to_string(entry.in.layout) + " area " + std::to_string(entry.in.area) +
                                        ", which the world lacks");
        }
    }
}

// The depth the game declares for the actor slot named `slot` in the area at `place`: the depth it is
// drawn at while that area is alone on screen. The depth it is drawn at among the areas on screen is
// the form that takes the area's Shown (camera.h). Throws std::out_of_range, naming the layout, the
// area and the slot, when `slot` is missing from the area's slots.
[[nodiscard]] inline std::int32_t ActorZ(const World& world, Place place, std::string_view slot) {
    const Area& area = world.layouts[static_cast<std::size_t>(place.layout)].areas[static_cast<std::size_t>(place.area)];
    for (const ActorSlot& candidate : area.actorSlots) {
        if (candidate.name == slot) {
            return candidate.z;
        }
    }
    throw std::out_of_range(detail::where(world, place.layout, place.area) + " has no actor slot '" + std::string{slot} + "'");
}

// The depth the game declares for the layer named `name` in the area at `place`, a reserved layer or a
// map layer: the depth it is drawn at while that area is alone on screen. The depth it is drawn at
// among the areas on screen is the form that takes the area's Shown (camera.h). Throws
// std::out_of_range, naming the layout, the area and the layer, when `name` is missing from the area's layers.
[[nodiscard]] inline std::int32_t LayerZ(const World& world, Place place, std::string_view name) {
    const Area& area = world.layouts[static_cast<std::size_t>(place.layout)].areas[static_cast<std::size_t>(place.area)];
    for (const MapLayer& candidate : area.layers) {
        if (candidate.name == name) {
            return candidate.z;
        }
    }
    throw std::out_of_range(detail::where(world, place.layout, place.area) + " has no layer '" + std::string{name} + "'");
}

// The place of the area named `area` in the layout named `layout`: for a game that names its places
// where it uses them, a warp table or a save file, say.
//
//     const WorldKit::Place treasury = WorldKit::placeOf(world, "dungeon-one", "treasury");
//     state.camera = WorldKit::Camera{treasury.layout, treasury.area};
//
// Throws std::out_of_range when `layout` is missing from the world's layouts ("WorldKit: the world has no
// layout '…'") or `area` from that layout's areas ("WorldKit: layout N '…' has no area '…'").
[[nodiscard]] inline Place placeOf(const World& world, std::string_view layout, std::string_view area) {
    for (std::size_t l = 0; l < world.layouts.size(); ++l) {
        if (world.layouts[l].name != layout) {
            continue;
        }
        const std::vector<Area>& areas = world.layouts[l].areas;
        for (std::size_t a = 0; a < areas.size(); ++a) {
            if (areas[a].name == area) {
                return Place{.layout = static_cast<int>(l), .area = static_cast<int>(a)};
            }
        }
        throw std::out_of_range(detail::where(world, static_cast<int>(l)) + " has no area '" + std::string{area} + "'");
    }
    throw std::out_of_range("WorldKit: the world has no layout '" + std::string{layout} + "'");
}

}  // namespace WorldKit
