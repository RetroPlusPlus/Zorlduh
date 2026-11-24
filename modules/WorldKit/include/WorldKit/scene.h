// The scene: a world running. The game hands it the world, the camera it created and its ActorKit config;
// the scene spawns every actor the world lists, runs them each tick, carries them through the openings,
// moves the camera with the actor it follows, and draws it all.
//
//     WorldKit::Camera camera{Layout::Castle, CastleArea::Hall};
//     State state{.scene = WorldKit::Scene{world, camera, config}};
//     state.scene.follow("marker");                                  // the camera follows this actor now
//
//     loop.simTick([&](const retropp::InputState& in) { state.scene.tick(readInput(in), state); });
//     loop.renderLoop([&] {
//         frame.layers  = state.scene.layers(state);
//         frame.regions = state.scene.fade();
//         renderer.renderFrame(frame);
//     });
//
// `world` is a validated World whose areas, layouts and world list their actors (area.h); `config` is the
// game's ActorKit::Config with its `world` left out. Every actor the scene holds is an ordinary ActorKit
// actor in one ActorKit::Actors set, reached through actors(); the camera is reached through camera().
// Each actor also has the scene's own settings, reached through actor(): how far it may roam, what it does
// at the edge of that, and when it is awake. A Scene is a plain value: a copy runs on by itself from where
// the original was. This header includes ActorKit's, so a game that includes it links ActorKit::ActorKit.
#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "retropp/engine_config.h"
#include "retropp/geometry.h"

#include "CameraKit/camera.h"
#include "LayerKit/sprite_layer.h"
#include "LayerKit/types.h"

#include "WorldKit/area.h"
#include "WorldKit/camera.h"
#include "WorldKit/world_layers.h"

#include "ActorKit/actor.h"
#include "ActorKit/actors.h"
#include "ActorKit/config.h"
#include "ActorKit/sprites.h"
#include "ActorKit/tick.h"

namespace WorldKit {

// The key of the sprite layer the scene draws for the actor slots named `slot` of every area on screen:
// "scene:<slot>". It stays the same as the current area changes, so the layer's scroll eases across the
// change.
[[nodiscard]] inline std::string SceneLayerKey(std::string_view slot) { return "scene:" + std::string{slot}; }

namespace detail {

// Where a hidden actor is: layout −1, area −1, a place outside every world.
inline constexpr Place kNowhere{.layout = -1, .area = -1};

// Whether `place` is an area of `world`.
[[nodiscard]] inline bool inWorld(const World& world, Place place) noexcept {
    return place.layout >= 0 && static_cast<std::size_t>(place.layout) < world.layouts.size() && place.area >= 0 &&
           static_cast<std::size_t>(place.area) < world.layouts[static_cast<std::size_t>(place.layout)].areas.size();
}

// The confinement that lets an actor's walk reach the edge of its reach: held at every opening outside it
// to Stop, let out through every opening that leads somewhere to Despawn or Hide.
[[nodiscard]] inline ActorKit::Confinement confinementOf(Reach reach, AtEdge atEdge) noexcept {
    if (atEdge != AtEdge::Stop) {
        return ActorKit::Confinement::None;
    }
    switch (reach) {
        case Reach::Area: return ActorKit::Confinement::Area;
        case Reach::Layout: return ActorKit::Confinement::Layout;
        case Reach::World:
        default: return ActorKit::Confinement::None;
    }
}

}  // namespace detail

class Scene;

// One actor of a scene, as the scene keeps it: its reach, what it does at the edge of its reach, when it
// is awake, and the freeze the game commands. Returned by Scene::actor and valid while that scene is. A
// setting is set and read by the same word; a state the game commands is done by a verb and reported by
// a bare word.
//
//     scene.actor("hound").reach(WorldKit::Reach::World);           // the hound goes anywhere from now
//     scene.actor(bat).awakeWhen(WorldKit::AwakeWhen::OnScreen);     // the bat flies while it is on screen
//     if (scene.actor(bat).frozen()) { … }
//
// Every member but id throws std::out_of_range, naming the id, once its actor is removed.
class SceneActor {
public:
    // The actor's ActorKit id, the one its Actors set and every ActorKit call know it by.
    [[nodiscard]] ActorKit::Id id() const noexcept { return id_; }

    // Sets how far the actor may roam. It governs the actor's next crossing of an opening; an actor that
    // stands outside a reach made smaller stays where it is.
    void reach(Reach reach);

    // How far the actor may roam: the level it is written at until reach is set.
    [[nodiscard]] Reach reach() const;

    // Sets what the actor does at an opening that leads outside its reach.
    void atEdge(AtEdge atEdge);

    // What the actor does at an opening that leads outside its reach.
    [[nodiscard]] AtEdge atEdge() const;

    // Sets when the actor is awake. The rule decides from the next tick.
    void awakeWhen(AwakeWhen awakeWhen);

    // When the actor is awake.
    [[nodiscard]] AwakeWhen awakeWhen() const;

    // Freezes the actor from now on, whatever its rule says, until wake.
    void freeze();

    // Lifts a freeze made by freeze and hands the actor back to its rule: it wakes at once when its rule
    // holds for the areas on screen now and it is in the world, and stays frozen otherwise.
    void wake();

    // Whether the actor is paused: frozen by hand, by its rule, or hidden.
    [[nodiscard]] bool frozen() const;

    // Whether the actor takes part in the tick: the opposite of frozen.
    [[nodiscard]] bool awake() const;

    // Whether AtEdge::Hide has put the actor out of the world. It comes back when the game puts it in a
    // place of the world with Actors::moveTo.
    [[nodiscard]] bool hidden() const;

private:
    friend class Scene;

    SceneActor(Scene* scene, ActorKit::Id id) noexcept : scene_(scene), id_(id) {}

    Scene* scene_;
    ActorKit::Id id_;
};

class Scene {
public:
    // The scene of `world`, with `camera` handed over and `config` with its `world` set to `world`. Every
    // actor the world lists is spawned, in the order the world is written: for each layout, each area's
    // actors and then the layout's; then the world's. Each takes the reach of the level it is written at,
    // its entry's `atEdge` and `awakeWhen`. Its scripts' waits are timed by retropp::EngineConfig::active.
    // `world` outlives the scene, and so do the bindings and handlers `config` names. Throws
    // std::logic_error, naming the entry, for an entry whose `actor` holds something other than an
    // ActorKit::Actor; what ActorKit::Actors::spawn throws passes through.
    Scene(const World& world, Camera camera, ActorKit::Config config)
        : world_(&world), camera_(std::move(camera)), config_(std::move(config)),
          actors_(retropp::EngineConfig::active) {
        config_.world = world_;
        for (std::size_t l = 0; l < world.layouts.size(); ++l) {
            const Layout& layout = world.layouts[l];
            for (std::size_t a = 0; a < layout.areas.size(); ++a) {
                for (const AreaActor& entry : layout.areas[a].actors) {
                    (void)spawn(entry, Place{.layout = static_cast<int>(l), .area = static_cast<int>(a)});
                }
            }
            for (const LayoutActor& entry : layout.actors) {
                (void)spawn(entry, static_cast<int>(l));
            }
        }
        for (const WorldActor& entry : world.actors) {
            (void)spawn(entry);
        }
    }

    // The scene of `world`, for actors that use the world alone: its config is ActorKit::Config{} with the
    // world filled in.
    Scene(const World& world, Camera camera) : Scene(world, std::move(camera), ActorKit::Config{}) {}

    // The camera follows the actor `id` from the next tick, and the actor is its set's player
    // (ActorKit::Actors::player): it goes out through every opening that leads somewhere, whatever its
    // reach and `atEdge`, and the camera carries it. Following another actor hands the first back to its
    // reach and `atEdge`. kNoId clears it, and the camera stays where it is. Throws std::out_of_range for
    // any id but a live actor's.
    void follow(ActorKit::Id id) { actors_.player(id); }

    // The camera follows the actor named `name`. Throws std::out_of_range, "WorldKit::Scene: no actor is
    // named '…'", for any name but a live actor's.
    void follow(std::string_view name) { follow(idNamed(name)); }

    // The actor the camera follows; kNoId before the first follow, after follow(kNoId), and once the actor
    // followed is removed.
    [[nodiscard]] ActorKit::Id following() const noexcept { return actors_.player(); }

    // The actor `id`, for the scene's settings of it. Throws std::out_of_range, naming the id, for any id
    // but a live actor's.
    [[nodiscard]] SceneActor actor(ActorKit::Id id) {
        (void)rowOf(id);
        return SceneActor{this, id};
    }

    // The actor named `name`. Throws std::out_of_range, "WorldKit::Scene: no actor is named '…'", for any
    // name but a live actor's.
    [[nodiscard]] SceneActor actor(std::string_view name) { return SceneActor{this, idNamed(name)}; }

    // Spawns the actor of `entry` in `place`, with Reach::Area, and returns its id. It takes part from the
    // next tick. Throws std::invalid_argument, "WorldKit::Scene: an actor is already named '…'", for a name
    // a live actor has, and std::logic_error, naming the entry, for an `actor` that holds something other
    // than an ActorKit::Actor; what ActorKit::Actors::spawn throws passes through.
    ActorKit::Id spawn(const AreaActor& entry, Place place) {
        return spawnRow(entry.name, entry.actor, place, entry.at, Reach::Area, entry.atEdge, entry.awakeWhen);
    }

    // Spawns the actor of `entry` in area `entry.in` of layout `layout`, with Reach::Layout. Throws as the
    // AreaActor form does.
    ActorKit::Id spawn(const LayoutActor& entry, int layout) {
        return spawnRow(entry.name, entry.actor, Place{.layout = layout, .area = entry.in}, entry.at, Reach::Layout,
                        entry.atEdge, entry.awakeWhen);
    }

    // Spawns the actor of `entry` in `entry.in`, with Reach::World. Throws as the AreaActor form does.
    ActorKit::Id spawn(const WorldActor& entry) {
        return spawnRow(entry.name, entry.actor, entry.in, entry.at, Reach::World, entry.atEdge, entry.awakeWhen);
    }

    // One tick of the scene, returning the camera's Step for the game's own use of `entered`. `input` is
    // the game's input value and `state` its state, as ActorKit::tick takes them. In this order:
    //
    //   1. Every live actor new to the scene — one spawned by a script, or through actors() — takes
    //      Reach::Area, AtEdge::Stop and AwakeWhen::Always. The settings of an actor removed are dropped.
    //   2. A hidden actor the game has put in a place of the world comes back into it, shown fresh.
    //   3. When the actor followed is in another place than the camera, the camera is put there, on the
    //      actor's box, any transition dropped, and the actor is shown fresh.
    //   4. Each actor is woken or frozen: awake when it is in the world, its freeze() lifted, and its rule
    //      holding for the areas the camera shows now.
    //   5. Each awake actor's box is noted.
    //   6. ActorKit::tick for every actor, against the camera — on a tick a scroll or a fade is running as
    //      it begins, left out, so the whole world holds still: every actor's scripts, waits, triggers
    //      and walking wait for the transition to end, and step 8 waits with them.
    //   7. The camera follows the followed actor's box, and the actor goes where the camera says: carried
    //      through an opening while a scroll runs, into the next area's pixels at a seam or at the scroll's
    //      end, and onto the entrance at the middle of a fade, shown fresh on that tick. While following()
    //      is kNoId, a scroll or a fade the last followed actor started runs to its end, and otherwise the
    //      camera is left as it is, for the game to drive by hand or to stay where it is.
    //   8. Each other awake actor that has gone through an opening this tick goes on by the opening's
    //      transition when the opening leads inside its reach: through a seam or a scroll into the next
    //      area's pixels, coming in through the doorway from where it crossed, or onto a fade's entrance,
    //      shown fresh. Outside its reach, AtEdge::Despawn removes it, AtEdge::Hide puts it out of the
    //      world, and AtEdge::Stop leaves it (held at a wall's opening, walking across a spot).
    //
    // An actor shown fresh takes a throwaway sprite key for the tick (ActorKit::Actors::fresh), so it
    // appears at once where it lands. Through a seam or a scroll it keeps its own key: layers() draws it in
    // the current area's pixels, where its position runs on unbroken, and it eases with everything else.
    //
    // A game holding the whole world still — under a menu or a message — leaves tick out for those ticks.
    // Throws whatever ActorKit::tick throws.
    template <class State, class Input>
    Step tick(const Input& input, State& state) {
        std::vector<ActorKit::Id> freshIds;

        // 1. Settings for new actors; the removed ones' dropped.
        for (const ActorKit::Id id : actors_.ids()) {
            if (findRow(id) == nullptr) {
                rows_.push_back(Row{.id = id});
            }
        }
        std::erase_if(rows_, [&](const Row& row) { return !actors_.has(row.id); });

        // 2. Back from hiding.
        for (Row& row : rows_) {
            if (row.hidden && detail::inWorld(*world_, actors_.place(row.id))) {
                row.hidden = false;
                freshIds.push_back(row.id);
            }
        }

        // 3. The camera in the followed actor's place.
        placeCameraOnFollowed(freshIds);

        // 4. Awake or frozen, by the areas on screen.
        const std::vector<Shown> shown = camera_.shown(*world_);
        for (const Row& row : rows_) {
            if (awakeUnder(row, shown)) {
                actors_.wake(row.id);
            } else {
                actors_.freeze(row.id);
            }
        }

        // While a scroll or a fade runs, the whole world holds still: steps 5, 6 and 8 wait, and only the
        // followed actor is carried.
        const bool held = camera_.transitioning();

        // 5. The boxes as the tick begins.
        for (Row& row : rows_) {
            row.was = held || actors_.frozen(row.id) ? std::nullopt
                                                      : std::optional<retropp::IntRect>{actors_.box(row.id)};
        }

        // 6. The actors.
        if (!held) {
            ActorKit::tick(actors_, config_, camera_, input, state);
        }

        // 7. The followed actor.
        Step step{};
        const ActorKit::Id followed = following();
        if (followed != ActorKit::kNoId && !hiddenNow(followed)) {
            if (!placeCameraOnFollowed(freshIds)) {
                const retropp::IntRect box = actors_.box(followed);
                const bool fading = detail::fading(camera_);
                step = camera_.follow(*world_, box);
                const retropp::Vec2i at = actors_.position(followed);
                actors_.moveTo(followed, step.entered.value_or(actors_.place(followed)),
                               {.x = at.x + step.move.x, .y = at.y + step.move.y});
                if (step.entered && fading) {
                    freshIds.push_back(followed);
                }
                lastBox_ = actors_.box(followed);
            }
        } else if (camera_.transitioning() && lastBox_) {
            // The scroll or fade the followed actor started runs to its end.
            step = camera_.follow(*world_, *lastBox_);
            lastBox_->x += step.move.x;
            lastBox_->y += step.move.y;
        } else {
            lastBox_.reset();
        }

        // 8. Every other actor through the openings.
        std::vector<ActorKit::Id> removed;
        for (Row& row : rows_) {
            if (row.id == followed || !row.was || !actors_.has(row.id) || actors_.frozen(row.id)) {
                continue;
            }
            const Place place = actors_.place(row.id);
            const Opening* opening = detail::goneThrough(*world_, place, *row.was, actors_.box(row.id));
            if (opening == nullptr) {
                continue;
            }
            const Step landing = detail::landing(*world_, place, *opening, actors_.box(row.id), /*carried=*/false);
            const bool inside = row.reach == Reach::World ||
                                (row.reach == Reach::Layout && landing.entered->layout == place.layout);
            if (inside) {
                const retropp::Vec2i at = actors_.position(row.id);
                actors_.moveTo(row.id, *landing.entered, {.x = at.x + landing.move.x, .y = at.y + landing.move.y});
                if (opening->transition == Transition::Fade) {
                    freshIds.push_back(row.id);
                }
            } else if (row.atEdge == AtEdge::Despawn) {
                actors_.remove(row.id);
                removed.push_back(row.id);
            } else if (row.atEdge == AtEdge::Hide) {
                actors_.moveTo(row.id, detail::kNowhere, {});
                actors_.freeze(row.id);
                row.hidden = true;
            }
        }
        std::erase_if(rows_, [&](const Row& row) {
            return std::find(removed.begin(), removed.end(), row.id) != removed.end();
        });

        for (const ActorKit::Id id : freshIds) {
            if (actors_.has(id)) {
                actors_.fresh(id);
            }
        }
        return step;
    }

    // The frame's layers: WorldLayers, then one sprite layer for each slot name among the areas the camera
    // shows, in the order the names first appear (the current area's slots first). Each is keyed
    // SceneLayerKey, drawn at the depth ActorZ gives that slot for the first area on screen that has it,
    // with the current area's scroll, confined to the current layout's view, and holds ActorKit::Sprites
    // of every area on screen with that slot, each area's moved into the current area's pixels: its
    // sprites' x, y and z shifted by where that area sits in them. So the actors of every area on screen
    // sort by their feet together, and each sprite keeps its key and eases with the layer when the current
    // area changes. Every slot name gets its layer, empty or with sprites. Throws what WorldLayers and
    // ActorKit::Sprites throw.
    template <class State>
    [[nodiscard]] LayerKit::Layers layers(const State& state) const {
        return layers(state, [](Place, std::string_view, LayerKit::Sprites sprites) { return sprites; });
    }

    // The same, with each slot's sprites handed to `dress` with the place and the slot's name, and what it
    // returns drawn in their place: to change a sprite on its way to the screen, or add sprites of the
    // game's own at a slot. `dress` is called once for each area on screen and each of its slots, its
    // sprites in that area's pixels; the scene moves what it returns into the current area's pixels.
    template <class State, class Dress>
        requires std::invocable<Dress&, Place, std::string_view, LayerKit::Sprites> &&
                 std::same_as<std::invoke_result_t<Dress&, Place, std::string_view, LayerKit::Sprites>, LayerKit::Sprites>
    [[nodiscard]] LayerKit::Layers layers(const State& state, Dress&& dress) const {
        LayerKit::Layers layers = WorldLayers(camera_, *world_);
        const CameraKit::View& view = world_->layouts[static_cast<std::size_t>(camera_.layout())].view;
        const std::vector<Shown> shown = camera_.shown(*world_);
        const retropp::LayerScroll current = shown.front().scroll;

        // One entry per slot name, in the order the names first appear.
        struct Gathered {
            std::string_view slot;
            std::int32_t z = 0;
            LayerKit::Sprites sprites;
        };
        std::vector<Gathered> gathered;
        for (const Shown& onScreen : shown) {
            const Place place{.layout = onScreen.layout, .area = onScreen.area};
            const Area& area =
                world_->layouts[static_cast<std::size_t>(onScreen.layout)].areas[static_cast<std::size_t>(onScreen.area)];
            // Where this area's top-left sits in the current area's pixels.
            const retropp::Vec2i by{.x = current.x - onScreen.scroll.x, .y = current.y - onScreen.scroll.y};
            for (const ActorSlot& slot : area.actorSlots) {
                auto entry = std::find_if(gathered.begin(), gathered.end(),
                                          [&](const Gathered& g) { return g.slot == slot.name; });
                if (entry == gathered.end()) {
                    gathered.push_back(Gathered{.slot = slot.name, .z = ActorZ(*world_, onScreen, slot.name)});
                    entry = std::prev(gathered.end());
                }
                for (retropp::Sprite& sprite : dress(place, std::string_view{slot.name},
                                                     ActorKit::Sprites(actors_, config_, place, slot.name, state))) {
                    sprite.x += by.x;
                    sprite.y += by.y;
                    sprite.z += by.y;
                    entry->sprites.push_back(std::move(sprite));
                }
            }
        }
        for (Gathered& entry : gathered) {
            layers.push_back(LayerKit::SpriteLayer(SceneLayerKey(entry.slot), entry.z, {std::move(entry.sprites)},
                                                   {.scroll = current, .regions = ViewStencil(view)}));
        }
        return layers;
    }

    // The fade over the screen while one runs: Fade for the scene's camera.
    [[nodiscard]] LayerKit::Regions fade() const { return Fade(camera_, *world_); }

    // The camera the game handed over. Every Camera call works on it: place, position, shown, follow for a
    // game that drives it by hand.
    [[nodiscard]] Camera& camera() noexcept { return camera_; }
    [[nodiscard]] const Camera& camera() const noexcept { return camera_; }

    // The scene's actors, one ActorKit::Actors set. Every ActorKit call works on it, with the ids the scene
    // hands out.
    [[nodiscard]] ActorKit::Actors& actors() noexcept { return actors_; }
    [[nodiscard]] const ActorKit::Actors& actors() const noexcept { return actors_; }

    // The world the scene runs.
    [[nodiscard]] const World& world() const noexcept { return *world_; }

    // The config the scene hands ActorKit: the game's, its `world` this scene's world.
    [[nodiscard]] const ActorKit::Config& config() const noexcept { return config_; }

private:
    friend class SceneActor;

    // The scene's own settings of one live actor.
    struct Row {
        ActorKit::Id id = ActorKit::kNoId;
        std::string name;
        Reach reach = Reach::Area;
        AtEdge atEdge = AtEdge::Stop;
        AwakeWhen awakeWhen = AwakeWhen::Always;
        bool frozenByHand = false;               // SceneActor::freeze stands
        bool hidden = false;                     // AtEdge::Hide put it out of the world
        std::optional<retropp::IntRect> was{};   // its box as this tick began, while it is awake
    };

    [[nodiscard]] Row* findRow(ActorKit::Id id) noexcept {
        for (Row& row : rows_) {
            if (row.id == id) {
                return &row;
            }
        }
        return nullptr;
    }

    // The settings of the live actor `id`, made with the defaults for one spawned by a script or through
    // actors() since the last tick.
    [[nodiscard]] Row& rowOf(ActorKit::Id id) {
        if (!actors_.has(id)) {
            throw std::out_of_range("WorldKit::Scene: no live actor has id " + std::to_string(id));
        }
        if (Row* row = findRow(id)) {
            return *row;
        }
        rows_.push_back(Row{.id = id});
        return rows_.back();
    }

    [[nodiscard]] ActorKit::Id idNamed(std::string_view name) const {
        if (!name.empty()) {
            for (const Row& row : rows_) {
                if (row.name == name && actors_.has(row.id)) {
                    return row.id;
                }
            }
        }
        throw std::out_of_range("WorldKit::Scene: no actor is named '" + std::string{name} + "'");
    }

    [[nodiscard]] bool hiddenNow(ActorKit::Id id) noexcept {
        const Row* row = findRow(id);
        return row != nullptr && row->hidden;
    }

    // Whether the actor of `row` is awake, with `shown` the areas on screen.
    [[nodiscard]] bool awakeUnder(const Row& row, const std::vector<Shown>& shown) const {
        if (row.hidden || row.frozenByHand) {
            return false;
        }
        const Place place = actors_.place(row.id);
        switch (row.awakeWhen) {
            case AwakeWhen::InLayout:
                for (const Shown& area : shown) {
                    if (area.layout == place.layout) {
                        return true;
                    }
                }
                return false;
            case AwakeWhen::OnScreen:
                for (const Shown& area : shown) {
                    if (area.layout == place.layout && area.area == place.area) {
                        return true;
                    }
                }
                return false;
            case AwakeWhen::Always:
            default: return true;
        }
    }

    // When the followed actor is in the world, in the scene, and in another place than the camera, puts the
    // camera there on its box and reports the actor fresh; returns whether it did.
    bool placeCameraOnFollowed(std::vector<ActorKit::Id>& freshIds) {
        const ActorKit::Id followed = following();
        if (followed == ActorKit::kNoId || hiddenNow(followed)) {
            return false;
        }
        const Place place = actors_.place(followed);
        if (!detail::inWorld(*world_, place) ||
            (place.layout == camera_.layout() && place.area == camera_.area())) {
            return false;
        }
        camera_ = Camera{place.layout, place.area};
        lastBox_ = actors_.box(followed);
        camera_.place(*world_, *lastBox_);
        freshIds.push_back(followed);
        return true;
    }

    ActorKit::Id spawnRow(const std::string& name, const AnyActor& held, Place place, retropp::Vec2i at, Reach reach,
                          AtEdge atEdge, AwakeWhen awakeWhen) {
        const ActorKit::Actor* actor = held.as<ActorKit::Actor>();
        if (actor == nullptr) {
            throw std::logic_error("WorldKit::Scene: actor '" + name + "' is not an ActorKit::Actor");
        }
        if (!name.empty()) {
            for (const Row& row : rows_) {
                if (row.name == name && actors_.has(row.id)) {
                    throw std::invalid_argument("WorldKit::Scene: an actor is already named '" + name + "'");
                }
            }
        }
        const ActorKit::Id id = actors_.spawn(*actor, place, at);
        Row& row = rowOf(id);
        row.name = name;
        row.reach = reach;
        row.atEdge = atEdge;
        row.awakeWhen = awakeWhen;
        actors_.confinement(id, detail::confinementOf(reach, atEdge));
        return id;
    }

    const World* world_;
    Camera camera_;
    ActorKit::Config config_;
    ActorKit::Actors actors_;
    std::vector<Row> rows_;          // in spawn order
    // The followed actor's box after the last tick it was followed, kept while a scroll or fade it started
    // runs on after the follow ends.
    std::optional<retropp::IntRect> lastBox_;
};

inline void SceneActor::reach(Reach reach) {
    Scene::Row& row = scene_->rowOf(id_);
    row.reach = reach;
    scene_->actors_.confinement(id_, detail::confinementOf(row.reach, row.atEdge));
}

inline Reach SceneActor::reach() const { return scene_->rowOf(id_).reach; }

inline void SceneActor::atEdge(AtEdge atEdge) {
    Scene::Row& row = scene_->rowOf(id_);
    row.atEdge = atEdge;
    scene_->actors_.confinement(id_, detail::confinementOf(row.reach, row.atEdge));
}

inline AtEdge SceneActor::atEdge() const { return scene_->rowOf(id_).atEdge; }

inline void SceneActor::awakeWhen(AwakeWhen awakeWhen) { scene_->rowOf(id_).awakeWhen = awakeWhen; }

inline AwakeWhen SceneActor::awakeWhen() const { return scene_->rowOf(id_).awakeWhen; }

inline void SceneActor::freeze() {
    scene_->rowOf(id_).frozenByHand = true;
    scene_->actors_.freeze(id_);
}

inline void SceneActor::wake() {
    Scene::Row& row = scene_->rowOf(id_);
    row.frozenByHand = false;
    if (scene_->awakeUnder(row, scene_->camera_.shown(*scene_->world_))) {
        scene_->actors_.wake(id_);
    }
}

inline bool SceneActor::frozen() const {
    (void)scene_->rowOf(id_);
    return scene_->actors_.frozen(id_);
}

inline bool SceneActor::awake() const { return !frozen(); }

inline bool SceneActor::hidden() const { return scene_->rowOf(id_).hidden; }

}  // namespace WorldKit
