// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_SCREEN_WIN_DISPLAY_H_
#define UI_DISPLAY_WIN_SCREEN_WIN_DISPLAY_H_

#include <windows.h>

#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"

namespace display {
namespace win {

class DisplayInfo;

// A display used by display::ScreenWin.
// It holds a display and additional parameters used for DPI calculations.
class ScreenWinDisplay final {
 public:
  ScreenWinDisplay();
  explicit ScreenWinDisplay(const DisplayInfo& display_info);

  const Display& display() const { return display_; }
  const gfx::Rect& pixel_bounds() const { return pixel_bounds_; }

 private:
  Display display_;
  gfx::Rect pixel_bounds_;
};

}  // namespace win
}  // namespace display

#endif  // UI_DISPLAY_WIN_SCREEN_WIN_DISPLAY_H_
