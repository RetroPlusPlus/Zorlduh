# How to

Recipes built on the module. Each assumes a `border`, a `font` and a frame being built, as in
[getting-started.md](getting-started.md).

## Contents

- [A HUD panel](#a-hud-panel)
- [A window that hugs its text](#a-window-that-hugs-its-text)
- [A box with no fill](#a-box-with-no-fill)
- [Two windows stacked](#two-windows-stacked)
- [A message the player cannot skip](#a-message-the-player-cannot-skip)
- [Another confirm button](#another-confirm-button)
- [A border of larger pieces](#a-border-of-larger-pieces)
- [Several borders](#several-borders)
- [A portrait beside a message](#a-portrait-beside-a-message)
- [Closing a message from the game](#closing-a-message-from-the-game)

## A HUD panel

A window is a layer; give it the HUD's depth and put the HUD's sprites in it.

```cpp
constexpr retropp::IntRect kPanel{.x = 0, .y = 0, .width = 160, .height = 16};
const retropp::IntRect room = WindowKit::inside(kPanel, border);
frame.layers.push_back(WindowKit::Window("hud", kHudZ, kPanel, border,
                                         {Hearts(state, {room.x, room.y}), Coins(state, {room.x + 64, room.y})}));
```

A border thicker than half the panel's height leaves no room; pick a border whose pieces suit the panel.

## A window that hugs its text

`TextKit::measure` gives the pixels a passage takes; add the thickness on each side.

```cpp
const retropp::PixelSize text = TextKit::measure(label, font);
const retropp::PixelSize t = WindowKit::thickness(border);
const retropp::IntRect box{.x = 24, .y = 40, .width = text.width + 2 * t.width, .height = text.height + 2 * t.height};
const retropp::IntRect room = WindowKit::inside(box, border);
frame.layers.push_back(WindowKit::Window("label", kHudZ, box, border,
                                         {TextKit::Text("label:text", label, {room.x, room.y}, font)}));
```

## A box with no fill

Draw the sheet's fill piece entirely in its transparent index. The fill is still laid out and cut; it
draws nothing, and what is under the window shows through the frame.

## Two windows stacked

Two windows are two layers: give each its own key and its own depth.

```cpp
frame.layers.push_back(WindowKit::Window("inventory", kMenuZ, inventoryBox, border, {Items(state)}));
frame.layers.push_back(WindowKit::Window("item-detail", kMenuZ + 1, detailBox, border, {Detail(state)}));
```

## A message the player cannot skip

Confirm only once the page is out, so the press shows nothing early and only turns the page.

```cpp
state.dialog.advance(input.confirm && state.dialog.pageDone());
```

## Another confirm button

`advance` takes whatever the game calls a confirm: a different action, or any of several.

```cpp
state.dialog.advance(input.justPressed(Action::Attack) || input.justPressed(Action::Item));
```

## A border of larger pieces

Carve the sheet at the size its pieces are drawn. Everything else — the thickness, the room, the cuts, the
arrow's place — follows from the slots.

```cpp
const WindowKit::Border ornate{
    .sheet = renderer.loadAtlas("assets/gfx/hud/ornate-border.png", retropp::AssetDimensions{.width = 16, .height = 16},
                                retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
                                /*count=*/10, retropp::TransparentIndices::of({0})),
    .palette = ornateColors,
};
```

A message in this border has 16 pixels less room on each side, so open it with
`inside(box, ornate)`'s size.

## Several borders

A `Border` is a value, and every window takes its own. Keep as many as the game needs:

```cpp
struct Borders {
    WindowKit::Border wood;    // signs
    WindowKit::Border stone;   // carved rocks
    WindowKit::Border plain;   // menus
};
```

Two borders can share one sheet and differ only in palette: the same pieces in other colors. When the
border a message is drawn in depends on what is being read, keep which one in the game's state and pick
it in the frame; open the message with that border's room.

## A portrait beside a message

A portrait is a sprite the game puts in the window. Draw the message box as a `Window` of your own, with
the portrait at the room's left and the text after it, and open the message with the room the text has.

```cpp
const retropp::IntRect room = WindowKit::inside(kMessageBox, border);
const retropp::IntRect textRoom{.x = room.x + 40, .y = room.y, .width = room.width - 40, .height = room.height};
// When the message starts: dialog.open(text, {textRoom.width, textRoom.height}, font);
frame.layers.push_back(WindowKit::Window("message", kMessageZ, kMessageBox, border,
                                         {Portrait(state, {room.x, room.y}),
                                          WindowKit::DialogText("message", state.dialog, {textRoom.x, textRoom.y}, font),
                                          WindowKit::DialogArrow("message", state.dialog, textRoom, border, 1)}));
```

## Closing a message from the game

`close` ends a message at once, wherever it is: when the player is hit, when a scene ends.

```cpp
if (playerWasHit) {
    state.dialog.close();
}
```
