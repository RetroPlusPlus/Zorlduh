# Concepts

The model behind the module: what a border is, how a box is laid out of its pieces at any size, what a
window is, how a window opens and closes, and how a message is split between state the tick advances and
components that draw it.

## Contents

- [A border is a sheet of pieces](#a-border-is-a-sheet-of-pieces)
- [A box is a rectangle and the pieces laid into it](#a-box-is-a-rectangle-and-the-pieces-laid-into-it)
- [The cut](#the-cut)
- [Every size is the art's or the caller's](#every-size-is-the-arts-or-the-callers)
- [Keys by place](#keys-by-place)
- [A window is one layer](#a-window-is-one-layer)
- [An animation is state; a style is a function](#an-animation-is-state-a-style-is-a-function)
- [A message is state and components](#a-message-is-state-and-components)
- [Why the page is in a glyph's key](#why-the-page-is-in-a-glyphs-key)
- [Where each part lives in a game](#where-each-part-lives-in-a-game)

## A border is a sheet of pieces

A border is a sheet carved into the nine pieces of a box, laid out as the box looks: the top-left
corner, the top edge and the top-right corner; the left edge, the fill and the right edge; the bottom
corners and the bottom edge between them. `WindowKit::Piece` names them in that order. A tenth slot, when
the sheet has one, is the arrow a message box shows while another page follows (`kArrowSlot`).

A `Border` is the sheet as the renderer carved it (`retropp::AtlasManifest`) and the palette its pieces
are drawn through. The sheet's transparent index is what lets whatever is under a window show around a
rounded corner. A game holds as many borders as it likes: one per kind of window, or one sheet through
several palettes.

## A box is a rectangle and the pieces laid into it

A box is a rectangle in the pixels of the layer it is drawn on, `retropp::IntRect`, and `Box` lays the
pieces into it:

- the four corners at the box's four corners;
- the top and bottom edges repeated left to right between the corners, the left and right edges
  repeated top to bottom between them;
- the fill repeated across what is left inside, left to right and then down.

The border's **thickness** is the size of its top-left piece. `inside(box, border)` is the box with the
thickness taken off each side: the room the border leaves, where a window's contents go.

## The cut

A box's sides are rarely a whole number of pieces long. The last repeat along an edge, and the last
column and row of the fill, run past where their room ends. Each of those pieces carries a region on its
own sprite — a rectangle over the part past the end, in the sprite's own pixels, with a `Transparency`
effect — so that part is not drawn. A piece cut on both axes carries two. A piece that fits carries
none.

The box is therefore exactly the size asked for, the pieces are never stretched, and a patterned edge
keeps its pattern from the corner it starts at.

## Every size is the art's or the caller's

Every size the module uses is read from what it is handed. A piece is the size its slot is, the
thickness is the top-left piece's, the text's step is the font's `advance`, and the box is the rectangle
the game hands over. A border drawn in large pieces lays out and cuts the same way as one drawn in small
ones.

## Keys by place

The engine matches a sprite to the one it drew last frame by its key, and eases its position between
ticks. A box keys each piece by its place in the box and the box's size:
`"<key>:fill:<column>:<row>:<width>x<height>"`, `"<key>:top:<n>:<width>x<height>"` and so on. A box that
moves at one size keeps every key, so all of it eases together.

A box drawn at a new size shares no key with the box before it. The engine eases a sprite's position but
never its regions, so a piece that kept its key while its cut snapped to the new size would ease from
where it was while its neighbors stood in place, and leave a gap for a frame. With every key fresh, a box
at a new size is drawn whole on the tick it changes, every piece in place with its cut — however it
changed: wider, taller, or grown from its middle so that every piece moves at once.

## A window is one layer

`Window` returns one sprite layer, built by LayerKit's `SpriteLayer`: the box, then the sprites the game
puts inside it, in the order given. The box's sprites sit at the lowest z there is, so the box is beneath
everything put in the window whatever its own z. Everything in the layer options passes onto the layer.

What goes inside is the game's: text from TextKit, a cursor, an icon, any sprite. The game places them in
the layer's pixels; `inside(box, border)` says where the room is.

## An animation is state; a style is a function

A window that opens and closes over time is split the same way as everything else here.

- **`Animation`** is state: how long an opening takes, the curve it follows, and how far along it is. It
  reports closed, opening, open or closing and a progress from 0 to 1. The tick advances it; it names no
  draw type.
- **A style** is a function from that progress to what a box or a layer takes: **grow** gives the box a
  smaller rectangle, **clip** gives the layer regions that leave only part of the box drawn, **squash**
  gives the layer a transform, **fade** an alpha, **slide** a scroll. `Style` names one with its
  parameters, and `Animate` applies it over the options a caller already has.

At progress 1 every style is its identity, so an open window is drawn exactly as a window with no
animation is. A window's contents are drawn only once it is open, and are gone the moment it starts to
close: while it moves, it is the box alone. On the opening's first tick it is nothing at all, its layer
already at the style's start, so the engine's easing of the layer from what it was before has nothing to
show ([animation.md](animation.md#window-with-an-animation)).

Whatever owns a window advances its animation. A `Dialog` holds one and advances it from its own
`advance`; a window a game draws itself, a menu or a panel, has an `Animation` in the game's state that
the game's tick advances. An animation with no length is instant, and a window with one opens and closes
the way a window without one does.

## A message is state and components

A message box is split the way the rest of a game is: state the tick advances, and components that read
it every frame.

- **`Dialog`** is state. `open` wraps a passage to the room it is given and cuts it into pages and opens
  its box; `advance` runs once per tick, moving the box's animation, typing the page on once the box is
  open, and applying a confirm, which finishes whatever is in progress: the opening, the page, the
  message, the closing. It names no draw type, so it sits in a game's simulation state beside
  everything else the tick advances.
- **`DialogText`**, **`DialogArrow`** and **`DialogBox`** are components. They read a `Dialog` and return
  sprites, or a layer; they change nothing.

The room `open` is given is the room the box will draw the text in, `inside(box, border)`, so the pages
the dialog cut are the pages the box has room for.

## Why the page is in a glyph's key

Each glyph of a page is keyed `"<key>:<page>:<n>"`, by its page and its position on the page. When a page
turns, the next page's glyphs are new sprites, so none of them eases from where a glyph of the last page
was. Within a page a glyph keeps its key as more of the page is typed.

## Where each part lives in a game

| Part | Where |
|---|---|
| The border sheets, their palettes and the font | loaded once, with the game's other assets |
| A `Dialog` | the game's state, advanced by the tick |
| An `Animation` for a window the game draws itself | the game's state, advanced by the tick |
| How long a window takes to open and close | the game's config, set on the animation |
| Which button confirms, and when a message opens | the game's tick |
| Where a window goes, how big it is, its depth | the game's layout and depth table |
| The style a window opens and closes in | the frame, handed to `Window` or `DialogBox` |
| `Window`, `DialogBox` | the frame, called once per window per frame |
