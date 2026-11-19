// What an actor shows: a look from a sheet, or the first of a list of looks whose condition holds.
//
//     const ActorKit::Look shut = ActorKit::lookOf(sheet, kGateShut, gatePalette);
//     const ActorKit::Look open = ActorKit::lookOf(sheet, kGateOpen, gatePalette);
//
//     ActorKit::Actor gate{.look = {{GateOpen, open}, {shut}}};   // open while GateOpen holds, else shut
//     ActorKit::Actor egg{.look = eggLook};                       // always the one look
//
// A condition reads the actor's values — its own, ActorKit's and the game's — on the frame the actor is
// drawn. An actor with a mover shows the mover's frame while every look's condition is false.
#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

#include "retropp/atlas_manifest.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/palette.h"

#include "ScriptKit/expression.h"

namespace ActorKit {

// The art an actor shows: the fields of a retropp::Sprite that say which art and how it is turned.
struct Look {
    retropp::AssetDimensions size{};
    retropp::AtlasId atlas{};
    std::uint16_t tile = 0;
    retropp::PaletteId palette{};
    bool flipX = false;
    bool flipY = false;

    [[nodiscard]] constexpr bool operator==(const Look&) const noexcept = default;
};

// The look of slot `slot` of `sheet`, through `palette`: the slot's cell and size. Throws
// std::out_of_range for a slot past the sheet's last.
[[nodiscard]] inline Look lookOf(const retropp::AtlasManifest& sheet, std::size_t slot, retropp::PaletteId palette) {
    const retropp::AssetSlot& found = sheet.slots.at(slot);
    return Look{.size = found.dimensions, .atlas = sheet.atlasId, .tile = found.tile, .palette = palette};
}

// A look shown while `when` holds: {GateOpen, open}, {!Awake, asleep}. Written as one look alone, it
// always holds.
struct LookRule {
    ScriptKit::Expr<bool> when = true;
    Look look{};

    LookRule(Look shown) : look(shown) {}
    LookRule(ScriptKit::Expr<bool> condition, Look shown) : when(std::move(condition)), look(shown) {}
};

// An actor's looks, in order: the first whose `when` holds is the one shown. Made from one Look, or
// from a braced list of rules.
class Looks {
public:
    Looks() = default;
    Looks(Look look) : rules_{LookRule{look}} {}
    Looks(std::initializer_list<LookRule> rules) : rules_(rules) {}

    [[nodiscard]] const std::vector<LookRule>& rules() const noexcept { return rules_; }
    [[nodiscard]] bool empty() const noexcept { return rules_.empty(); }

private:
    std::vector<LookRule> rules_;
};

}  // namespace ActorKit
