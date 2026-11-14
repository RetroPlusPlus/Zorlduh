// The simulation: the game's whole state, what it is configured by, and the tick that advances it. The
// tick changes the state; everything on screen only reads it.
//
//     const zorlduh::Config config = zorlduh::gameConfig(assets, kView, kAreaDepths, kDialogBox, kMenuBoxes);
//     zorlduh::GameState state{.player = TopDownMovement::Walker{zorlduh::kPlayerStart},
//                              .screen = zorlduh::Screen::Title};
//     state.camera.place(config.world, state.player.collision(config.player).bounds());
//     zorlduh::tick(state, zorlduh::readInput(in), config);   // once per tick
#pragma once

#include <chrono>
#include <cstdint>
#include <variant>
#include <vector>

#include "retropp/geometry.h"

#include "CameraKit/camera.h"
#include "MenuKit/input.h"
#include "MenuKit/menu.h"
#include "MenuKit/menu_config.h"
#include "TextKit/font.h"
#include "TextKit/reveal.h"
#include "TopDownMovement/directions.h"
#include "TopDownMovement/walker.h"
#include "WindowKit/animation.h"
#include "WindowKit/border.h"
#include "WindowKit/dialog.h"
#include "WorldKit/area.h"
#include "WorldKit/camera.h"

#include "assets/assets.h"
#include "game/areas.h"
#include "game/menus.h"
#include "game/signs.h"

namespace zorlduh {

// What is on screen: the title screen, or the world in play — held still under a menu or a message.
enum class Screen : std::uint8_t { Title, Playing };

// What the player asked for this tick.
struct Input {
    TopDownMovement::DirectionalInput move{};  // the directions held and pressed
    bool item = false;                         // Item was pressed this tick
    bool start = false;                        // Start was pressed this tick
    MenuKit::MenuInput menu{};                 // the directions, and Item and Attack as confirm and cancel
};

// What the tick says happened. Presentation and audio read these; the tick clears them first.
struct AreaChanged {
    Layout layout;  // the layout the player is now in
    int area;       // the area in it: an Area in the overworld, a Room in DungeonOne, a Woods in
                    // SeamlessForest
};
using GameEvent = std::variant<AreaChanged>;

// How long a message's box takes to open, and to close.
inline constexpr std::chrono::milliseconds kMessageAnimationTime{300};

// How long a menu's window takes to open, and to close; and how long a direction is held in a menu
// before the cursor moves again, then how long between each move after.
inline constexpr std::chrono::milliseconds kMenuAnimationTime{200};
inline constexpr std::chrono::milliseconds kMenuRepeatDelay{400};
inline constexpr std::chrono::milliseconds kMenuRepeatEvery{100};

// What the game is: the player's walker config, the world, what a message is read in, and the menus.
// Built once from the loaded assets.
struct Config {
    TopDownMovement::WalkerConfig player;
    WorldKit::World world;
    TextKit::Font font;                        // a message's text
    Borders borders;                           // the border of each material, and the menus'
    retropp::IntRect messageBox;               // where a message's box is, on screen
    WindowKit::Animation messageAnimation;     // how a message's box opens and closes; a message starts with a copy
    MenuKit::MenuConfig title;                 // the title screen's menu (menus.h)
    MenuKit::MenuConfig pause;                 // the menu Start opens in play (menus.h)
};

// The game's config over `assets`, with the world shown in `view`, its areas' layers and actor slots
// at `depths`, a message read in `messageBox`, and the menus' windows in `menuBoxes`, each opening and
// closing over kMenuAnimationTime.
[[nodiscard]] Config gameConfig(const Assets& assets, CameraKit::View view, const AreaDepths& depths,
                                retropp::IntRect messageBox, const MenuBoxes& menuBoxes);

// The border a message from a thing of `material` is drawn in.
[[nodiscard]] const WindowKit::Border& borderOf(const Borders& borders, Material material) noexcept;

struct GameState {
    TopDownMovement::Walker player;
    WorldKit::Camera camera{kStartLayout, kStartArea};  // which area, where the screen looks, any scroll or fade running
    TextKit::Reveal nameReveal;  // how much of the current area's name has been typed into the HUD strip
    WindowKit::Dialog dialog;    // the message being read, if any
    Material reading = Material::Wood;  // what the thing being read is made of: the border its message is in
    MessageStyle readingStyle = MessageStyle::Grow;  // how the message being read opens and closes
    std::vector<GameEvent> events;
    Screen screen = Screen::Playing;  // the game's entry point starts at the title; a state built for a
                                      // test is in play
    MenuKit::Menu title;              // the title screen's menu, over Config::title
    MenuKit::Menu pause;              // the pause menu, over Config::pause
    std::uint32_t messageTicksPerGlyph = 2;          // how fast a message types: TEXT's SLOW 4, NORMAL 2, FAST 1
    MessageStyle menuStyle = MessageStyle::Grow;     // how the menus' windows open and close: WINDOWS'
};

// Back to the title screen: the player at kPlayerStart in the start area, the camera placed on it, no
// message, the area's name to type from its start, both menus closed. How fast messages type and how the
// menus open are kept.
void returnToTitle(GameState& state, const Config& config);

// One tick.
//
// On the title screen the title menu opens if it is not showing and takes the tick's menu input: START
// closes it, and play begins on the tick it has closed; an option is applied and the menu steps back to
// OPTIONS. Nothing else moves.
//
// While the pause menu shows, the world holds still under it: Start closes it, as RESUME does; TITLE
// returns to the title screen; an area chosen from WARP puts the player there and closes the menu; an
// option is applied and the menu steps back to OPTIONS.
//
// While a message is showing, Item finishes whatever it is doing (opens the box at once, shows the page,
// turns it, closes it after the last, closes the box at once) and nothing else moves.
//
// Otherwise the player walks the current area unless a scroll or fade is running, the camera follows the
// player's collision box and moves the player through any opening it goes out by, and entering an area
// is recorded as an AreaChanged event and starts its name typing out again; then Item, with no scroll or
// fade running, opens the message of the sign or rock the player faces, in the border of its material,
// the style of its own and the speed TEXT set; then Start, with no scroll or fade running and no message
// showing, opens the pause menu. The name's reveal advances every tick but on the title screen.
void tick(GameState& state, const Input& input, const Config& config);

}  // namespace zorlduh
