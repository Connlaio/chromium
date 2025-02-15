// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_crash_reporter_client_win.h"

#include <windows.h>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/metrics_constants_util_win.h"
#include "chrome/installer/util/google_chrome_sxs_distribution.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "components/browser_watcher/crash_reporting_metrics_win.h"
#include "content/public/common/content_switches.h"
#include "policy/policy_constants.h"


namespace {

// This is the minimum version of google update that is required for deferred
// crash uploads to work.
const char kMinUpdateVersion[] = "1.3.21.115";

}  // namespace

ChromeCrashReporterClient::ChromeCrashReporterClient() {}

ChromeCrashReporterClient::~ChromeCrashReporterClient() {}

bool ChromeCrashReporterClient::GetAlternativeCrashDumpLocation(
    base::FilePath* crash_dir) {
  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write breakpad crash dumps can be set.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string alternate_crash_dump_location;
  if (env->GetVar("BREAKPAD_DUMP_LOCATION", &alternate_crash_dump_location)) {
    *crash_dir = base::FilePath::FromUTF8Unsafe(alternate_crash_dump_location);
    return true;
  }

  return false;
}

void ChromeCrashReporterClient::GetProductNameAndVersion(
    const base::FilePath& exe_path,
    base::string16* product_name,
    base::string16* version,
    base::string16* special_build,
    base::string16* channel_name) {
  DCHECK(product_name);
  DCHECK(version);
  DCHECK(special_build);
  DCHECK(channel_name);

  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(exe_path));

  if (version_info.get()) {
    // Get the information from the file.
    *version = version_info->product_version();
    if (!version_info->is_official_build())
      version->append(base::ASCIIToUTF16("-devel"));

    *product_name = version_info->product_short_name();
    *special_build = version_info->special_build();
  } else {
    // No version info found. Make up the values.
    *product_name = base::ASCIIToUTF16("Chrome");
    *version = base::ASCIIToUTF16("0.0.0.0-devel");
  }

  GoogleUpdateSettings::GetChromeChannelAndModifiers(
      !GetIsPerUserInstall(exe_path), channel_name);
}

bool ChromeCrashReporterClient::ShouldShowRestartDialog(base::string16* title,
                                                        base::string16* message,
                                                        bool* is_rtl_locale) {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  if (!env->HasVar(env_vars::kShowRestart) ||
      !env->HasVar(env_vars::kRestartInfo) ||
      env->HasVar(env_vars::kMetroConnected)) {
    return false;
  }

  std::string restart_info;
  env->GetVar(env_vars::kRestartInfo, &restart_info);

  // The CHROME_RESTART var contains the dialog strings separated by '|'.
  // See ChromeBrowserMainPartsWin::PrepareRestartOnCrashEnviroment()
  // for details.
  std::vector<std::string> dlg_strings = base::SplitString(
      restart_info, "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (dlg_strings.size() < 3)
    return false;

  *title = base::UTF8ToUTF16(dlg_strings[0]);
  *message = base::UTF8ToUTF16(dlg_strings[1]);
  *is_rtl_locale = dlg_strings[2] == env_vars::kRtlLocale;
  return true;
}

bool ChromeCrashReporterClient::AboutToRestart() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  if (!env->HasVar(env_vars::kRestartInfo))
    return false;

  env->SetVar(env_vars::kShowRestart, "1");
  return true;
}

bool ChromeCrashReporterClient::GetDeferredUploadsSupported(
    bool is_per_user_install) {
  Version update_version = GoogleUpdateSettings::GetGoogleUpdateVersion(
      !is_per_user_install);
  if (!update_version.IsValid() ||
      update_version < base::Version(kMinUpdateVersion))
    return false;

  return true;
}

bool ChromeCrashReporterClient::GetIsPerUserInstall(
    const base::FilePath& exe_path) {
  return InstallUtil::IsPerUserInstall(exe_path);
}

bool ChromeCrashReporterClient::GetShouldDumpLargerDumps(
    bool is_per_user_install) {
  base::string16 channel_name =
      GoogleUpdateSettings::GetChromeChannel(!is_per_user_install);

  // Capture more detail in crash dumps for Beta, Dev, Canary channels and
  // if channel is unknown (e.g. Chromium or developer builds).
  return (channel_name == installer::kChromeChannelBeta ||
          channel_name == installer::kChromeChannelDev ||
          channel_name == GoogleChromeSxSDistribution::ChannelName() ||
          channel_name == installer::kChromeChannelUnknown);
}

int ChromeCrashReporterClient::GetResultCodeRespawnFailed() {
  return chrome::RESULT_CODE_RESPAWN_FAILED;
}

void ChromeCrashReporterClient::InitBrowserCrashDumpsRegKey() {
#if !defined(NACL_WIN64)
  if (GetCollectStatsConsent()){
    crash_reporting_metrics_.reset(new browser_watcher::CrashReportingMetrics(
        chrome::GetBrowserCrashDumpAttemptsRegistryPath()));
  }
#endif
}

void ChromeCrashReporterClient::RecordCrashDumpAttempt(bool is_real_crash) {
#if !defined(NACL_WIN64)
  if (!crash_reporting_metrics_)
    return;

  if (is_real_crash)
    crash_reporting_metrics_->RecordCrashDumpAttempt();
  else
    crash_reporting_metrics_->RecordDumpWithoutCrashAttempt();
#endif
}

void ChromeCrashReporterClient::RecordCrashDumpAttemptResult(bool is_real_crash,
                                                             bool succeeded) {
#if !defined(NACL_WIN64)
  if (!crash_reporting_metrics_)
    return;

  if (is_real_crash)
    crash_reporting_metrics_->RecordCrashDumpAttemptResult(succeeded);
  else
    crash_reporting_metrics_->RecordDumpWithoutCrashAttemptResult(succeeded);
#endif
}

bool ChromeCrashReporterClient::ReportingIsEnforcedByPolicy(
    bool* breakpad_enabled) {
// Determine whether configuration management allows loading the crash reporter.
// Since the configuration management infrastructure is not initialized at this
// point, we read the corresponding registry key directly. The return status
// indicates whether policy data was successfully read. If it is true,
// |breakpad_enabled| contains the value set by policy.
  base::string16 key_name =
      base::UTF8ToUTF16(policy::key::kMetricsReportingEnabled);
  DWORD value = 0;
  base::win::RegKey hklm_policy_key(HKEY_LOCAL_MACHINE,
                                    policy::kRegistryChromePolicyKey, KEY_READ);
  if (hklm_policy_key.ReadValueDW(key_name.c_str(), &value) == ERROR_SUCCESS) {
    *breakpad_enabled = value != 0;
    return true;
  }

  base::win::RegKey hkcu_policy_key(HKEY_CURRENT_USER,
                                    policy::kRegistryChromePolicyKey, KEY_READ);
  if (hkcu_policy_key.ReadValueDW(key_name.c_str(), &value) == ERROR_SUCCESS) {
    *breakpad_enabled = value != 0;
    return true;
  }

  return false;
}


bool ChromeCrashReporterClient::GetCrashDumpLocation(
    base::FilePath* crash_dir) {
  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write breakpad crash dumps can be set.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string alternate_crash_dump_location;
  if (env->GetVar("BREAKPAD_DUMP_LOCATION", &alternate_crash_dump_location)) {
    base::FilePath crash_dumps_dir_path =
        base::FilePath::FromUTF8Unsafe(alternate_crash_dump_location);

    // If this environment variable exists, then for the time being,
    // short-circuit how it's handled on Windows. Honoring this
    // variable is required in order to symbolize stack traces in
    // Telemetry based tests: http://crbug.com/561763.
    *crash_dir = crash_dumps_dir_path;
    return true;
  }

  // TODO(scottmg): Consider supporting --user-data-dir. See
  // https://crbug.com/565446.
  return chrome::GetDefaultCrashDumpLocation(crash_dir);
}

size_t ChromeCrashReporterClient::RegisterCrashKeys() {
  return crash_keys::RegisterChromeCrashKeys();
}

bool ChromeCrashReporterClient::IsRunningUnattended() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  return env->HasVar(env_vars::kHeadless);
}

bool ChromeCrashReporterClient::GetCollectStatsConsent() {
#if defined(GOOGLE_CHROME_BUILD)
  bool is_official_chrome_build = true;
#else
  bool is_official_chrome_build = false;
#endif

  return is_official_chrome_build &&
      GoogleUpdateSettings::GetCollectStatsConsent();
}

bool ChromeCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return process_type == switches::kRendererProcess ||
         process_type == switches::kPpapiPluginProcess ||
         process_type == switches::kZygoteProcess ||
         process_type == switches::kGpuProcess;
}
