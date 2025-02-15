// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_requester.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_ALSA)
#include "media/audio/alsa/audio_manager_alsa.h"
#elif defined(OS_ANDROID)
#include "media/audio/android/audio_manager_android.h"
#elif defined(OS_MACOSX)
#include "media/audio/mac/audio_manager_mac.h"
#elif defined(OS_WIN)
#include "media/audio/win/audio_manager_win.h"
#else
#include "media/audio/fake_audio_manager.h"
#endif

using testing::_;

namespace content {

#if defined(USE_ALSA)
typedef media::AudioManagerAlsa AudioManagerPlatform;
#elif defined(OS_MACOSX)
typedef media::AudioManagerMac AudioManagerPlatform;
#elif defined(OS_WIN)
typedef media::AudioManagerWin AudioManagerPlatform;
#elif defined(OS_ANDROID)
typedef media::AudioManagerAndroid AudioManagerPlatform;
#else
typedef media::FakeAudioManager AudioManagerPlatform;
#endif

namespace {

std::string ReturnMockSalt() {
  return std::string();
}

ResourceContext::SaltCallback GetMockSaltCallback() {
  return base::Bind(&ReturnMockSalt);
}

// This class mocks the audio manager and overrides some methods to ensure that
// we can run our tests on the buildbots.
class MockAudioManager : public AudioManagerPlatform {
 public:
  MockAudioManager()
      : AudioManagerPlatform(base::ThreadTaskRunnerHandle::Get(),
                             base::ThreadTaskRunnerHandle::Get(),
                             &fake_audio_log_factory_),
        num_output_devices_(2),
        num_input_devices_(2) {}
  ~MockAudioManager() override {}

  void GetAudioInputDeviceNames(
      media::AudioDeviceNames* device_names) override {
    DCHECK(device_names->empty());

    // AudioManagers add a default device when there is at least one real device
    if (num_input_devices_ > 0) {
      device_names->push_back(media::AudioDeviceName(
          "Default", AudioManagerBase::kDefaultDeviceId));
    }
    for (size_t i = 0; i < num_input_devices_; i++) {
      device_names->push_back(media::AudioDeviceName(
          std::string("fake_device_name_") + base::SizeTToString(i),
          std::string("fake_device_id_") + base::SizeTToString(i)));
    }
  }

  void GetAudioOutputDeviceNames(
      media::AudioDeviceNames* device_names) override {
    DCHECK(device_names->empty());

    // AudioManagers add a default device when there is at least one real device
    if (num_output_devices_ > 0) {
      device_names->push_back(media::AudioDeviceName(
          "Default", AudioManagerBase::kDefaultDeviceId));
    }
    for (size_t i = 0; i < num_output_devices_; i++) {
      device_names->push_back(media::AudioDeviceName(
          std::string("fake_device_name_") + base::SizeTToString(i),
          std::string("fake_device_id_") + base::SizeTToString(i)));
    }
  }

  media::AudioParameters GetDefaultOutputStreamParameters() override {
    return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  media::CHANNEL_LAYOUT_STEREO, 48000, 16, 128);
  }

  media::AudioParameters GetOutputStreamParameters(
      const std::string& device_id) override {
    return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  media::CHANNEL_LAYOUT_STEREO, 48000, 16, 128);
  }

  void SetNumAudioOutputDevices(size_t num_devices) {
    num_output_devices_ = num_devices;
  }

  void SetNumAudioInputDevices(size_t num_devices) {
    num_input_devices_ = num_devices;
  }

 private:
  media::FakeAudioLogFactory fake_audio_log_factory_;
  size_t num_output_devices_;
  size_t num_input_devices_;
  DISALLOW_COPY_AND_ASSIGN(MockAudioManager);
};

class MockMediaStreamRequester : public MediaStreamRequester {
 public:
  MockMediaStreamRequester(MediaStreamManager* media_stream_manager,
                           base::RunLoop* run_loop_enumeration,
                           size_t num_expected_devices,
                           base::RunLoop* run_loop_devices_changed)
      : media_stream_manager_(media_stream_manager),
        run_loop_enumeration_(run_loop_enumeration),
        num_expected_devices_(num_expected_devices),
        run_loop_devices_changed_(run_loop_devices_changed),
        is_device_change_subscriber_(false) {}
  virtual ~MockMediaStreamRequester() {
    if (is_device_change_subscriber_)
      media_stream_manager_->CancelDeviceChangeNotifications(this);
  }

  // MediaStreamRequester implementation.
  MOCK_METHOD5(StreamGenerated,
               void(int render_frame_id,
                    int page_request_id,
                    const std::string& label,
                    const StreamDeviceInfoArray& audio_devices,
                    const StreamDeviceInfoArray& video_devices));
  MOCK_METHOD3(StreamGenerationFailed,
               void(int render_frame_id,
                    int page_request_id,
                    content::MediaStreamRequestResult result));
  MOCK_METHOD3(DeviceStopped,
               void(int render_frame_id,
                    const std::string& label,
                    const StreamDeviceInfo& device));
  void DevicesEnumerated(int render_frame_id,
                         int page_request_id,
                         const std::string& label,
                         const StreamDeviceInfoArray& devices) override {
    MockDevicesEnumerated(render_frame_id, page_request_id, label, devices);
    EXPECT_EQ(num_expected_devices_, devices.size());

    if (run_loop_enumeration_)
      run_loop_enumeration_->Quit();
  }
  MOCK_METHOD4(MockDevicesEnumerated,
               void(int render_frame_id,
                    int page_request_id,
                    const std::string& label,
                    const StreamDeviceInfoArray& devices));
  MOCK_METHOD4(DeviceOpened,
               void(int render_frame_id,
                    int page_request_id,
                    const std::string& label,
                    const StreamDeviceInfo& device_info));
  void DevicesChanged(MediaStreamType type) override {
    MockDevicesChanged(type);
    if (run_loop_devices_changed_)
      run_loop_devices_changed_->Quit();
  }
  MOCK_METHOD1(MockDevicesChanged, void(MediaStreamType type));

  void SubscribeToDeviceChangeNotifications() {
    if (is_device_change_subscriber_)
      return;

    media_stream_manager_->SubscribeToDeviceChangeNotifications(this);
    is_device_change_subscriber_ = true;
  }

 private:
  MediaStreamManager* media_stream_manager_;
  base::RunLoop* run_loop_enumeration_;
  size_t num_expected_devices_;
  base::RunLoop* run_loop_devices_changed_;
  bool is_device_change_subscriber_;
  DISALLOW_COPY_AND_ASSIGN(MockMediaStreamRequester);
};

}  // namespace

class MediaStreamManagerTest : public ::testing::Test {
 public:
  MediaStreamManagerTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
    audio_manager_.reset(new MockAudioManager());
    media_stream_manager_.reset(new MediaStreamManager(audio_manager_.get()));
    base::RunLoop().RunUntilIdle();
  }

  ~MediaStreamManagerTest() override {}

  MOCK_METHOD1(Response, void(int index));
  void ResponseCallback(int index,
                        const MediaStreamDevices& devices,
                        std::unique_ptr<MediaStreamUIProxy> ui_proxy) {
    Response(index);
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop_.QuitClosure());
  }

 protected:
  std::string MakeMediaAccessRequest(int index) {
    const int render_process_id = 1;
    const int render_frame_id = 1;
    const int page_request_id = 1;
    const GURL security_origin;
    MediaStreamManager::MediaRequestResponseCallback callback =
        base::Bind(&MediaStreamManagerTest::ResponseCallback,
                   base::Unretained(this), index);
    StreamControls controls(true, true);
    return media_stream_manager_->MakeMediaAccessRequest(
        render_process_id, render_frame_id, page_request_id, controls,
        security_origin, callback);
  }

  // media_stream_manager_ needs to outlive thread_bundle_ because it is a
  // MessageLoop::DestructionObserver. audio_manager_ needs to outlive
  // thread_bundle_ because it uses the underlying message loop.
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<MockAudioManager, media::AudioManagerDeleter> audio_manager_;
  base::RunLoop run_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaStreamManagerTest);
};

TEST_F(MediaStreamManagerTest, MakeMediaAccessRequest) {
  MakeMediaAccessRequest(0);

  // Expecting the callback will be triggered and quit the test.
  EXPECT_CALL(*this, Response(0));
  run_loop_.Run();
}

TEST_F(MediaStreamManagerTest, MakeAndCancelMediaAccessRequest) {
  std::string label = MakeMediaAccessRequest(0);
  // No callback is expected.
  media_stream_manager_->CancelRequest(label);
  run_loop_.RunUntilIdle();
}

TEST_F(MediaStreamManagerTest, MakeMultipleRequests) {
  // First request.
  std::string label1 =  MakeMediaAccessRequest(0);

  // Second request.
  int render_process_id = 2;
  int render_frame_id = 2;
  int page_request_id = 2;
  GURL security_origin;
  StreamControls controls(true, true);
  MediaStreamManager::MediaRequestResponseCallback callback =
      base::Bind(&MediaStreamManagerTest::ResponseCallback,
                 base::Unretained(this), 1);
  std::string label2 = media_stream_manager_->MakeMediaAccessRequest(
      render_process_id, render_frame_id, page_request_id, controls,
      security_origin, callback);

  // Expecting the callbackS from requests will be triggered and quit the test.
  // Note, the callbacks might come in a different order depending on the
  // value of labels.
  EXPECT_CALL(*this, Response(0));
  EXPECT_CALL(*this, Response(1));
  run_loop_.Run();
}

TEST_F(MediaStreamManagerTest, MakeAndCancelMultipleRequests) {
  std::string label1 = MakeMediaAccessRequest(0);
  std::string label2 = MakeMediaAccessRequest(1);
  media_stream_manager_->CancelRequest(label1);

  // Expecting the callback from the second request will be triggered and
  // quit the test.
  EXPECT_CALL(*this, Response(1));
  run_loop_.Run();
}

TEST_F(MediaStreamManagerTest, DeviceID) {
  GURL security_origin("http://localhost");
  const std::string unique_default_id(
      media::AudioManagerBase::kDefaultDeviceId);
  const std::string hashed_default_id =
      MediaStreamManager::GetHMACForMediaDeviceID(
          GetMockSaltCallback(), security_origin, unique_default_id);
  EXPECT_TRUE(MediaStreamManager::DoesMediaDeviceIDMatchHMAC(
      GetMockSaltCallback(), security_origin, hashed_default_id,
      unique_default_id));
  EXPECT_EQ(unique_default_id, hashed_default_id);

  const std::string unique_communications_id(
      media::AudioManagerBase::kCommunicationsDeviceId);
  const std::string hashed_communications_id =
      MediaStreamManager::GetHMACForMediaDeviceID(
          GetMockSaltCallback(), security_origin, unique_communications_id);
  EXPECT_TRUE(MediaStreamManager::DoesMediaDeviceIDMatchHMAC(
      GetMockSaltCallback(), security_origin, hashed_communications_id,
      unique_communications_id));
  EXPECT_EQ(unique_communications_id, hashed_communications_id);

  const std::string unique_other_id("other-unique-id");
  const std::string hashed_other_id =
      MediaStreamManager::GetHMACForMediaDeviceID(
          GetMockSaltCallback(), security_origin, unique_other_id);
  EXPECT_TRUE(MediaStreamManager::DoesMediaDeviceIDMatchHMAC(
      GetMockSaltCallback(), security_origin, hashed_other_id,
      unique_other_id));
  EXPECT_NE(unique_other_id, hashed_other_id);
  EXPECT_EQ(hashed_other_id.size(), 64U);
  for (const char& c : hashed_other_id)
    EXPECT_TRUE(base::IsAsciiDigit(c) || (c >= 'a' && c <= 'f'));
}

TEST_F(MediaStreamManagerTest, EnumerationOutputDevices) {
  for (size_t num_devices = 0; num_devices < 3; num_devices++) {
    audio_manager_->SetNumAudioOutputDevices(num_devices);
    base::RunLoop run_loop;
    MockMediaStreamRequester requester(media_stream_manager_.get(), &run_loop,
                                       num_devices == 0 ? 0 : num_devices + 1,
                                       nullptr);
    const int render_process_id = 1;
    const int render_frame_id = 1;
    const int page_request_id = 1;
    const GURL security_origin("http://localhost");
    EXPECT_CALL(requester,
                MockDevicesEnumerated(render_frame_id, page_request_id, _, _));
    std::string label = media_stream_manager_->EnumerateDevices(
        &requester, render_process_id, render_frame_id, GetMockSaltCallback(),
        page_request_id, MEDIA_DEVICE_AUDIO_OUTPUT, security_origin);
    run_loop.Run();
    // CancelRequest is necessary for enumeration requests.
    media_stream_manager_->CancelRequest(label);
  }
}

TEST_F(MediaStreamManagerTest, NotifyDeviceChanges) {
  const int render_process_id = 1;
  const int render_frame_id = 1;
  const int page_request_id = 1;
  const GURL security_origin("http://localhost");

  // Check that device change notifications are received
  {
    // First run an enumeration to warm up the cache
    base::RunLoop run_loop_enumeration;
    base::RunLoop run_loop_device_change;
    MockMediaStreamRequester requester(media_stream_manager_.get(),
                                       &run_loop_enumeration, 3,
                                       &run_loop_device_change);
    audio_manager_->SetNumAudioInputDevices(2);
    requester.SubscribeToDeviceChangeNotifications();

    EXPECT_CALL(requester,
                MockDevicesEnumerated(render_frame_id, page_request_id, _, _));
    EXPECT_CALL(requester, MockDevicesChanged(_)).Times(0);
    std::string label = media_stream_manager_->EnumerateDevices(
        &requester, render_process_id, render_frame_id, GetMockSaltCallback(),
        page_request_id, MEDIA_DEVICE_AUDIO_CAPTURE, security_origin);
    run_loop_enumeration.Run();
    media_stream_manager_->CancelRequest(label);

    // Simulate device change
    EXPECT_CALL(requester, MockDevicesChanged(_));
    audio_manager_->SetNumAudioInputDevices(3);
    media_stream_manager_->OnDevicesChanged(
        base::SystemMonitor::DEVTYPE_AUDIO_CAPTURE);
    run_loop_device_change.Run();
  }

  // Check that bogus device changes where devices have not changed
  // do not trigger a notification.
  {
    base::RunLoop run_loop;
    MockMediaStreamRequester requester(media_stream_manager_.get(), &run_loop,
                                       4, &run_loop);
    requester.SubscribeToDeviceChangeNotifications();

    // Bogus OnDeviceChange, as devices have not changed. Should not trigger
    // notification.
    EXPECT_CALL(requester, MockDevicesChanged(_)).Times(0);
    media_stream_manager_->OnDevicesChanged(
        base::SystemMonitor::DEVTYPE_AUDIO_CAPTURE);

    // Do enumeration to be able to quit the RunLoop.
    EXPECT_CALL(requester,
                MockDevicesEnumerated(render_frame_id, page_request_id, _, _));
    std::string label = media_stream_manager_->EnumerateDevices(
        &requester, render_process_id, render_frame_id, GetMockSaltCallback(),
        page_request_id, MEDIA_DEVICE_AUDIO_CAPTURE, security_origin);
    run_loop.Run();
    media_stream_manager_->CancelRequest(label);
  }
}

}  // namespace content
