// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/gpu/aw_content_gpu_client.h"

namespace android_webview {

AwContentGpuClient::AwContentGpuClient(
    const GetSyncPointManagerCallback& callback)
    : sync_point_manager_callback_(callback) {}

AwContentGpuClient::~AwContentGpuClient() {}

void AwContentGpuClient::RegisterMojoServices(
    content::ServiceRegistry* registry) {}

gpu::SyncPointManager* AwContentGpuClient::GetSyncPointManager() {
  return sync_point_manager_callback_.Run();
}

}  // namespace android_webview
