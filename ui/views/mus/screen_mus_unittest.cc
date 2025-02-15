// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/screen_mus.h"

#include "base/command_line.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/switches.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/test/views_test_base.h"

namespace views {
namespace {

TEST(ScreenMusTest, ConsistentDisplayInHighDPI) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kForceDeviceScaleFactor, "2");
  ScopedViewsTestHelper test_helper;
  gfx::Screen* screen = gfx::Screen::GetScreen();
  std::vector<gfx::Display> displays = screen->GetAllDisplays();
  ASSERT_FALSE(displays.empty());
  for (const gfx::Display& display : displays) {
    EXPECT_EQ(2.f, display.device_scale_factor());
    EXPECT_EQ(display.work_area(), display.bounds());
  }
}

}  // namespace
}  // namespace views
