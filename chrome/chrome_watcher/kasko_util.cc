// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_watcher/kasko_util.h"

#include <sddl.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/wait_chain.h"
#include "base/win/win_util.h"

#include "chrome/chrome_watcher/chrome_watcher_main_api.h"
#include "components/crash/content/app/crashpad.h"
#include "syzygy/kasko/api/reporter.h"

namespace {

// Helper function for determining the crash server to use. Defaults to the
// standard crash server, but can be overridden via an environment variable.
// Enables easy integration testing.
base::string16 GetKaskoCrashServerUrl() {
  static const char kKaskoCrashServerUrl[] = "KASKO_CRASH_SERVER_URL";
  static const wchar_t kDefaultKaskoCrashServerUrl[] =
      L"https://clients2.google.com/cr/report";

  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string env_var;
  if (env->GetVar(kKaskoCrashServerUrl, &env_var)) {
    return base::UTF8ToUTF16(env_var);
  }
  return kDefaultKaskoCrashServerUrl;
}

// Helper function for determining the crash reports directory to use. Defaults
// to the browser data directory, but can be overridden via an environment
// variable. Enables easy integration testing.
base::FilePath GetKaskoCrashReportsBaseDir(
    const base::char16* browser_data_directory) {
  static const char kKaskoCrashReportBaseDir[] = "KASKO_CRASH_REPORTS_BASE_DIR";
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string env_var;
  if (env->GetVar(kKaskoCrashReportBaseDir, &env_var)) {
    return base::FilePath(base::UTF8ToUTF16(env_var));
  }
  return base::FilePath(browser_data_directory);
}

struct EventSourceDeregisterer {
  using pointer = HANDLE;
  void operator()(HANDLE event_source_handle) const {
    if (!::DeregisterEventSource(event_source_handle))
      DPLOG(ERROR) << "DeregisterEventSource";
  }
};
using ScopedEventSourceHandle =
    std::unique_ptr<HANDLE, EventSourceDeregisterer>;

struct SidDeleter {
  using pointer = PSID;
  void operator()(PSID sid) const {
    if (::LocalFree(sid) != nullptr)
      DPLOG(ERROR) << "LocalFree";
  }
};
using ScopedSid = std::unique_ptr<PSID, SidDeleter>;

void OnCrashReportUpload(void* context,
                         const base::char16* report_id,
                         const base::char16* minidump_path,
                         const base::char16* const* keys,
                         const base::char16* const* values) {
  // Open the event source.
  ScopedEventSourceHandle event_source_handle(
      ::RegisterEventSource(nullptr, L"Chrome"));
  if (!event_source_handle) {
    PLOG(ERROR) << "RegisterEventSource";
    return;
  }

  // Get the user's SID for the log record.
  base::string16 sid_string;
  PSID sid = nullptr;
  if (base::win::GetUserSidString(&sid_string) && !sid_string.empty()) {
    if (!::ConvertStringSidToSid(sid_string.c_str(), &sid))
      DPLOG(ERROR) << "ConvertStringSidToSid";
    DCHECK(sid);
  }
  // Ensure cleanup on scope exit.
  ScopedSid scoped_sid;
  if (sid)
    scoped_sid.reset(sid);

  // Generate the message.
  // Note that the format of this message must match the consumer in
  // chrome/browser/crash_upload_list_win.cc.
  base::string16 message =
      L"Crash uploaded. Id=" + base::string16(report_id) + L".";

  // Matches Omaha.
  const int kCrashUploadEventId = 2;

  // Report the event.
  const base::char16* strings[] = {message.c_str()};
  if (!::ReportEvent(event_source_handle.get(), EVENTLOG_INFORMATION_TYPE,
                     0,  // category
                     kCrashUploadEventId, sid,
                     1,  // count
                     0, strings, nullptr)) {
    DPLOG(ERROR);
  }
}

void AddCrashKey(const wchar_t *key, const wchar_t *value,
                 std::vector<kasko::api::CrashKey> *crash_keys) {
  DCHECK(key);
  DCHECK(value);
  DCHECK(crash_keys);

  crash_keys->resize(crash_keys->size() + 1);
  kasko::api::CrashKey& crash_key = crash_keys->back();
  base::wcslcpy(crash_key.name, key, kasko::api::CrashKey::kNameMaxLength);
  base::wcslcpy(crash_key.value, value, kasko::api::CrashKey::kValueMaxLength);
}

// Get the |process| and the |thread_id| of the node inside the |wait_chain|
// that is of type ThreadType and belongs to a process that is valid for the
// capture of a crash dump. Returns if such a node was found.
bool GetLastValidNodeInfo(const base::win::WaitChainNodeVector& wait_chain,
                          base::Process* process,
                          DWORD* thread_id) {
  // The last thread in the wait chain is nominated as the hung thread.
  base::win::WaitChainNodeVector::const_reverse_iterator it;
  for (it = wait_chain.rbegin(); it != wait_chain.rend(); ++it) {
    if (it->ObjectType != WctThreadType)
      continue;

    auto current_process = base::Process::Open(it->ThreadObject.ProcessId);
    if (EnsureTargetProcessValidForCapture(current_process)) {
      *process = std::move(current_process);
      *thread_id = it->ThreadObject.ThreadId;
      return true;
    }
  }
  return false;
}

}  // namespace

bool InitializeKaskoReporter(const base::string16& endpoint,
                             const base::char16* browser_data_directory) {
  base::string16 crash_server = GetKaskoCrashServerUrl();
  base::FilePath crash_reports_base_dir =
      GetKaskoCrashReportsBaseDir(browser_data_directory);

  return kasko::api::InitializeReporter(
      endpoint.c_str(),
      crash_server.c_str(),
      crash_reports_base_dir.Append(L"Crash Reports").value().c_str(),
      crash_reports_base_dir.Append(kPermanentlyFailedReportsSubdir)
          .value()
          .c_str(),
      &OnCrashReportUpload,
      nullptr);
}

void ShutdownKaskoReporter() {
  kasko::api::ShutdownReporter();
}

bool EnsureTargetProcessValidForCapture(const base::Process& process) {
  // Ensure the target process shares the current process's executable name.
  base::FilePath exe_self;
  if (!PathService::Get(base::FILE_EXE, &exe_self))
    return false;

  wchar_t exe_name_other[MAX_PATH];
  DWORD exe_name_other_len = arraysize(exe_name_other);
  // Note: requesting the Win32 path format.
  if (::QueryFullProcessImageName(process.Handle(), 0, exe_name_other,
                                  &exe_name_other_len) == 0) {
    DPLOG(ERROR) << "Failed to get executable name for other process";
    return false;
  }

  // QueryFullProcessImageName's documentation does not specify behavior when
  // the buffer is too small, but we know that GetModuleFileNameEx succeeds and
  // truncates the returned name in such a case. Given that paths of arbitrary
  // length may exist, the conservative approach is to reject names when
  // the returned length is that of the buffer.
  if (exe_name_other_len > 0 &&
      exe_name_other_len < arraysize(exe_name_other)) {
    return base::FilePath::CompareEqualIgnoreCase(exe_self.value(),
                                                  exe_name_other);
  }
  return false;
}

void DumpHungProcess(DWORD main_thread_id, const base::string16& channel,
                     const base::char16* key, const base::Process& process) {
  // Read the Crashpad module annotations for the process.
  std::vector<kasko::api::CrashKey> annotations;
  crash_reporter::ReadMainModuleAnnotationsForKasko(process, &annotations);
  AddCrashKey(key, L"1", &annotations);

  // Use the Wait Chain Traversal API to determine the hung thread. Defaults to
  // UI thread on error. The wait chain may point to a different thread in a
  // different process for the hung thread.
  DWORD hung_thread_id = main_thread_id;
  base::Process hung_process = process.Duplicate();

  base::win::WaitChainNodeVector wait_chain;
  bool is_deadlock = false;
  base::string16 thread_chain_failure_reason;
  DWORD thread_chain_last_error = ERROR_SUCCESS;
  if (base::win::GetThreadWaitChain(main_thread_id, &wait_chain, &is_deadlock,
                                    &thread_chain_failure_reason,
                                    &thread_chain_last_error)) {
    bool found_valid_node =
        GetLastValidNodeInfo(wait_chain, &hung_process, &hung_thread_id);
    DCHECK(found_valid_node);

    // The entire wait chain is added to the crash report via crash keys.
    //
    // As an example (key : value):
    // hung-process-is-deadlock  : false
    // hung-process-wait-chain-00 : Thread #10242 with status Blocked
    // hung-process-wait-chain-01 : Lock of type ThreadWait with status Owned
    // hung-process-wait-chain-02 : Thread #77221 with status Blocked
    //
    AddCrashKey(L"hung-process-is-deadlock", is_deadlock ? L"true" : L"false",
                &annotations);
    for (size_t i = 0; i < wait_chain.size(); i++) {
      AddCrashKey(
          base::StringPrintf(L"hung-process-wait-chain-%02" PRIuS, i).c_str(),
          base::win::WaitChainNodeToString(wait_chain[i]).c_str(),
          &annotations);
    }
  } else {
    // The call to GetThreadWaitChain() failed. Include the reason inside the
    // report using crash keys.
    // TODO(pmonette): Remove this when UMA is added to wait_chain.cc.
    AddCrashKey(L"hung-process-wait-chain-failure-reason",
                thread_chain_failure_reason.c_str(), &annotations);
    AddCrashKey(L"hung-process-wait-chain-last-error",
                base::UintToString16(thread_chain_last_error).c_str(),
                &annotations);
  }

  std::vector<const base::char16*> key_buffers;
  std::vector<const base::char16*> value_buffers;
  for (const auto& crash_key : annotations) {
    key_buffers.push_back(crash_key.name);
    value_buffers.push_back(crash_key.value);
  }
  key_buffers.push_back(nullptr);
  value_buffers.push_back(nullptr);

  // Synthesize an exception for the hung thread. Populate the record with the
  // current context of the thread to get the stack trace bucketed on the crash
  // backend.
  CONTEXT thread_context = {};
  EXCEPTION_RECORD exception_record = {};
  exception_record.ExceptionCode = EXCEPTION_ARRAY_BOUNDS_EXCEEDED;
  EXCEPTION_POINTERS exception_pointers = {&exception_record, &thread_context};

  base::win::ScopedHandle hung_thread(::OpenThread(
      THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
      FALSE, hung_thread_id));

  bool have_context = false;
  if (hung_thread.IsValid()) {
    DWORD suspend_count = ::SuspendThread(hung_thread.Get());
    const DWORD kSuspendFailed = static_cast<DWORD>(-1);
    if (suspend_count != kSuspendFailed) {
      // Best effort capture of the context.
      thread_context.ContextFlags = CONTEXT_FLOATING_POINT | CONTEXT_SEGMENTS |
                                    CONTEXT_INTEGER | CONTEXT_CONTROL;
      if (::GetThreadContext(hung_thread.Get(), &thread_context) == TRUE)
        have_context = true;

      ::ResumeThread(hung_thread.Get());
    }
  }

  // TODO(manzagop): consider making the dump-type channel-dependent.
  if (have_context) {
    kasko::api::SendReportForProcess(
        hung_process.Handle(), hung_thread_id, &exception_pointers,
        kasko::api::LARGER_DUMP_TYPE, key_buffers.data(), value_buffers.data());
  } else {
    kasko::api::SendReportForProcess(hung_process.Handle(), 0, nullptr,
                                     kasko::api::LARGER_DUMP_TYPE,
                                     key_buffers.data(), value_buffers.data());
  }
}
