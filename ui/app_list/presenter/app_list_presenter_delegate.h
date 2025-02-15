// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_PRESENTER_APP_LIST_PRESENTER_DELEGATE_H_
#define UI_APP_LIST_PRESENTER_APP_LIST_PRESENTER_DELEGATE_H_

#include "ui/app_list/presenter/app_list_presenter_export.h"

namespace aura {
class Window;
}

namespace gfx {
class Vector2d;
}

namespace app_list {

class AppListView;
class AppListViewDelegate;

// Delegate of AppListPresenter which allows to customize AppListPresenter's
// behavior.
// The design of this interface was heavily influenced by the needs of Ash's
// app list implementation (see ash::AppListPresenterDelegate).
class APP_LIST_PRESENTER_EXPORT AppListPresenterDelegate {
 public:
  virtual ~AppListPresenterDelegate() {}

  // Returns the delegate for the app list view, possibly creating one.
  virtual AppListViewDelegate* GetViewDelegate() = 0;

  // Called to initialize the layout of the app list.
  virtual void Init(AppListView* view,
                    aura::Window* root_window,
                    int current_apps_page) = 0;

  // Called when app list is shown.
  virtual void OnShown(aura::Window* root_window) = 0;

  // Called when app list is dismissed
  virtual void OnDismissed() = 0;

  // Update app list bounds if necessary.
  virtual void UpdateBounds() = 0;

  // Returns the offset vector by which the app list window should animate
  // when it gets hidden.
  virtual gfx::Vector2d GetVisibilityAnimationOffset(
      aura::Window* root_window) = 0;
};

}  // namespace app_list

#endif  // UI_APP_LIST_PRESENTER_APP_LIST_PRESENTER_DELEGATE_H_
