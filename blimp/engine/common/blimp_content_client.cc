// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/common/blimp_content_client.h"

#include "components/version_info/version_info.h"
#include "content/public/common/user_agent.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace blimp {
namespace engine {

std::string client_os_info = "Linux; Android 5.1.1";

std::string GetBlimpEngineUserAgent() {
  return content::BuildUserAgentFromOSAndProduct(
      client_os_info,
      version_info::GetProductNameAndVersionForUserAgent() + " Mobile");
}

void SetClientOSInfo(std::string os_version_info) {
  client_os_info = os_version_info;
}

BlimpContentClient::~BlimpContentClient() {}

std::string BlimpContentClient::GetUserAgent() const {
  return GetBlimpEngineUserAgent();
}

base::string16 BlimpContentClient::GetLocalizedString(int message_id) const {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece BlimpContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedStaticMemory* BlimpContentClient::GetDataResourceBytes(
    int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

gfx::Image& BlimpContentClient::GetNativeImageNamed(int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

}  // namespace engine
}  // namespace blimp
