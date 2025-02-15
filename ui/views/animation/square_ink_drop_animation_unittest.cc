// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_UNITTEST_H_
#define UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_UNITTEST_H_

#include "ui/views/animation/square_ink_drop_animation.h"

#include <memory>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/views/animation/ink_drop_animation_observer.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/animation/test/square_ink_drop_animation_test_api.h"
#include "ui/views/animation/test/test_ink_drop_animation_observer.h"

namespace views {
namespace test {

namespace {

using PaintedShape = views::test::SquareInkDropAnimationTestApi::PaintedShape;

// Transforms a copy of |point| with |transform| and returns it.
gfx::Point TransformPoint(const gfx::Transform& transform,
                          const gfx::Point& point) {
  gfx::Point transformed_point = point;
  transform.TransformPoint(&transformed_point);
  return transformed_point;
}

class SquareInkDropAnimationCalculateTransformsTest : public testing::Test {
 public:
  SquareInkDropAnimationCalculateTransformsTest();
  ~SquareInkDropAnimationCalculateTransformsTest() override;

 protected:
  // Half the width/height of the drawn ink drop.
  static const int kHalfDrawnSize;

  // The full width/height of the drawn ink drop.
  static const int kDrawnSize;

  // The radius of the rounded rectangle corners.
  static const int kTransformedRadius;

  // Half the width/height of the transformed ink drop.
  static const int kHalfTransformedSize;

  // The full width/height of the transformed ink drop.
  static const int kTransformedSize;

  // Constant points in the drawn space that will be transformed.
  static const gfx::Point kDrawnCenterPoint;
  static const gfx::Point kDrawnMidLeftPoint;
  static const gfx::Point kDrawnMidRightPoint;
  static const gfx::Point kDrawnTopMidPoint;
  static const gfx::Point kDrawnBottomMidPoint;

  // The test target.
  SquareInkDropAnimation ink_drop_animation_;

  // Provides internal access to the test target.
  SquareInkDropAnimationTestApi test_api_;

  // The gfx::Transforms collection that is populated via the
  // Calculate*Transforms() calls.
  SquareInkDropAnimationTestApi::InkDropTransforms transforms_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SquareInkDropAnimationCalculateTransformsTest);
};

const int SquareInkDropAnimationCalculateTransformsTest::kHalfDrawnSize = 5;
const int SquareInkDropAnimationCalculateTransformsTest::kDrawnSize =
    2 * kHalfDrawnSize;

const int SquareInkDropAnimationCalculateTransformsTest::kTransformedRadius =
    10;
const int SquareInkDropAnimationCalculateTransformsTest::kHalfTransformedSize =
    100;
const int SquareInkDropAnimationCalculateTransformsTest::kTransformedSize =
    2 * kHalfTransformedSize;

const gfx::Point
    SquareInkDropAnimationCalculateTransformsTest::kDrawnCenterPoint =
        gfx::Point(kHalfDrawnSize, kHalfDrawnSize);

const gfx::Point
    SquareInkDropAnimationCalculateTransformsTest::kDrawnMidLeftPoint =
        gfx::Point(0, kHalfDrawnSize);

const gfx::Point
    SquareInkDropAnimationCalculateTransformsTest::kDrawnMidRightPoint =
        gfx::Point(kDrawnSize, kHalfDrawnSize);

const gfx::Point
    SquareInkDropAnimationCalculateTransformsTest::kDrawnTopMidPoint =
        gfx::Point(kHalfDrawnSize, 0);

const gfx::Point
    SquareInkDropAnimationCalculateTransformsTest::kDrawnBottomMidPoint =
        gfx::Point(kHalfDrawnSize, kDrawnSize);

SquareInkDropAnimationCalculateTransformsTest::
    SquareInkDropAnimationCalculateTransformsTest()
    : ink_drop_animation_(gfx::Size(kDrawnSize, kDrawnSize),
                          2,
                          gfx::Size(kHalfDrawnSize, kHalfDrawnSize),
                          1,
                          gfx::Point(),
                          SK_ColorBLACK),
      test_api_(&ink_drop_animation_) {}

SquareInkDropAnimationCalculateTransformsTest::
    ~SquareInkDropAnimationCalculateTransformsTest() {}

}  // namespace

TEST_F(SquareInkDropAnimationCalculateTransformsTest,
       TransformedPointsUsingTransformsFromCalculateCircleTransforms) {
  test_api_.CalculateCircleTransforms(
      gfx::Size(kTransformedSize, kTransformedSize), &transforms_);

  struct {
    PaintedShape shape;
    gfx::Point center_point;
    gfx::Point mid_left_point;
    gfx::Point mid_right_point;
    gfx::Point top_mid_point;
    gfx::Point bottom_mid_point;
  } test_cases[] = {
      {PaintedShape::TOP_LEFT_CIRCLE, gfx::Point(0, 0),
       gfx::Point(-kHalfTransformedSize, 0),
       gfx::Point(kHalfTransformedSize, 0),
       gfx::Point(0, -kHalfTransformedSize),
       gfx::Point(0, kHalfTransformedSize)},
      {PaintedShape::TOP_RIGHT_CIRCLE, gfx::Point(0, 0),
       gfx::Point(-kHalfTransformedSize, 0),
       gfx::Point(kHalfTransformedSize, 0),
       gfx::Point(0, -kHalfTransformedSize),
       gfx::Point(0, kHalfTransformedSize)},
      {PaintedShape::BOTTOM_RIGHT_CIRCLE, gfx::Point(0, 0),
       gfx::Point(-kHalfTransformedSize, 0),
       gfx::Point(kHalfTransformedSize, 0),
       gfx::Point(0, -kHalfTransformedSize),
       gfx::Point(0, kHalfTransformedSize)},
      {PaintedShape::BOTTOM_LEFT_CIRCLE, gfx::Point(0, 0),
       gfx::Point(-kHalfTransformedSize, 0),
       gfx::Point(kHalfTransformedSize, 0),
       gfx::Point(0, -kHalfTransformedSize),
       gfx::Point(0, kHalfTransformedSize)},
      {PaintedShape::HORIZONTAL_RECT, gfx::Point(0, 0),
       gfx::Point(-kHalfTransformedSize, 0),
       gfx::Point(kHalfTransformedSize, 0), gfx::Point(0, 0), gfx::Point(0, 0)},
      {PaintedShape::VERTICAL_RECT, gfx::Point(0, 0), gfx::Point(0, 0),
       gfx::Point(0, 0), gfx::Point(0, -kHalfTransformedSize),
       gfx::Point(0, kHalfTransformedSize)}};

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    PaintedShape shape = test_cases[i].shape;
    SCOPED_TRACE(testing::Message() << "i=" << i << " shape=" << shape);
    gfx::Transform transform = transforms_[shape];

    EXPECT_EQ(test_cases[i].center_point,
              TransformPoint(transform, kDrawnCenterPoint));
    EXPECT_EQ(test_cases[i].mid_left_point,
              TransformPoint(transform, kDrawnMidLeftPoint));
    EXPECT_EQ(test_cases[i].mid_right_point,
              TransformPoint(transform, kDrawnMidRightPoint));
    EXPECT_EQ(test_cases[i].top_mid_point,
              TransformPoint(transform, kDrawnTopMidPoint));
    EXPECT_EQ(test_cases[i].bottom_mid_point,
              TransformPoint(transform, kDrawnBottomMidPoint));
  }
}

TEST_F(SquareInkDropAnimationCalculateTransformsTest,
       TransformedPointsUsingTransformsFromCalculateRectTransforms) {
  test_api_.CalculateRectTransforms(
      gfx::Size(kTransformedSize, kTransformedSize), kTransformedRadius,
      &transforms_);

  const int x_offset = kHalfTransformedSize - kTransformedRadius;
  const int y_offset = kHalfTransformedSize - kTransformedRadius;

  struct {
    PaintedShape shape;
    gfx::Point center_point;
    gfx::Point mid_left_point;
    gfx::Point mid_right_point;
    gfx::Point top_mid_point;
    gfx::Point bottom_mid_point;
  } test_cases[] = {
      {PaintedShape::TOP_LEFT_CIRCLE, gfx::Point(-x_offset, -y_offset),
       gfx::Point(-kHalfTransformedSize, -y_offset),
       gfx::Point(-x_offset + kTransformedRadius, -y_offset),
       gfx::Point(-x_offset, -kHalfTransformedSize),
       gfx::Point(-x_offset, -y_offset + kTransformedRadius)},
      {PaintedShape::TOP_RIGHT_CIRCLE, gfx::Point(x_offset, -y_offset),
       gfx::Point(x_offset - kTransformedRadius, -y_offset),
       gfx::Point(kHalfTransformedSize, -y_offset),
       gfx::Point(x_offset, -kHalfTransformedSize),
       gfx::Point(x_offset, -y_offset + kTransformedRadius)},
      {PaintedShape::BOTTOM_RIGHT_CIRCLE, gfx::Point(x_offset, y_offset),
       gfx::Point(x_offset - kTransformedRadius, y_offset),
       gfx::Point(kHalfTransformedSize, y_offset),
       gfx::Point(x_offset, y_offset - kTransformedRadius),
       gfx::Point(x_offset, kHalfTransformedSize)},
      {PaintedShape::BOTTOM_LEFT_CIRCLE, gfx::Point(-x_offset, y_offset),
       gfx::Point(-kHalfTransformedSize, y_offset),
       gfx::Point(-x_offset + kTransformedRadius, y_offset),
       gfx::Point(-x_offset, y_offset - kTransformedRadius),
       gfx::Point(-x_offset, kHalfTransformedSize)},
      {PaintedShape::HORIZONTAL_RECT, gfx::Point(0, 0),
       gfx::Point(-kHalfTransformedSize, 0),
       gfx::Point(kHalfTransformedSize, 0), gfx::Point(0, -y_offset),
       gfx::Point(0, y_offset)},
      {PaintedShape::VERTICAL_RECT, gfx::Point(0, 0), gfx::Point(-x_offset, 0),
       gfx::Point(x_offset, 0), gfx::Point(0, -kHalfTransformedSize),
       gfx::Point(0, kHalfTransformedSize)}};

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    PaintedShape shape = test_cases[i].shape;
    SCOPED_TRACE(testing::Message() << "i=" << i << " shape=" << shape);
    gfx::Transform transform = transforms_[shape];

    EXPECT_EQ(test_cases[i].center_point,
              TransformPoint(transform, kDrawnCenterPoint));
    EXPECT_EQ(test_cases[i].mid_left_point,
              TransformPoint(transform, kDrawnMidLeftPoint));
    EXPECT_EQ(test_cases[i].mid_right_point,
              TransformPoint(transform, kDrawnMidRightPoint));
    EXPECT_EQ(test_cases[i].top_mid_point,
              TransformPoint(transform, kDrawnTopMidPoint));
    EXPECT_EQ(test_cases[i].bottom_mid_point,
              TransformPoint(transform, kDrawnBottomMidPoint));
  }
}

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_UNITTEST_H_
