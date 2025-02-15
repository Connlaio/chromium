// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_SCOPED_FAKE_NSWINDOW_FULLSCREEN_H_
#define UI_BASE_TEST_SCOPED_FAKE_NSWINDOW_FULLSCREEN_H_

#include <memory>

#include "base/macros.h"

namespace ui {
namespace test {

// Simulates fullscreen transitions on NSWindows. This allows testing fullscreen
// logic without relying on the OS's fullscreen behavior as that tends to be
// flaky, even in interactive tests. We only allow one NSWindow per process to
// be fullscreen at a time, which should be sufficient for tests.
class ScopedFakeNSWindowFullscreen {
 public:
  // Impl class to hide Obj-C implementation from this header.
  class Impl;

  ScopedFakeNSWindowFullscreen();
  ~ScopedFakeNSWindowFullscreen();

 private:
  std::unique_ptr<Impl> impl_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFakeNSWindowFullscreen);
};

}  // namespace test
}  // namespace ui

#endif  // UI_BASE_TEST_SCOPED_FAKE_NSWINDOW_FULLSCREEN_H_
