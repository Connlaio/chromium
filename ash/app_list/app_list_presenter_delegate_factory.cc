// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_presenter_delegate_factory.h"

#include "ash/app_list/app_list_presenter_delegate.h"
#include "ash/app_list/app_list_view_delegate_factory.h"
#include "base/memory/ptr_util.h"

namespace ash {

AppListPresenterDelegateFactory::AppListPresenterDelegateFactory(
    std::unique_ptr<AppListViewDelegateFactory> view_delegate_factory)
    : view_delegate_factory_(std::move(view_delegate_factory)) {}

AppListPresenterDelegateFactory::~AppListPresenterDelegateFactory() {}

std::unique_ptr<app_list::AppListPresenterDelegate>
AppListPresenterDelegateFactory::GetDelegate(
    app_list::AppListPresenter* presenter) {
  return base::WrapUnique(
      new AppListPresenterDelegate(presenter, view_delegate_factory_.get()));
}

}  // namespace ash
