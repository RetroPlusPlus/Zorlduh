// The CameraKit module: the follow law on each kind of axis, placing the camera, the scroll a view
// takes, and a pan tick by tick. No window or GPU is needed.

#include <gtest/gtest.h>

#include "retropp/draw_state.h"
#include "retropp/geometry.h"

#include "CameraKit/camera.h"
#include "CameraKit/laws.h"

namespace {

using retropp::IntRect;
using retropp::PixelSize;
using retropp::Vec2i;

constexpr PixelSize kView{.width = 160, .height = 128};
constexpr CameraKit::View kScreen{.size = kView, .origin = {.x = 0, .y = 16}};

// ── The follow law ───────────────────────────────────────────────────────────────────────────

TEST(Follow, CentersTheFocusAndStopsAtTheEdgesOfBoundsWiderThanTheView) {
    const IntRect bounds{.x = 0, .y = 0, .width = 320, .height = 128};
    EXPECT_EQ(CameraKit::follow(bounds, kView, {.x = 160, .y = 64}), (Vec2i{80, 0}));   // centered
    EXPECT_EQ(CameraKit::follow(bounds, kView, {.x = 10, .y = 64}), (Vec2i{0, 0}));     // stopped at the left
    EXPECT_EQ(CameraKit::follow(bounds, kView, {.x = 310, .y = 64}), (Vec2i{160, 0}));  // stopped at the right
    EXPECT_EQ(CameraKit::follow(bounds, kView, {.x = 80, .y = 64}), (Vec2i{0, 0}));     // exactly at the stop
    EXPECT_EQ(CameraKit::follow(bounds, kView, {.x = 81, .y = 64}), (Vec2i{1, 0}));
}

TEST(Follow, HoldsStillOnAnAxisWhereBoundsAreTheViewsSize) {
    const IntRect bounds{.x = 0, .y = 0, .width = 160, .height = 128};
    EXPECT_EQ(CameraKit::follow(bounds, kView, {.x = 5, .y = 5}), (Vec2i{0, 0}));
    EXPECT_EQ(CameraKit::follow(bounds, kView, {.x = 155, .y = 120}), (Vec2i{0, 0}));
}

TEST(Follow, CentersBoundsSmallerThanTheView) {
    const IntRect bounds{.x = 0, .y = 0, .width = 96, .height = 80};
    EXPECT_EQ(CameraKit::follow(bounds, kView, {.x = 10, .y = 10}), (Vec2i{-32, -24}));
    EXPECT_EQ(CameraKit::follow(bounds, kView, {.x = 90, .y = 70}), (Vec2i{-32, -24}));
}

TEST(Follow, BoundsNeedNotStartAtTheOrigin) {
    const IntRect bounds{.x = 160, .y = 0, .width = 320, .height = 128};
    EXPECT_EQ(CameraKit::follow(bounds, kView, {.x = 170, .y = 64}), (Vec2i{160, 0}));
    EXPECT_EQ(CameraKit::follow(bounds, kView, {.x = 320, .y = 64}), (Vec2i{240, 0}));
    EXPECT_EQ(CameraKit::follow(bounds, kView, {.x = 479, .y = 64}), (Vec2i{320, 0}));
}

// ── The camera ───────────────────────────────────────────────────────────────────────────────

TEST(Camera, StartsWhereItIsBuiltAndGoesWhereItIsPlaced) {
    CameraKit::Camera camera{{.x = 40, .y = 8}};
    EXPECT_EQ(camera.position(), (Vec2i{40, 8}));
    camera.place({.x = 96, .y = 0});
    EXPECT_EQ(camera.position(), (Vec2i{96, 0}));
    EXPECT_FALSE(camera.panning());
}

TEST(Camera, ScrollIsThePositionLessTheViewsOrigin) {
    const CameraKit::Camera camera{{.x = 96, .y = 32}};
    EXPECT_EQ(camera.scroll(kScreen), (retropp::LayerScroll{.x = 96, .y = 16}));
    EXPECT_EQ(camera.scroll(CameraKit::View{.size = kView}), (retropp::LayerScroll{.x = 96, .y = 32}));
}

TEST(Camera, PansInAStraightLineArrivingOnTheLastTick) {
    CameraKit::Camera camera;
    camera.pan({.x = 160, .y = 0}, 32);
    EXPECT_TRUE(camera.panning());
    EXPECT_EQ(camera.position(), (Vec2i{0, 0}));
    for (int i = 0; i < 16; ++i) {
        camera.advance();
    }
    EXPECT_EQ(camera.position(), (Vec2i{80, 0}));
    EXPECT_TRUE(camera.panning());
    for (int i = 0; i < 15; ++i) {
        camera.advance();
    }
    EXPECT_EQ(camera.position(), (Vec2i{155, 0}));
    camera.advance();
    EXPECT_EQ(camera.position(), (Vec2i{160, 0}));
    EXPECT_FALSE(camera.panning());
}

TEST(Camera, PanRoundsToTheNearestPixel) {
    CameraKit::Camera camera;
    camera.pan({.x = 10, .y = -10}, 4);
    camera.advance();
    EXPECT_EQ(camera.position(), (Vec2i{3, -3}));  // 2.5 rounds away from zero
    camera.advance();
    EXPECT_EQ(camera.position(), (Vec2i{5, -5}));
    camera.advance();
    EXPECT_EQ(camera.position(), (Vec2i{8, -8}));
    camera.advance();
    EXPECT_EQ(camera.position(), (Vec2i{10, -10}));
}

TEST(Camera, APanOverNoTicksIsAPlacement) {
    CameraKit::Camera camera;
    camera.pan({.x = 7, .y = 9}, 0);
    EXPECT_EQ(camera.position(), (Vec2i{7, 9}));
    EXPECT_FALSE(camera.panning());
}

TEST(Camera, PlacingDropsAPanAndAdvancingWithNoneDoesNothing) {
    CameraKit::Camera camera;
    camera.pan({.x = 100, .y = 0}, 10);
    camera.advance();
    camera.place({.x = 3, .y = 4});
    EXPECT_FALSE(camera.panning());
    camera.advance();
    EXPECT_EQ(camera.position(), (Vec2i{3, 4}));
}

}  // namespace
