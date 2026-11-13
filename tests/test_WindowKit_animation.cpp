// The WindowKit module's animation: an Animation's length, phases, progress, reversal and skip; each
// style's function at progress 0, midway and 1; Style; Animate over a caller's options; a Window with
// an animation; the keys of a box as it grows; a Dialog opening, typing once open, closing, and confirm
// finishing whatever is in progress; the page, the arrow and the message box while the box moves. The
// borders and fonts are built over made-up manifests and handles. A box grown from its middle is also
// drawn through a renderer on a GPU device and read back at every tick.

#include <gtest/gtest.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "retropp/atlas_manifest.h"
#include "retropp/draw_state.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/palette.h"
#include "retropp/renderer.h"
#include "retropp/timing.h"
#include "retropp/transform.h"
#include "retropp/tween.h"
#include "retropp/viewport.h"

#include "LayerKit/layer_options.h"
#include "TextKit/font.h"
#include "WindowKit/animate.h"
#include "WindowKit/animation.h"
#include "WindowKit/border.h"
#include "WindowKit/box.h"
#include "WindowKit/dialog.h"
#include "WindowKit/dialog_box.h"
#include "WindowKit/window.h"

namespace {

using retropp::IntRect;
using retropp::Sprite;
using WindowKit::Phase;

// A ten-slot border sheet of 8×8 cells, three to a row.
WindowKit::Border border() {
    return WindowKit::Border{
        .sheet = retropp::AtlasManifest{
            .atlasId = retropp::AtlasId{9},
            .slots = retropp::sliceLayout({.width = 24, .height = 32}, {.width = 8, .height = 8},
                                          retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown, 10),
            .kind = retropp::ContentKind::Tileset,
        },
        .palette = retropp::PaletteId{4},
    };
}

// An 8-pixel font over a 128×24 sheet of 8×8 cells, uppercase, digits and punctuation.
TextKit::Font font() {
    return TextKit::Font{
        .sheet = retropp::AtlasManifest{
            .atlasId = retropp::AtlasId{5},
            .slots = retropp::sliceLayout({.width = 128, .height = 24}, {.width = 8, .height = 8},
                                          retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown, 43),
            .kind = retropp::ContentKind::Tileset,
        },
        .glyphs = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?'-:",
        .palette = retropp::PaletteId{6},
    };
}

// An animation of `ticks` ticks along `easing`.
WindowKit::Animation lasting(std::uint32_t ticks, retropp::Easing easing = retropp::Easing::Linear) {
    WindowKit::Animation animation;
    animation.ticks = ticks;
    animation.easing = easing;
    return animation;
}

// An animation of four linear ticks, opened and advanced `ticks` ticks.
WindowKit::Animation openedFor(std::uint32_t ticks) {
    WindowKit::Animation animation = lasting(4);
    animation.open();
    for (std::uint32_t i = 0; i < ticks; ++i) {
        animation.advance();
    }
    return animation;
}

std::vector<std::string> keys(std::span<const Sprite> sprites) {
    std::vector<std::string> out;
    for (const Sprite& sprite : sprites) {
        out.emplace_back(std::string_view(sprite.key));
    }
    return out;
}

std::span<const Sprite> spritesOf(const retropp::DrawLayer& layer) {
    return std::get<retropp::SpriteContent>(layer.content).sprites;
}

// The message box of these tests: 160×48 at (0, 96).
constexpr IntRect kBox{.x = 0, .y = 96, .width = 160, .height = 48};
// The 18-by-2 room a message in the 8-pixel font is paged to in these tests.
constexpr retropp::PixelSize kRoom{.width = 144, .height = 16};
constexpr std::string_view kPassage = "The quick brown fox jumps over the lazy dog and runs far away.";

// ── The animation ────────────────────────────────────────────────────────────────────────────

TEST(Animation, ItsLengthIsTicksOrElseItsDurationThroughItsTiming) {
    WindowKit::Animation animation;
    EXPECT_EQ(animation.length(), 0u);
    animation.timing = retropp::TimingProfile::GameBoyColor;
    animation.duration = std::chrono::milliseconds{150};
    EXPECT_EQ(animation.length(), 9u);
    animation.duration = std::chrono::milliseconds{8};  // under half a tick
    EXPECT_EQ(animation.length(), 0u);
    animation.duration = std::chrono::milliseconds{150};
    animation.ticks = 5;
    EXPECT_EQ(animation.length(), 5u);
}

TEST(Animation, WithNoLengthItOpensAndClosesAtOnce) {
    WindowKit::Animation animation;
    EXPECT_EQ(animation.phase(), Phase::Closed);
    EXPECT_FLOAT_EQ(animation.progress(), 0.0f);
    animation.open();
    EXPECT_EQ(animation.phase(), Phase::Open);
    EXPECT_FLOAT_EQ(animation.progress(), 1.0f);
    EXPECT_FALSE(animation.moving());
    animation.close();
    EXPECT_EQ(animation.phase(), Phase::Closed);
    EXPECT_FALSE(animation.moving());
}

TEST(Animation, AnOpeningMovesOnEachTickAndIsOpenOnTheLast) {
    WindowKit::Animation animation = lasting(4);
    animation.open();
    EXPECT_EQ(animation.phase(), Phase::Opening);
    EXPECT_TRUE(animation.moving());
    EXPECT_FLOAT_EQ(animation.progress(), 0.0f);
    for (const float expected : {0.25f, 0.5f, 0.75f}) {
        animation.advance();
        EXPECT_EQ(animation.phase(), Phase::Opening);
        EXPECT_FLOAT_EQ(animation.progress(), expected);
    }
    animation.advance();
    EXPECT_EQ(animation.phase(), Phase::Open);
    EXPECT_FLOAT_EQ(animation.progress(), 1.0f);
    animation.advance();
    EXPECT_EQ(animation.phase(), Phase::Open);
}

TEST(Animation, AClosingIsTheOpeningBackward) {
    WindowKit::Animation animation = openedFor(4);
    animation.close();
    EXPECT_EQ(animation.phase(), Phase::Closing);
    EXPECT_FLOAT_EQ(animation.progress(), 1.0f);
    for (const float expected : {0.75f, 0.5f, 0.25f}) {
        animation.advance();
        EXPECT_EQ(animation.phase(), Phase::Closing);
        EXPECT_FLOAT_EQ(animation.progress(), expected);
    }
    animation.advance();
    EXPECT_EQ(animation.phase(), Phase::Closed);
    EXPECT_FLOAT_EQ(animation.progress(), 0.0f);
}

TEST(Animation, TurningBackCarriesOnFromTheSameProgress) {
    WindowKit::Animation closing = openedFor(1);
    closing.close();
    EXPECT_EQ(closing.phase(), Phase::Closing);
    EXPECT_FLOAT_EQ(closing.progress(), 0.25f);
    closing.advance();
    EXPECT_EQ(closing.phase(), Phase::Closed);

    WindowKit::Animation opening = openedFor(4);
    opening.close();
    opening.advance();
    opening.open();
    EXPECT_EQ(opening.phase(), Phase::Opening);
    EXPECT_FLOAT_EQ(opening.progress(), 0.75f);
    opening.advance();
    EXPECT_EQ(opening.phase(), Phase::Open);
}

TEST(Animation, SkipFinishesWhatIsInProgressAndOpenOrCloseAgainDoesNothing) {
    WindowKit::Animation animation = openedFor(1);
    animation.skip();
    EXPECT_EQ(animation.phase(), Phase::Open);
    animation.skip();
    EXPECT_EQ(animation.phase(), Phase::Open);
    animation.open();
    EXPECT_EQ(animation.phase(), Phase::Open);
    animation.close();
    animation.advance();
    animation.skip();
    EXPECT_EQ(animation.phase(), Phase::Closed);
    animation.skip();
    animation.close();
    EXPECT_EQ(animation.phase(), Phase::Closed);
}

TEST(Animation, ProgressFollowsTheEasing) {
    WindowKit::Animation linear = lasting(4);
    WindowKit::Animation eased = lasting(4, retropp::Easing::InOutQuad);
    linear.open();
    eased.open();
    EXPECT_FLOAT_EQ(eased.progress(), 0.0f);
    linear.advance();
    eased.advance();
    EXPECT_FLOAT_EQ(linear.progress(), 0.25f);
    EXPECT_FLOAT_EQ(eased.progress(), retropp::ease(retropp::Easing::InOutQuad, 0.25f));
    EXPECT_NE(eased.progress(), linear.progress());
    for (int i = 0; i < 3; ++i) {
        eased.advance();
    }
    EXPECT_FLOAT_EQ(eased.progress(), 1.0f);
}

// ── The styles ───────────────────────────────────────────────────────────────────────────────

TEST(Grow, FromTheMiddleACornerOrAnyPoint) {
    EXPECT_EQ(WindowKit::Grow(kBox, openedFor(0)), (IntRect{.x = 80, .y = 120, .width = 0, .height = 0}));
    EXPECT_EQ(WindowKit::Grow(kBox, openedFor(2)), (IntRect{.x = 40, .y = 108, .width = 80, .height = 24}));
    EXPECT_EQ(WindowKit::Grow(kBox, openedFor(4)), kBox);
    EXPECT_EQ(WindowKit::Grow(kBox, openedFor(2), {.x = 0, .y = 96}),
              (IntRect{.x = 0, .y = 96, .width = 80, .height = 24}));
    EXPECT_EQ(WindowKit::Grow(kBox, openedFor(2), {.x = 200, .y = 0}),
              (IntRect{.x = 100, .y = 48, .width = 80, .height = 24}));
}

TEST(Clip, IsNothingOpenTheWholeLayerAtNoProgressAndTheGrownRectangleBetween) {
    EXPECT_TRUE(WindowKit::Clip(kBox, openedFor(4)).empty());

    const std::vector<retropp::Region> none = WindowKit::Clip(kBox, openedFor(0));
    ASSERT_EQ(none.size(), 1u);
    EXPECT_EQ(std::string_view(none[0].key), "clip");
    EXPECT_FALSE(none[0].shape.hasRegion());
    ASSERT_EQ(none[0].effects.size(), 1u);
    EXPECT_EQ(none[0].effects[0].kind, retropp::ScreenSpaceEffectKind::Transparency);

    const std::vector<retropp::Region> half = WindowKit::Clip(kBox, openedFor(2));
    ASSERT_EQ(half.size(), 1u);
    EXPECT_EQ(std::string_view(half[0].key), "clip");
    EXPECT_EQ(half[0].shape.points, retropp::ShapePoints::rectangle(retropp::Point{40, 108}, 80, 24).points);
    ASSERT_EQ(half[0].effects.size(), 1u);
    EXPECT_EQ(half[0].effects[0].kind, retropp::ScreenSpaceEffectKind::Transparency);
    EXPECT_EQ(half[0].effects[0].stencil, retropp::StencilMode::TransparentOutside);
    EXPECT_EQ(half[0].effects[0].scope, retropp::ScreenSpaceEffectScope::Layer);
}

TEST(Squash, ScalesAboutTheMiddleAlongItsAxisNeverBelowOnePixel) {
    EXPECT_TRUE(WindowKit::Squash(kBox, openedFor(4)).isIdentity());

    const retropp::Transform half = WindowKit::Squash(kBox, openedFor(2));
    EXPECT_FLOAT_EQ(half.applyY(37, 96), 108.0f);  // the top edge halfway to the middle at 120
    EXPECT_FLOAT_EQ(half.applyX(37, 96), 37.0f);

    const retropp::Transform none = WindowKit::Squash(kBox, openedFor(0));
    EXPECT_FLOAT_EQ(none.applyY(0, 144) - none.applyY(0, 96), 1.0f);  // one pixel of the box

    const retropp::Transform across = WindowKit::Squash(kBox, openedFor(2), retropp::Axis::Horizontal);
    EXPECT_FLOAT_EQ(across.applyX(0, 96), 40.0f);
    EXPECT_FLOAT_EQ(across.applyY(0, 96), 96.0f);
}

TEST(FadeAndSlide, AreTheProgressAndTheScrollThatBringsTheBoxFromAPoint) {
    EXPECT_FLOAT_EQ(WindowKit::Fade(openedFor(0)), 0.0f);
    EXPECT_FLOAT_EQ(WindowKit::Fade(openedFor(1)), 0.25f);
    EXPECT_FLOAT_EQ(WindowKit::Fade(openedFor(4)), 1.0f);

    // From just below a 144-pixel screen: the box's top at 144 at progress 0, in place at 1.
    EXPECT_EQ(WindowKit::Slide(kBox, openedFor(0), {.x = 0, .y = 144}), (retropp::LayerScroll{.x = 0, .y = -48}));
    EXPECT_EQ(WindowKit::Slide(kBox, openedFor(1), {.x = 0, .y = 144}), (retropp::LayerScroll{.x = 0, .y = -36}));
    EXPECT_EQ(WindowKit::Slide(kBox, openedFor(4), {.x = 0, .y = 144}), (retropp::LayerScroll{}));
    // Rounded to the nearest pixel: 45 · ½ is 22.5.
    EXPECT_EQ(WindowKit::Slide(kBox, openedFor(2), {.x = 0, .y = 141}), (retropp::LayerScroll{.x = 0, .y = -23}));
}

TEST(Style, EachNamedConstructorFillsWhatItsKindReads) {
    EXPECT_EQ(WindowKit::Style{}.kind, WindowKit::Style::Kind::Grow);
    EXPECT_FALSE(WindowKit::Style{}.origin.has_value());
    EXPECT_EQ(WindowKit::Style::pop().kind, WindowKit::Style::Kind::Pop);
    EXPECT_EQ(WindowKit::Style::grow().kind, WindowKit::Style::Kind::Grow);
    EXPECT_FALSE(WindowKit::Style::grow().origin.has_value());
    EXPECT_EQ(WindowKit::Style::grow({.x = 3, .y = 4}).origin, (retropp::Vec2i{.x = 3, .y = 4}));
    EXPECT_EQ(WindowKit::Style::clip().kind, WindowKit::Style::Kind::Clip);
    EXPECT_EQ(WindowKit::Style::clip({.x = 5, .y = 6}).origin, (retropp::Vec2i{.x = 5, .y = 6}));
    EXPECT_EQ(WindowKit::Style::squash().kind, WindowKit::Style::Kind::Squash);
    EXPECT_EQ(WindowKit::Style::squash().axis, retropp::Axis::Vertical);
    EXPECT_EQ(WindowKit::Style::squash(retropp::Axis::Horizontal).axis, retropp::Axis::Horizontal);
    EXPECT_EQ(WindowKit::Style::fade().kind, WindowKit::Style::Kind::Fade);
    EXPECT_EQ(WindowKit::Style::slide({.x = 0, .y = 144}).kind, WindowKit::Style::Kind::Slide);
    EXPECT_EQ(WindowKit::Style::slide({.x = 0, .y = 144}).from, (retropp::Vec2i{.x = 0, .y = 144}));
}

// A caller's options: scrolled, half see-through, a region of its own, moved two pixels right.
LayerKit::LayerOptions callers() {
    return LayerKit::LayerOptions{
        .scroll = {.x = 3, .y = 1},
        .alpha = 0.5f,
        .regions = {retropp::Region{.key = "mine"}},
        .transform = retropp::Transform::translation(2, 0),
    };
}

TEST(Animate, AppliesEachStyleOverTheCallersOptions) {
    const WindowKit::Animation half = openedFor(2);

    const WindowKit::Animated pop = WindowKit::Animate(kBox, half, WindowKit::Style::pop(), callers());
    EXPECT_EQ(pop.box, kBox);
    EXPECT_EQ(pop.options.scroll, (retropp::LayerScroll{.x = 3, .y = 1}));
    EXPECT_FLOAT_EQ(pop.options.alpha, 0.5f);

    const WindowKit::Animated grow = WindowKit::Animate(kBox, half, WindowKit::Style::grow(), callers());
    EXPECT_EQ(grow.box, (IntRect{.x = 40, .y = 108, .width = 80, .height = 24}));
    EXPECT_EQ(WindowKit::Animate(kBox, half, WindowKit::Style::grow({.x = 0, .y = 96})).box,
              (IntRect{.x = 0, .y = 96, .width = 80, .height = 24}));

    const WindowKit::Animated clip = WindowKit::Animate(kBox, half, WindowKit::Style::clip(), callers());
    EXPECT_EQ(clip.box, kBox);
    ASSERT_EQ(clip.options.regions.size(), 2u);
    EXPECT_EQ(std::string_view(clip.options.regions[0].key), "mine");
    EXPECT_EQ(std::string_view(clip.options.regions[1].key), "clip");

    const WindowKit::Animated squash = WindowKit::Animate(kBox, half, WindowKit::Style::squash(), callers());
    EXPECT_FLOAT_EQ(squash.options.transform.applyX(0, 96), 2.0f);    // squashed, then moved right
    EXPECT_FLOAT_EQ(squash.options.transform.applyY(0, 96), 108.0f);

    EXPECT_FLOAT_EQ(WindowKit::Animate(kBox, half, WindowKit::Style::fade(), callers()).options.alpha, 0.25f);

    EXPECT_EQ(WindowKit::Animate(kBox, half, WindowKit::Style::slide({.x = 0, .y = 144}), callers()).options.scroll,
              (retropp::LayerScroll{.x = 3, .y = -23}));
}

TEST(Animate, ReturnsItsInputsOnceTheWindowIsOpen) {
    const WindowKit::Animation open = openedFor(4);
    for (const WindowKit::Style& style :
         {WindowKit::Style::pop(), WindowKit::Style::grow(), WindowKit::Style::clip(), WindowKit::Style::squash(),
          WindowKit::Style::fade(), WindowKit::Style::slide({.x = 0, .y = 144})}) {
        const WindowKit::Animated drawn = WindowKit::Animate(kBox, open, style, callers());
        const int kind = static_cast<int>(style.kind);
        EXPECT_EQ(drawn.box, kBox) << kind;
        EXPECT_EQ(drawn.options.scroll, (retropp::LayerScroll{.x = 3, .y = 1})) << kind;
        EXPECT_FLOAT_EQ(drawn.options.alpha, 0.5f) << kind;
        EXPECT_EQ(drawn.options.regions.size(), 1u) << kind;
        EXPECT_EQ(drawn.options.transform, retropp::Transform::translation(2, 0)) << kind;
    }
}

// ── A window with an animation ───────────────────────────────────────────────────────────────

TEST(Window, WithAnAnimationIsEmptyClosedTheBoxAloneMovingAndEverythingOpen) {
    const WindowKit::Border b = border();
    const std::vector<Sprite> child{Sprite{.key = "child", .x = 12, .y = 104}};

    const retropp::DrawLayer closed =
        WindowKit::Window("w-closed", 7, kBox, b, WindowKit::Animation{}, WindowKit::Style::grow(), {child});
    EXPECT_EQ(std::string_view(closed.key), "w-closed");
    EXPECT_EQ(closed.z, 7);
    EXPECT_TRUE(spritesOf(closed).empty());

    const retropp::DrawLayer moving =
        WindowKit::Window("w-moving", 7, kBox, b, openedFor(2), WindowKit::Style::grow(), {child});
    EXPECT_EQ(keys(spritesOf(moving)),
              keys(WindowKit::Box("w-moving:box", {.x = 40, .y = 108, .width = 80, .height = 24}, b)));

    const std::vector<std::string> open =
        keys(spritesOf(WindowKit::Window("w-open", 7, kBox, b, openedFor(4), WindowKit::Style::grow(), {child})));
    const std::vector<std::string> plain = keys(spritesOf(WindowKit::Window("w-open", 7, kBox, b, {child})));
    EXPECT_EQ(open, plain);
    EXPECT_EQ(open.back(), "child");
}

// The engine eases a layer's scroll, alpha and transform from tick to tick by its key, from whatever the
// layer was before. On an opening's first tick the window is an empty layer at the style's start, so
// none of that easing shows; from the next tick the box is drawn, easing on from the start.
TEST(Window, OnTheOpeningsFirstTickIsAnEmptyLayerAtTheStylesStart) {
    const WindowKit::Border b = border();
    const WindowKit::Animation first = openedFor(0);
    for (const WindowKit::Style& style :
         {WindowKit::Style::pop(), WindowKit::Style::grow(), WindowKit::Style::clip(), WindowKit::Style::squash(),
          WindowKit::Style::fade(), WindowKit::Style::slide({.x = 0, .y = 144})}) {
        const int kind = static_cast<int>(style.kind);
        EXPECT_TRUE(spritesOf(WindowKit::Window("w-first", 7, kBox, b, first, style, {})).empty()) << kind;
        EXPECT_FALSE(spritesOf(WindowKit::Window("w-next", 7, kBox, b, openedFor(1), style, {})).empty()) << kind;
    }
    EXPECT_FLOAT_EQ(WindowKit::Window("w-fade", 7, kBox, b, first, WindowKit::Style::fade(), {}).alpha, 0.0f);
    EXPECT_EQ(WindowKit::Window("w-slide", 7, kBox, b, first, WindowKit::Style::slide({.x = 0, .y = 144}), {}).scroll,
              (retropp::LayerScroll{.x = 0, .y = -48}));
    EXPECT_EQ(WindowKit::Window("w-squash", 7, kBox, b, first, WindowKit::Style::squash(), {}).transform,
              WindowKit::Squash(kBox, first));

    // A message box on its opening's first tick is the same.
    WindowKit::Dialog dialog;
    dialog.animation = lasting(4);
    dialog.open(kPassage, kRoom, font());
    const retropp::DrawLayer box =
        WindowKit::DialogBox("msg-fade", 20, kBox, b, font(), dialog, WindowKit::Style::fade());
    EXPECT_TRUE(spritesOf(box).empty());
    EXPECT_FLOAT_EQ(box.alpha, 0.0f);
}

TEST(Box, GrowingSharesNoKeyFromOneTickToTheNext) {
    const WindowKit::Border b = border();
    WindowKit::Animation animation = lasting(4);
    animation.open();
    animation.advance();
    std::vector<std::string> before = keys(WindowKit::Box("w", WindowKit::Grow(kBox, animation), b));
    for (int tick = 2; tick <= 4; ++tick) {
        animation.advance();
        const std::vector<std::string> after = keys(WindowKit::Box("w", WindowKit::Grow(kBox, animation), b));
        ASSERT_FALSE(after.empty());
        for (const std::string& key : before) {
            EXPECT_EQ(std::find(after.begin(), after.end(), key), after.end()) << "tick " << tick << ": " << key;
        }
        before = after;
    }
}

// ── A message whose box opens and closes ─────────────────────────────────────────────────────

// A dialog whose box takes four linear ticks to open and to close, typing a character a tick.
WindowKit::Dialog animated() {
    WindowKit::Dialog dialog;
    dialog.ticksPerGlyph = 1;
    dialog.animation = lasting(4);
    return dialog;
}

TEST(Dialog, TypesOnlyOnceItsBoxIsOpen) {
    WindowKit::Dialog dialog = animated();
    dialog.open(kPassage, kRoom, font());
    EXPECT_TRUE(dialog.showing());
    EXPECT_EQ(dialog.animation.phase(), Phase::Opening);
    for (int i = 0; i < 3; ++i) {
        dialog.advance(false);
        EXPECT_EQ(dialog.animation.phase(), Phase::Opening) << i;
        EXPECT_EQ(dialog.shown(), 0u) << i;
    }
    dialog.advance(false);  // the opening ends, and the page types its first tick
    EXPECT_EQ(dialog.animation.phase(), Phase::Open);
    EXPECT_EQ(dialog.shown(), 1u);
}

TEST(Dialog, AConfirmWhileTheBoxOpensOpensItAtOnceAndNothingMore) {
    WindowKit::Dialog dialog = animated();
    dialog.open(kPassage, kRoom, font());
    dialog.advance(false);
    dialog.advance(true);
    EXPECT_EQ(dialog.animation.phase(), Phase::Open);
    EXPECT_EQ(dialog.shown(), 0u);
    EXPECT_FALSE(dialog.pageDone());
    dialog.advance(false);
    EXPECT_EQ(dialog.shown(), 1u);
}

TEST(Dialog, TheLastPagesConfirmClosesTheBoxOverItsLengthOrAtOnceWithAnother) {
    WindowKit::Dialog dialog = animated();
    dialog.open(kPassage, kRoom, font());
    dialog.advance(true);  // opens the box
    dialog.advance(true);  // shows the first page
    dialog.advance(true);  // turns it
    dialog.advance(true);  // shows the last page
    dialog.advance(true);  // starts the close
    EXPECT_EQ(dialog.animation.phase(), Phase::Closing);
    EXPECT_TRUE(dialog.showing());
    EXPECT_EQ(dialog.page(), 1u);
    EXPECT_EQ(dialog.text(), "lazy dog and runs\nfar away.");
    for (int i = 0; i < 3; ++i) {
        dialog.advance(false);
        EXPECT_TRUE(dialog.showing()) << i;
    }
    dialog.advance(false);
    EXPECT_FALSE(dialog.showing());
    EXPECT_EQ(dialog.pageCount(), 0u);
    EXPECT_EQ(dialog.animation.phase(), Phase::Closed);

    WindowKit::Dialog quick = animated();
    quick.open("Short.", kRoom, font());
    quick.advance(true);
    quick.advance(true);
    quick.advance(true);  // starts the close
    quick.advance(true);  // closes the box at once
    EXPECT_FALSE(quick.showing());
}

TEST(Dialog, ClosePlaysTheClosingAndOpeningAgainGrowsBackWithTheNewText) {
    WindowKit::Dialog dialog = animated();
    dialog.open(kPassage, kRoom, font());
    dialog.advance(true);
    dialog.close();
    EXPECT_EQ(dialog.animation.phase(), Phase::Closing);
    EXPECT_TRUE(dialog.showing());
    dialog.advance(false);
    dialog.open("Another one.", kRoom, font());
    EXPECT_EQ(dialog.animation.phase(), Phase::Opening);
    EXPECT_FLOAT_EQ(dialog.animation.progress(), 0.75f);
    EXPECT_EQ(dialog.pageCount(), 1u);
    EXPECT_EQ(dialog.text(), "Another one.");

    WindowKit::Dialog instant;
    instant.open(kPassage, kRoom, font());
    instant.close();
    EXPECT_FALSE(instant.showing());
}

TEST(DialogBox, WhileTheBoxMovesIsTheBoxAloneAndPassesItsOptionsOnceOpen) {
    const WindowKit::Border b = border();
    const IntRect area = WindowKit::inside(kBox, b);
    WindowKit::Dialog dialog = animated();
    dialog.open(kPassage, kRoom, font());
    dialog.advance(false);
    dialog.advance(false);  // halfway open
    EXPECT_TRUE(WindowKit::DialogText("msg", dialog, {.x = area.x, .y = area.y}, font()).empty());
    EXPECT_EQ(keys(spritesOf(WindowKit::DialogBox("msg", 20, kBox, b, font(), dialog))),
              keys(WindowKit::Box("msg:box", {.x = 40, .y = 108, .width = 80, .height = 24}, b)));

    dialog.advance(true);   // opens it
    dialog.advance(true);   // shows the page: the arrow is on
    ASSERT_TRUE(dialog.arrowOn());
    EXPECT_FALSE(WindowKit::DialogArrow("msg", dialog, area, b).empty());
    const retropp::DrawLayer faded =
        WindowKit::DialogBox("msg", 20, kBox, b, font(), dialog, WindowKit::Style::fade(),
                             {.scroll = {.x = 2, .y = 3}});
    EXPECT_FLOAT_EQ(faded.alpha, 1.0f);
    EXPECT_EQ(faded.scroll, (retropp::LayerScroll{.x = 2, .y = 3}));

    dialog.close();         // closing with the arrow on: no arrow, no text
    ASSERT_TRUE(dialog.arrowOn());
    EXPECT_TRUE(WindowKit::DialogArrow("msg", dialog, area, b).empty());
    EXPECT_TRUE(WindowKit::DialogText("msg", dialog, {.x = area.x, .y = area.y}, font()).empty());
}

// ── A box grown, drawn (through a renderer on a GPU device) ───────────────────────────────────────

class GrowOnDevice : public ::testing::Test {
protected:
    static inline SDL_GPUDevice* device_ = nullptr;
    static inline std::string error_;

    static void SetUpTestSuite() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            error_ = std::string("SDL_Init failed: ") + SDL_GetError();
            return;
        }
        device_ = SDL_CreateGPUDevice(
            SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB,
            /*debug_mode=*/false, /*name=*/nullptr);
        if (device_ == nullptr) {
            error_ = std::string("SDL_CreateGPUDevice failed: ") + SDL_GetError();
        }
    }

    static void TearDownTestSuite() {
        if (device_ != nullptr) {
            SDL_DestroyGPUDevice(device_);
            device_ = nullptr;
        }
        SDL_Quit();
    }

    void SetUp() override {
        if (device_ == nullptr) {
            FAIL() << "no GPU device: " << error_ << ". A grown box is read back from a renderer, which needs one.";
        }
    }
};

// A 24×24 box of one color at (4, 4) in a 32×32 viewport over ground of another, growing from its middle
// over four ticks: at every tick the box's color covers exactly the grown rectangle, every piece in place
// with its cut, and nothing outside it.
TEST_F(GrowOnDevice, AtEveryTickTheDrawnBoxIsExactlyTheGrownRectangle) {
    constexpr int kSide = 32;
    retropp::Renderer renderer{device_, nullptr, retropp::ViewportResolution{kSide, kSide}};
    constexpr std::array<retropp::Rgba8, 2> kColors{{{220, 30, 30, 255}, {30, 30, 220, 255}}};
    const retropp::PaletteId palette = renderer.uploadPalette(std::span<const retropp::Rgba8>(kColors));

    // A border sheet of ten 8×8 slots, every pixel the box's color.
    std::array<std::uint8_t, 24 * 32> pieces{};
    pieces.fill(0);
    const WindowKit::Border border{
        .sheet = retropp::AtlasManifest{
            .atlasId = renderer.uploadAtlas(pieces.data(), 24, 32).atlasId,
            .slots = retropp::sliceLayout({.width = 24, .height = 32}, {.width = 8, .height = 8},
                                          retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown, 10),
            .kind = retropp::ContentKind::Tileset,
        },
        .palette = palette,
    };
    std::array<std::uint8_t, 8 * 8> ground{};
    ground.fill(1);
    const retropp::AtlasId groundSheet = renderer.uploadAtlas(ground.data(), 8, 8).atlasId;
    std::vector<retropp::TileCell> cells(kSide / 8 * (kSide / 8),
                                         retropp::TileCell{.atlas = groundSheet, .tile = 0, .palette = palette});

    const IntRect box{.x = 4, .y = 4, .width = 24, .height = 24};
    WindowKit::Animation animation;
    animation.ticks = 4;
    animation.easing = retropp::Easing::Linear;
    animation.open();

    const std::array<IntRect, 5> expected{{
        {.x = 16, .y = 16, .width = 0, .height = 0},
        {.x = 13, .y = 13, .width = 6, .height = 6},
        {.x = 10, .y = 10, .width = 12, .height = 12},
        {.x = 7, .y = 7, .width = 18, .height = 18},
        box,
    }};
    for (std::size_t tick = 0; tick < expected.size(); ++tick) {
        const IntRect grown = WindowKit::Grow(box, animation);
        ASSERT_EQ(grown, expected[tick]) << "tick " << tick;
        const std::vector<retropp::Sprite> sprites = WindowKit::Box("box", grown, border);

        retropp::DrawLayer back{.key = "ground"};
        back.size = retropp::PixelSize{kSide, kSide};
        back.content = retropp::TileContent{.widthInTiles = kSide / 8, .heightInTiles = kSide / 8,
                                            .cells = std::span<const retropp::TileCell>(cells)};
        retropp::DrawLayer front{.key = "window"};
        front.z = 10;
        front.size = retropp::PixelSize{kSide, kSide};
        front.content = retropp::SpriteContent{.sprites = std::span<const retropp::Sprite>(sprites)};
        retropp::FrameDrawState frame;
        frame.layers.push_back(back);
        frame.layers.push_back(front);
        const std::vector<retropp::Rgba8> pixels = renderer.captureViewport(frame);
        ASSERT_EQ(pixels.size(), static_cast<std::size_t>(kSide * kSide));

        int wrong = 0;
        for (int y = 0; y < kSide; ++y) {
            for (int x = 0; x < kSide; ++x) {
                const retropp::Rgba8 c = pixels[static_cast<std::size_t>(y * kSide + x)];
                const bool inside = x >= grown.x && x < grown.x + grown.width && y >= grown.y &&
                                    y < grown.y + grown.height;
                const bool isBox = c.r > 180 && c.b < 80;
                const bool isGround = c.b > 180 && c.r < 80;
                if (inside ? !isBox : !isGround) {
                    ++wrong;
                    ADD_FAILURE() << "tick " << tick << " at " << x << "," << y << ": " << +c.r << " " << +c.g
                                  << " " << +c.b << (inside ? " (inside the grown box)" : " (outside it)");
                }
            }
        }
        EXPECT_EQ(wrong, 0) << "tick " << tick;
        animation.advance();
    }
    EXPECT_EQ(animation.phase(), WindowKit::Phase::Open);
}

}  // namespace
