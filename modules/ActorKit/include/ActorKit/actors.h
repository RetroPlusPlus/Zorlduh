// Actors — the live actors: one value in the game's state, holding every actor from its spawn to its
// removal.
//
//     ActorKit::Actors actors{engineConfig};                         // in the game's state
//
//     const ActorKit::Id player = actors.spawn(hero, {Farm, Yard}, {72, 96});
//     actors.follow(player);                                         // the actor the game's camera follows
//     const ActorKit::Id hen = actors.spawn(henOf(brown), {Farm, Field}, {40, 32});
//     actors.remove(hen);
//
//     actors.box(player);        // asked, an actor reports where it is: its place, position, box, facing
//
// Spawn copies the value it is handed, so any number of actors come from one value, at any time. Each
// live actor has its place and position, its own values, its mover and its scripts' running state; the
// tick (tick.h) runs them all and Sprites (sprites.h) reports what they show. An Actors value is plain
// data: a copy runs on by itself from where the original was.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "retropp/engine_config.h"
#include "retropp/geometry.h"
#include "retropp/input.h"
#include "retropp/input_actions.h"

#include "ScriptKit/expression.h"
#include "ScriptKit/script.h"
#include "ScriptKit/script_processor.h"
#include "ScriptKit/value.h"

#include "WorldKit/area.h"

#include "ActorKit/actor.h"
#include "ActorKit/mover.h"
#include "ActorKit/values.h"

namespace ActorKit {

namespace detail {

// One live actor.
struct Live {
    Live(Id idOf, std::shared_ptr<const Actor> actorOf, WorldKit::Place placeOf, retropp::Vec2i at,
         ScriptKit::ScriptProcessor processorOf)
        : id(idOf), actor(std::move(actorOf)), place(placeOf), position(at), processor(std::move(processorOf)) {}

    // A copy whose mover walks the copy's own position.
    Live(const Live& other)
        : id(other.id), actor(other.actor), place(other.place), position(other.position),
          mover(other.mover ? other.mover->copyFor(position) : nullptr), processor(other.processor), own(other.own),
          gameReads(other.gameReads), newlySpawned(other.newlySpawned), started(other.started),
          removing(other.removing), solid(other.solid), touched(other.touched), touching(other.touching),
          within(other.within), wasTrue(other.wasTrue), queued(other.queued), shownIn(other.shownIn),
          shownAt(other.shownAt), freshReported(other.freshReported), keyPhase(other.keyPhase) {}

    Live& operator=(const Live&) = delete;

    Id id;
    std::shared_ptr<const Actor> actor;
    WorldKit::Place place;
    retropp::Vec2i position;
    std::unique_ptr<MoverState> mover;
    ScriptKit::ScriptProcessor processor;
    ScriptKit::Values own;
    std::vector<ScriptKit::ValueId> gameReads;   // every value it reads that is the game's

    bool newlySpawned = true;   // spawned since the current tick began: it takes part from the next
    bool started = false;       // it has taken part in a tick
    bool removing = false;   // removed during the requests of a tick, gone when they are written
    bool solid = false;      // its `solid`, as worked out this tick
    bool touched = false;    // ActorKit::Touched
    std::vector<std::pair<std::uint32_t, Id>> touching;   // the touch triggers and their setters overlapping it
    std::vector<char> within;                             // per script: a near trigger's actor within reach
    std::vector<char> wasTrue;                            // per script: a becomes trigger's value
    std::vector<retropp::ActionId> queued;                // actions asked for with Actors::act

    WorldKit::Place shownIn{};     // where it was when the last tick began: what the last frame showed
    retropp::Vec2i shownAt{};
    bool freshReported = false;    // Actors::fresh was called since the last tick began
    std::uint8_t keyPhase = 0;     // the key it was last shown under: 0 its own, 1 and 2 the throwaway keys
};

// The next number for an Actors value: 1 and up, in the order they are constructed.
[[nodiscard]] inline std::uint32_t mintSetNumber() noexcept {
    static std::uint32_t next = 0;
    return ++next;
}

// Which key a live actor's sprite has now: its own (0) unless Actors::fresh is reported and it has moved
// from where the last frame showed it, when it takes the other throwaway key from the one it had last.
[[nodiscard]] inline std::uint8_t keyPhaseNow(const Live& live) noexcept {
    if (!live.freshReported) {
        return 0;
    }
    const bool moved = live.place != live.shownIn || live.position != live.shownAt;
    return moved ? static_cast<std::uint8_t>(live.keyPhase == 1 ? 2 : 1) : live.keyPhase;
}

// The sprite key of a live actor of the Actors value numbered `set`.
[[nodiscard]] inline std::string keyOf(std::uint32_t set, const Live& live) {
    std::string key = "actor:" + std::to_string(set) + ":" + std::to_string(live.id);
    const std::uint8_t phase = keyPhaseNow(live);
    if (phase != 0) {
        key += phase == 1 ? ":a" : ":b";
    }
    return key;
}

// The relative box of a live actor: its mover's, or its own.
[[nodiscard]] inline retropp::IntRect relativeBox(const Live& live) {
    return live.mover ? live.mover->box() : live.actor->box;
}

// The box of a live actor in its area's pixels.
[[nodiscard]] inline retropp::IntRect areaBox(const Live& live) {
    retropp::IntRect box = relativeBox(live);
    box.x += live.position.x;
    box.y += live.position.y;
    return box;
}

[[nodiscard]] inline bool overlap(retropp::IntRect a, retropp::IntRect b) noexcept {
    return a.width > 0 && a.height > 0 && b.width > 0 && b.height > 0 && a.x < b.x + b.width &&
           b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}

struct ActorsAccess;

}  // namespace detail

class Actors {
public:
    // Live actors whose scripts' waits are timed by `config.timing`.
    explicit Actors(const retropp::EngineConfig& config)
        : config_(std::make_shared<const retropp::EngineConfig>(config)), set_(detail::mintSetNumber()) {}

    Actors(const Actors& other)
        : config_(other.config_), set_(other.set_), ids_(other.ids_), next_(other.next_),
          followed_(other.followed_), lastCameraPlace_(other.lastCameraPlace_), prototypes_(other.prototypes_) {
        live_.reserve(other.live_.size());
        for (const std::unique_ptr<detail::Live>& live : other.live_) {
            live_.push_back(std::make_unique<detail::Live>(*live));
        }
    }

    Actors& operator=(const Actors& other) {
        if (this != &other) {
            Actors copy(other);
            *this = std::move(copy);
        }
        return *this;
    }

    Actors(Actors&&) noexcept = default;
    Actors& operator=(Actors&&) noexcept = default;

    // Spawns a copy of `actor` in `place`, its top-left at `at`, and returns its id. The actor takes part
    // from the next call to tick. Throws std::invalid_argument for an actor with a mover and a box of its
    // own, an own value listed twice, or a null script; ScriptKit's refusals of the actor's scripts — two
    // of one name, a script naming one outside the list — pass through as std::logic_error.
    Id spawn(const Actor& actor, WorldKit::Place place, retropp::Vec2i at) {
        const std::string named = "ActorKit::Actors::spawn: ";
        if (actor.mover && (actor.box.width != 0 || actor.box.height != 0)) {
            throw std::invalid_argument(named + "an actor with a mover takes its box from the mover");
        }
        for (std::size_t n = 0; n < actor.ownValues.size(); ++n) {
            for (std::size_t earlier = 0; earlier < n; ++earlier) {
                if (actor.ownValues[earlier].id() == actor.ownValues[n].id()) {
                    throw std::invalid_argument(named + "the value with id " + std::to_string(actor.ownValues[n].id()) +
                                                " is listed twice in ownValues");
                }
            }
        }
        for (const ScriptRef& script : actor.scripts) {
            if (script.get() == nullptr) {
                throw std::invalid_argument(named + "a null script");
            }
        }

        auto kind = std::make_shared<const Actor>(actor);
        auto live = std::make_unique<detail::Live>(next_, kind, place, at, prototypeFor(*kind));
        for (const OwnValue& value : kind->ownValues) {
            live->own.setRaw(value.id(), value.start());
        }
        if (kind->mover) {
            live->mover = kind->mover.kind()->make(live->position);
        }
        live->gameReads = gameReadsOf(*live);
        live->within.assign(kind->scripts.size(), 0);
        live->wasTrue.assign(kind->scripts.size(), 0);
        live->shownIn = place;
        live->shownAt = at;

        live_.push_back(std::move(live));
        ids_.push_back(next_);
        return next_++;
    }

    // Removes the actor `id`. Returns true when the actor was live, false otherwise. Inside a tick —
    // from a handler — it takes effect once that tick's requests are all written. Removing the followed
    // actor leaves followed() at kNoId.
    bool remove(Id id) {
        detail::Live* live = find(id);
        if (live == nullptr || live->removing) {
            return false;
        }
        if (deferring_) {
            live->removing = true;
            return true;
        }
        erase(id);
        return true;
    }

    // Records `id` as the followed actor, the one the game's camera follows; kNoId for none. The followed
    // actor walks out through its area's openings, where every other actor stops at its area's edge, and
    // the values FollowedHere, FollowedX and FollowedY are filled from it. Moving the camera is the game's
    // own call to WorldKit, with what followed(), box and place report.
    void follow(Id id) {
        if (id != kNoId) {
            (void)require(id);
        }
        followed_ = id;
    }

    // The followed actor; kNoId for none.
    [[nodiscard]] Id followed() const noexcept { return followed_; }

    // Whether an actor `id` is live.
    [[nodiscard]] bool has(Id id) const noexcept { return find(id) != nullptr; }

    // How many actors are live.
    [[nodiscard]] std::size_t size() const noexcept { return ids_.size(); }

    // The live actors' ids, in spawn order.
    [[nodiscard]] std::span<const Id> ids() const noexcept { return ids_; }

    // Where the actor is: its layout and area.
    [[nodiscard]] WorldKit::Place place(Id id) const { return require(id).place; }

    // Its top-left, in its area's pixels.
    [[nodiscard]] retropp::Vec2i position(Id id) const { return require(id).position; }

    // Its box in its area's pixels: the mover's collision bounds, or the actor's own box.
    [[nodiscard]] retropp::IntRect box(Id id) const { return detail::areaBox(require(id)); }

    // The way it faces: its mover's facing; Dir::Down for an actor whose `mover` is empty.
    [[nodiscard]] retropp::Dir facing(Id id) const {
        const detail::Live& live = require(id);
        return live.mover ? live.mover->facing() : retropp::Dir::Down;
    }

    // Whether the two actors are in one area with their boxes overlapping.
    [[nodiscard]] bool touching(Id a, Id b) const {
        const detail::Live& first = require(a);
        const detail::Live& second = require(b);
        return first.place == second.place && detail::overlap(detail::areaBox(first), detail::areaBox(second));
    }

    // Puts the actor at `at` in `place` at once. It is how the game carries an actor where WorldKit's
    // camera says it goes — through a scroll, a fade or a seam — and how it warps one.
    void moveTo(Id id, WorldKit::Place place, retropp::Vec2i at) {
        detail::Live& live = require(id);
        live.place = place;
        live.position = at;
    }

    // The key the actor's sprite has now: "actor:<set>:<id>", where <set> is a number this Actors value
    // took when it was constructed, so the actors of every Actors value in a frame have keys of their
    // own. A copy of an Actors value keeps the number. While fresh is reported and the actor has moved,
    // the key is a throwaway one.
    [[nodiscard]] std::string key(Id id) const { return detail::keyOf(set_, require(id)); }

    // Reports that the actor's sprite is fresh this tick: where it is now is shown at once, in place of
    // easing from where the last frame showed it. Call it after tick, on each tick it is wanted — the
    // tick of a warp, or every tick for an actor always shown at once. While it is reported and the actor has
    // moved since the last frame, its sprite goes out under a throwaway key, a different one from the
    // tick before, which the renderer mounts as a new sprite. Reported for an actor that stayed where it
    // was, the key stays as it is. On the first tick it goes unreported, the key is the actor's own again.
    void fresh(Id id) { require(id).freshReported = true; }

    // What the actor's own `value` is worth. Throws std::out_of_range naming the id for a value outside the
    // actor's `ownValues`.
    template <ScriptKit::ValueType T>
    [[nodiscard]] T get(Id id, ScriptKit::Value<T> value) const {
        const detail::Live& live = require(id);
        requireOwn(live, value.id());
        return live.own.get(value);
    }

    // Sets the actor's own `value` to `worth`, as a script's `set` would. Throws as get does.
    template <ScriptKit::ValueType T>
    void set(Id id, ScriptKit::Value<T> value, T worth) {
        detail::Live& live = require(id);
        requireOwn(live, value.id());
        live.own.set(value, worth);
    }

    // Has the actor perform `action` on the actor it faces, on the next tick, as its own action rows do.
    template <retropp::ActionLike A>
    void act(Id id, A action) {
        require(id).queued.push_back(retropp::actionId(action));
    }

    // The actor's script processor, for a readout and for tests.
    [[nodiscard]] const ScriptKit::ScriptProcessor& processor(Id id) const { return require(id).processor; }

    // Every member that takes an id, except has and remove, throws std::out_of_range naming the id unless
    // a live actor has it.

private:
    friend struct detail::ActorsAccess;

    // A processor over one list of scripts, built once and copied for each actor spawned with that list.
    struct Prototype {
        std::vector<std::shared_ptr<const ScriptKit::Script>> scripts;
        ScriptKit::ScriptProcessor processor;
    };

    [[nodiscard]] ScriptKit::ScriptProcessor prototypeFor(const Actor& actor) {
        std::vector<const ScriptKit::Script*> list;
        list.reserve(actor.scripts.size());
        for (const ScriptRef& script : actor.scripts) {
            list.push_back(script.get());
        }
        for (const Prototype& prototype : prototypes_) {
            if (prototype.scripts.size() == list.size() &&
                std::equal(list.begin(), list.end(), prototype.scripts.begin(),
                           [](const ScriptKit::Script* a, const std::shared_ptr<const ScriptKit::Script>& b) {
                               return a == b.get();
                           })) {
                return prototype.processor;
            }
        }
        std::vector<std::shared_ptr<const ScriptKit::Script>> kept;
        kept.reserve(actor.scripts.size());
        for (const ScriptRef& script : actor.scripts) {
            kept.push_back(script.shared());
        }
        prototypes_.push_back(Prototype{.scripts = std::move(kept), .processor = ScriptKit::ScriptProcessor(list, *config_)});
        return prototypes_.back().processor;
    }

    // Every value the actor reads through the game's bindings — the ones outside its own and ActorKit's
    // six: its scripts', its `solid`'s, its looks', its triggers' and its mover's.
    [[nodiscard]] static std::vector<ScriptKit::ValueId> gameReadsOf(const detail::Live& live) {
        std::vector<ScriptKit::ValueId> ids;
        const auto add = [&](ScriptKit::ValueId id) {
            if (id == 0 || detail::provided(id) || live.own.has(id) ||
                std::find(ids.begin(), ids.end(), id) != ids.end()) {
                return;
            }
            ids.push_back(id);
        };
        for (const ScriptKit::ValueId id : live.processor.reads()) {
            add(id);
        }
        for (const ScriptKit::ValueId id : ScriptKit::valuesOf(live.actor->solid)) {
            add(id);
        }
        for (const LookRule& rule : live.actor->look.rules()) {
            for (const ScriptKit::ValueId id : ScriptKit::valuesOf(rule.when)) {
                add(id);
            }
        }
        for (const ScriptRef& script : live.actor->scripts) {
            add(script.get()->on.value);
        }
        if (live.mover) {
            add(live.actor->mover.kind()->reads());
        }
        return ids;
    }

    [[nodiscard]] detail::Live* find(Id id) const noexcept {
        for (const std::unique_ptr<detail::Live>& live : live_) {
            if (live->id == id) {
                return live.get();
            }
        }
        return nullptr;
    }

    [[nodiscard]] detail::Live& require(Id id) const {
        detail::Live* live = find(id);
        if (live == nullptr) {
            throw std::out_of_range("ActorKit::Actors: no live actor has id " + std::to_string(id));
        }
        return *live;
    }

    static void requireOwn(const detail::Live& live, ScriptKit::ValueId value) {
        if (!live.own.has(value)) {
            throw std::out_of_range("ActorKit::Actors: actor " + std::to_string(live.id) +
                                    " owns no value with id " + std::to_string(value));
        }
    }

    void erase(Id id) {
        live_.erase(std::remove_if(live_.begin(), live_.end(),
                                   [id](const std::unique_ptr<detail::Live>& live) { return live->id == id; }),
                    live_.end());
        ids_.erase(std::remove(ids_.begin(), ids_.end(), id), ids_.end());
        if (followed_ == id) {
            followed_ = kNoId;
        }
    }

    std::shared_ptr<const retropp::EngineConfig> config_;
    std::uint32_t set_ = 0;                             // this value's number, in every one of its sprite keys
    std::vector<std::unique_ptr<detail::Live>> live_;   // in spawn order; each at one address for its life
    std::vector<Id> ids_;
    Id next_ = 1;
    Id followed_ = kNoId;
    std::optional<WorldKit::Place> lastCameraPlace_;    // the camera's place on the last tick
    bool deferring_ = false;                            // removals wait for the requests to be written
    std::vector<Prototype> prototypes_;
    ScriptKit::Values scratch_;                         // where the tick gathers one actor's values
};

namespace detail {

// What the tick and the frame reach inside an Actors value.
struct ActorsAccess {
    [[nodiscard]] static std::vector<std::unique_ptr<Live>>& live(Actors& actors) noexcept { return actors.live_; }
    [[nodiscard]] static const std::vector<std::unique_ptr<Live>>& live(const Actors& actors) noexcept {
        return actors.live_;
    }
    [[nodiscard]] static Live* find(const Actors& actors, Id id) noexcept { return actors.find(id); }
    [[nodiscard]] static std::optional<WorldKit::Place>& lastCameraPlace(Actors& actors) noexcept {
        return actors.lastCameraPlace_;
    }
    [[nodiscard]] static ScriptKit::Values& scratch(Actors& actors) noexcept { return actors.scratch_; }
    [[nodiscard]] static std::uint32_t set(const Actors& actors) noexcept { return actors.set_; }

    // The start of a tick: every actor spawned before it takes part; each actor's key settles as the last
    // frame showed it, where it is now becomes where it was shown, and the fresh reports lapse.
    static void begin(Actors& actors) noexcept {
        for (const std::unique_ptr<Live>& live : actors.live_) {
            live->newlySpawned = false;
            live->keyPhase = keyPhaseNow(*live);
            live->shownIn = live->place;
            live->shownAt = live->position;
            live->freshReported = false;
        }
    }

    // Requests are being written: removals wait.
    static void defer(Actors& actors, bool on) { actors.deferring_ = on; }

    // Erases every actor removed while removals waited.
    static void applyRemovals(Actors& actors) {
        std::vector<Id> gone;
        for (const std::unique_ptr<Live>& live : actors.live_) {
            if (live->removing) {
                gone.push_back(live->id);
            }
        }
        for (const Id id : gone) {
            actors.erase(id);
        }
    }
};

}  // namespace detail

}  // namespace ActorKit
