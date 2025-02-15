// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_state_aura.h"

#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"

namespace ash {
namespace wm {

WindowState* GetActiveWindowState() {
  aura::Window* active = GetActiveWindow();
  return active ? GetWindowState(active) : nullptr;
}

WindowState* GetWindowState(aura::Window* window) {
  if (!window)
    return nullptr;
  WindowState* settings = window->GetProperty(kWindowStateKey);
  if (!settings) {
    settings = new WindowState(WmWindowAura::Get(window));
    window->SetProperty(kWindowStateKey, settings);
  }
  return settings;
}

const WindowState* GetWindowState(const aura::Window* window) {
  return GetWindowState(const_cast<aura::Window*>(window));
}

}  // namespace wm
}  // namespace ash
