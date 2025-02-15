// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WM_OVERVIEW_MODE_OBSERVER_H_
#define ASH_WM_COMMON_WM_OVERVIEW_MODE_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {
namespace wm {

class WmWindow;

class ASH_EXPORT WmOverviewModeObserver {
 public:
  virtual void OnOverviewModeEnded() {}

 protected:
  virtual ~WmOverviewModeObserver() {}
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_WM_OVERVIEW_MODE_OBSERVER_H_
