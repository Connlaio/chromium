// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/startup_task_runner_service.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_errors.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/chromeos_switches.h"
#endif

namespace {

// Simple URLFetcherDelegate with an expected final status and the ability to
// wait until a request completes. It's not considered a failure for the request
// to never complete.
class TestURLFetcherDelegate : public net::URLFetcherDelegate {
 public:
  // Creating the TestURLFetcherDelegate automatically creates and starts a
  // URLFetcher.
  TestURLFetcherDelegate(
      scoped_refptr<net::URLRequestContextGetter> context_getter,
      const GURL& url,
      net::URLRequestStatus expected_request_status)
      : expected_request_status_(expected_request_status),
        is_complete_(false),
        fetcher_(net::URLFetcher::Create(url, net::URLFetcher::GET, this)) {
    fetcher_->SetRequestContext(context_getter.get());
    fetcher_->Start();
  }

  ~TestURLFetcherDelegate() override {}

  void OnURLFetchComplete(const net::URLFetcher* source) override {
    EXPECT_EQ(expected_request_status_.status(), source->GetStatus().status());
    EXPECT_EQ(expected_request_status_.error(), source->GetStatus().error());

    run_loop_.Quit();
  }

  void WaitForCompletion() {
    run_loop_.Run();
  }

  bool is_complete() const { return is_complete_; }

 private:
  const net::URLRequestStatus expected_request_status_;
  base::RunLoop run_loop_;

  bool is_complete_;
  std::unique_ptr<net::URLFetcher> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(TestURLFetcherDelegate);
};

class MockProfileDelegate : public Profile::Delegate {
 public:
  MOCK_METHOD1(OnPrefsLoaded, void(Profile*));
  MOCK_METHOD3(OnProfileCreated, void(Profile*, bool, bool));
};

// Creates a prefs file in the given directory.
void CreatePrefsFileInDirectory(const base::FilePath& directory_path) {
  base::FilePath pref_path(directory_path.Append(chrome::kPreferencesFilename));
  std::string data("{}");
  ASSERT_TRUE(base::WriteFile(pref_path, data.c_str(), data.size()));
}

void CheckChromeVersion(Profile *profile, bool is_new) {
  std::string created_by_version;
  if (is_new) {
    created_by_version = version_info::GetVersionNumber();
  } else {
    created_by_version = "1.0.0.0";
  }
  std::string pref_version =
      ChromeVersionService::GetVersion(profile->GetPrefs());
  // Assert that created_by_version pref gets set to current version.
  EXPECT_EQ(created_by_version, pref_version);
}

void FlushTaskRunner(base::SequencedTaskRunner* runner) {
  ASSERT_TRUE(runner);
  base::WaitableEvent unblock(false, false);

  runner->PostTask(FROM_HERE,
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&unblock)));

  unblock.Wait();
}

void SpinThreads() {
  // Give threads a chance to do their stuff before shutting down (i.e.
  // deleting scoped temp dir etc).
  // Should not be necessary anymore once Profile deletion is fixed
  // (see crbug.com/88586).
  content::RunAllPendingInMessageLoop();
  content::RunAllPendingInMessageLoop(content::BrowserThread::DB);
  content::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
}

}  // namespace

class ProfileBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  // content::BrowserTestBase implementation:

  void SetUpOnMainThread() override {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&chrome_browser_net::SetUrlRequestMocksEnabled, true));
  }

  void TearDownOnMainThread() override {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&chrome_browser_net::SetUrlRequestMocksEnabled, false));
  }

  std::unique_ptr<Profile> CreateProfile(const base::FilePath& path,
                                         Profile::Delegate* delegate,
                                         Profile::CreateMode create_mode) {
    std::unique_ptr<Profile> profile(
        Profile::CreateProfile(path, delegate, create_mode));
    EXPECT_TRUE(profile.get());

    // Store the Profile's IO task runner so we can wind it down.
    profile_io_task_runner_ = profile->GetIOTaskRunner();

    return profile;
  }

  void FlushIoTaskRunnerAndSpinThreads() {
    FlushTaskRunner(profile_io_task_runner_.get());
    SpinThreads();
  }

  // Starts a test where a URLFetcher is active during profile shutdown. The
  // test completes during teardown of the test fixture. The request should be
  // canceled by |context_getter| during profile shutdown, before the
  // URLRequestContext is destroyed. If that doesn't happen, the Context's
  // will still have oustanding requests during its destruction, and will
  // trigger a CHECK failure.
  void StartActiveFetcherDuringProfileShutdownTest(
      scoped_refptr<net::URLRequestContextGetter> context_getter) {
    // This method should only be called once per test.
    DCHECK(!url_fetcher_delegate_);

    // Start a hanging request.  This request may or may not completed before
    // the end of the request.
    url_fetcher_delegate_.reset(new TestURLFetcherDelegate(
        context_getter.get(),
        net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_IO_PENDING),
        net::URLRequestStatus(net::URLRequestStatus::CANCELED,
                              net::ERR_CONTEXT_SHUT_DOWN)));

    // Start a second mock request that just fails, and wait for it to complete.
    // This ensures the first request has reached the network stack.
    TestURLFetcherDelegate url_fetcher_delegate2(
        context_getter.get(),
        net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_FAILED),
        net::URLRequestStatus(net::URLRequestStatus::FAILED,
                              net::ERR_FAILED));
    url_fetcher_delegate2.WaitForCompletion();

    // The first request should still be hung.
    EXPECT_FALSE(url_fetcher_delegate_->is_complete());
  }

  // Runs a test where an incognito profile's URLFetcher is active during
  // teardown of the profile, and makes sure the request fails as expected.
  // Also tries issuing a request after the incognito profile has been
  // destroyed.
  static void RunURLFetcherActiveDuringIncognitoTeardownTest(
      Browser* incognito_browser,
      scoped_refptr<net::URLRequestContextGetter> context_getter) {
    // Start a hanging request.
    TestURLFetcherDelegate url_fetcher_delegate1(
        context_getter.get(),
        net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_IO_PENDING),
        net::URLRequestStatus(net::URLRequestStatus::CANCELED,
                              net::ERR_CONTEXT_SHUT_DOWN));

    // Start a second mock request that just fails, and wait for it to complete.
    // This ensures the first request has reached the network stack.
    TestURLFetcherDelegate url_fetcher_delegate2(
        context_getter.get(),
        net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_FAILED),
        net::URLRequestStatus(net::URLRequestStatus::FAILED,
                              net::ERR_FAILED));
    url_fetcher_delegate2.WaitForCompletion();

    // The first request should still be hung.
    EXPECT_FALSE(url_fetcher_delegate1.is_complete());

    // Close all incognito tabs, starting profile shutdown.
    incognito_browser->tab_strip_model()->CloseAllTabs();

    // The request should have been canceled when the Profile shut down.
    url_fetcher_delegate1.WaitForCompletion();

    // Requests issued after Profile shutdown should fail in a similar manner.
    TestURLFetcherDelegate url_fetcher_delegate3(
        context_getter.get(),
        net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_IO_PENDING),
        net::URLRequestStatus(net::URLRequestStatus::CANCELED,
                              net::ERR_CONTEXT_SHUT_DOWN));
    url_fetcher_delegate3.WaitForCompletion();
  }

  scoped_refptr<base::SequencedTaskRunner> profile_io_task_runner_;

  // URLFetcherDelegate that outlives the Profile, to test shutdown.
  std::unique_ptr<TestURLFetcherDelegate> url_fetcher_delegate_;
};

// Test OnProfileCreate is called with is_new_profile set to true when
// creating a new profile synchronously.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, CreateNewProfileSynchronous) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));

  {
    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.path(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));
    CheckChromeVersion(profile.get(), true);
  }

  FlushIoTaskRunnerAndSpinThreads();
}

// Test OnProfileCreate is called with is_new_profile set to false when
// creating a profile synchronously with an existing prefs file.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, CreateOldProfileSynchronous) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  CreatePrefsFileInDirectory(temp_dir.path());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, false));

  {
    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.path(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));
    CheckChromeVersion(profile.get(), false);
  }

  FlushIoTaskRunnerAndSpinThreads();
}

// Flaky: http://crbug.com/393177
// Test OnProfileCreate is called with is_new_profile set to true when
// creating a new profile asynchronously.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       DISABLED_CreateNewProfileAsynchronous) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));

  {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_PROFILE_CREATED,
        content::NotificationService::AllSources());

    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.path(), &delegate, Profile::CREATE_MODE_ASYNCHRONOUS));

    // Wait for the profile to be created.
    observer.Wait();
    CheckChromeVersion(profile.get(), true);
  }

  FlushIoTaskRunnerAndSpinThreads();
}


// Flaky: http://crbug.com/393177
// Test OnProfileCreate is called with is_new_profile set to false when
// creating a profile asynchronously with an existing prefs file.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       DISABLED_CreateOldProfileAsynchronous) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  CreatePrefsFileInDirectory(temp_dir.path());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, false));

  {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_PROFILE_CREATED,
        content::NotificationService::AllSources());

    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.path(), &delegate, Profile::CREATE_MODE_ASYNCHRONOUS));

    // Wait for the profile to be created.
    observer.Wait();
    CheckChromeVersion(profile.get(), false);
  }

  FlushIoTaskRunnerAndSpinThreads();
}

// Flaky: http://crbug.com/393177
// Test that a README file is created for profiles that didn't have it.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, DISABLED_ProfileReadmeCreated) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));

  {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_PROFILE_CREATED,
        content::NotificationService::AllSources());

    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.path(), &delegate, Profile::CREATE_MODE_ASYNCHRONOUS));

    // Wait for the profile to be created.
    observer.Wait();

    // Verify that README exists.
    EXPECT_TRUE(base::PathExists(
        temp_dir.path().Append(chrome::kReadmeFilename)));
  }

  FlushIoTaskRunnerAndSpinThreads();
}

// Test that repeated setting of exit type is handled correctly.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, ExitType) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));
  {
    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.path(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));

    PrefService* prefs = profile->GetPrefs();
    // The initial state is crashed; store for later reference.
    std::string crash_value(prefs->GetString(prefs::kSessionExitType));

    // The first call to a type other than crashed should change the value.
    profile->SetExitType(Profile::EXIT_SESSION_ENDED);
    std::string first_call_value(prefs->GetString(prefs::kSessionExitType));
    EXPECT_NE(crash_value, first_call_value);

    // Subsequent calls to a non-crash value should be ignored.
    profile->SetExitType(Profile::EXIT_NORMAL);
    std::string second_call_value(prefs->GetString(prefs::kSessionExitType));
    EXPECT_EQ(first_call_value, second_call_value);

    // Setting back to a crashed value should work.
    profile->SetExitType(Profile::EXIT_CRASHED);
    std::string final_value(prefs->GetString(prefs::kSessionExitType));
    EXPECT_EQ(crash_value, final_value);
  }

  FlushIoTaskRunnerAndSpinThreads();
}

// The EndSession IO synchronization is only critical on Windows, but also
// happens under the USE_X11 define. See BrowserProcessImpl::EndSession.
#if defined(USE_X11) || defined(OS_WIN) || defined(USE_OZONE)

namespace {

std::string GetExitTypePreferenceFromDisk(Profile* profile) {
  base::FilePath prefs_path =
      profile->GetPath().Append(chrome::kPreferencesFilename);
  std::string prefs;
  if (!base::ReadFileToString(prefs_path, &prefs))
    return std::string();

  std::unique_ptr<base::Value> value = base::JSONReader::Read(prefs);
  if (!value)
    return std::string();

  base::DictionaryValue* dict = NULL;
  if (!value->GetAsDictionary(&dict) || !dict)
    return std::string();

  std::string exit_type;
  if (!dict->GetString("profile.exit_type", &exit_type))
    return std::string();

  return exit_type;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       WritesProfilesSynchronouslyOnEndSession) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);
  std::vector<Profile*> loaded_profiles = profile_manager->GetLoadedProfiles();

  ASSERT_NE(loaded_profiles.size(), 0UL);
  Profile* profile = loaded_profiles[0];

#if defined(OS_CHROMEOS)
  for (const auto& loaded_profile : loaded_profiles) {
    if (!chromeos::ProfileHelper::IsSigninProfile(loaded_profile)) {
      profile = loaded_profile;
      break;
    }
  }
#endif

  // This retry loop reduces flakiness due to the fact that this ultimately
  // tests whether or not a code path hits a timed wait.
  bool succeeded = false;
  for (size_t retries = 0; !succeeded && retries < 3; ++retries) {
    // Flush the profile data to disk for all loaded profiles.
    profile->SetExitType(Profile::EXIT_CRASHED);
    profile->GetPrefs()->CommitPendingWrite();
    FlushTaskRunner(profile->GetIOTaskRunner().get());

    // Make sure that the prefs file was written with the expected key/value.
    ASSERT_EQ(GetExitTypePreferenceFromDisk(profile), "Crashed");

    // The blocking wait in EndSession has a timeout.
    base::Time start = base::Time::Now();

    // This must not return until the profile data has been written to disk.
    // If this test flakes, then logoff on Windows has broken again.
    g_browser_process->EndSession();

    base::Time end = base::Time::Now();

    // The EndSession timeout is 10 seconds. If we take more than half that,
    // go around again, as we may have timed out on the wait.
    // This helps against flakes, and also ensures that if the IO thread starts
    // blocking systemically for that length of time (e.g. deadlocking or such),
    // we'll get a consistent test failure.
    if (end - start > base::TimeDelta::FromSeconds(5))
      continue;

    // Make sure that the prefs file was written with the expected key/value.
    ASSERT_EQ(GetExitTypePreferenceFromDisk(profile), "SessionEnded");

    // Mark the success.
    succeeded = true;
  }

  ASSERT_TRUE(succeeded) << "profile->EndSession() timed out too often.";
}

#endif  // defined(USE_X11) || defined(OS_WIN) || defined(USE_OZONE)

// The following tests make sure that it's safe to shut down while one of the
// Profile's URLRequestContextGetters is in use by a URLFetcher.

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       URLFetcherUsingMainContextDuringShutdown) {
  StartActiveFetcherDuringProfileShutdownTest(
      browser()->profile()->GetRequestContext());
}

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       URLFetcherUsingMediaContextDuringShutdown) {
  StartActiveFetcherDuringProfileShutdownTest(
      content::BrowserContext::GetDefaultStoragePartition(
          browser()->profile())->GetMediaURLRequestContext());
}

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       URLFetcherUsingExtensionContextDuringShutdown) {
  StartActiveFetcherDuringProfileShutdownTest(
      browser()->profile()->GetRequestContextForExtensions());
}

// The following tests make sure that it's safe to destroy an incognito profile
// while one of the its URLRequestContextGetters is in use by a URLFetcher.

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       URLFetcherUsingMainContextDuringIncognitoTeardown) {
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  RunURLFetcherActiveDuringIncognitoTeardownTest(
      incognito_browser, incognito_browser->profile()->GetRequestContext());
}

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       URLFetcherUsingExtensionContextDuringIncognitoTeardown) {
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  RunURLFetcherActiveDuringIncognitoTeardownTest(
      incognito_browser,
      incognito_browser->profile()->GetRequestContextForExtensions());
}
