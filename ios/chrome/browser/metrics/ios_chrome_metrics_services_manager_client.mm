// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/ios_chrome_metrics_services_manager_client.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/pref_service.h"
#include "components/rappor/rappor_service.h"
#include "components/variations/service/variations_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/metrics/ios_chrome_metrics_service_accessor.h"
#include "ios/chrome/browser/metrics/ios_chrome_metrics_service_client.h"
#include "ios/chrome/browser/ui/browser_otr_state.h"
#include "ios/chrome/browser/variations/ios_chrome_variations_service_client.h"
#include "ios/chrome/browser/variations/ios_ui_string_overrider_factory.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

namespace {

void PostStoreMetricsClientInfo(const metrics::ClientInfo& client_info) {}

std::unique_ptr<metrics::ClientInfo> LoadMetricsClientInfo() {
  return std::unique_ptr<metrics::ClientInfo>();
}

}  // namespace

IOSChromeMetricsServicesManagerClient::IOSChromeMetricsServicesManagerClient(
    PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state);
}

IOSChromeMetricsServicesManagerClient::
    ~IOSChromeMetricsServicesManagerClient() = default;

std::unique_ptr<rappor::RapporService>
IOSChromeMetricsServicesManagerClient::CreateRapporService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::WrapUnique(new rappor::RapporService(
      local_state_, base::Bind(&::IsOffTheRecordSessionActive)));
}

std::unique_ptr<variations::VariationsService>
IOSChromeMetricsServicesManagerClient::CreateVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // NOTE: On iOS, disabling background networking is not supported, so pass in
  // a dummy value for the name of the switch that disables background
  // networking.
  return variations::VariationsService::Create(
      base::WrapUnique(new IOSChromeVariationsServiceClient), local_state_,
      GetMetricsStateManager(), "dummy-disable-background-switch",
      ::CreateUIStringOverrider());
}

std::unique_ptr<metrics::MetricsServiceClient>
IOSChromeMetricsServicesManagerClient::CreateMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return IOSChromeMetricsServiceClient::Create(GetMetricsStateManager(),
                                               local_state_);
}

net::URLRequestContextGetter*
IOSChromeMetricsServicesManagerClient::GetURLRequestContext() {
  return GetApplicationContext()->GetSystemURLRequestContext();
}

bool IOSChromeMetricsServicesManagerClient::IsSafeBrowsingEnabled(
    const base::Closure& on_update_callback) {
  return ios::GetChromeBrowserProvider()->IsSafeBrowsingEnabled(
      on_update_callback);
}

bool IOSChromeMetricsServicesManagerClient::IsMetricsReportingEnabled() {
  return IOSChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
}

bool IOSChromeMetricsServicesManagerClient::OnlyDoMetricsRecording() {
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  return cmdline->HasSwitch(switches::kIOSMetricsRecordingOnly);
}

metrics::MetricsStateManager*
IOSChromeMetricsServicesManagerClient::GetMetricsStateManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!metrics_state_manager_) {
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state_, base::Bind(&IOSChromeMetricsServiceAccessor::
                                     IsMetricsAndCrashReportingEnabled),
        base::Bind(&PostStoreMetricsClientInfo),
        base::Bind(&LoadMetricsClientInfo));
  }
  return metrics_state_manager_.get();
}
