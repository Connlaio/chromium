// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/sounds/audio_stream_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/test_message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/simple_sources.h"
#include "media/audio/sounds/test_data.h"
#include "media/base/channel_layout.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioStreamHandlerTest : public testing::Test {
 public:
  AudioStreamHandlerTest() {}
  ~AudioStreamHandlerTest() override {}

  void SetUp() override {
    audio_manager_ =
        AudioManager::CreateForTesting(base::ThreadTaskRunnerHandle::Get());
    base::RunLoop().RunUntilIdle();

    base::StringPiece data(kTestAudioData, arraysize(kTestAudioData));
    audio_stream_handler_.reset(new AudioStreamHandler(data));
  }

  void TearDown() override {
    audio_stream_handler_.reset();
    base::RunLoop().RunUntilIdle();
  }

  AudioStreamHandler* audio_stream_handler() {
    return audio_stream_handler_.get();
  }

  void SetObserverForTesting(AudioStreamHandler::TestObserver* observer) {
    AudioStreamHandler::SetObserverForTesting(observer);
  }

  void SetAudioSourceForTesting(
      AudioOutputStream::AudioSourceCallback* source) {
    AudioStreamHandler::SetAudioSourceForTesting(source);
  }

 private:
  base::TestMessageLoop message_loop_;
  ScopedAudioManagerPtr audio_manager_;
  std::unique_ptr<AudioStreamHandler> audio_stream_handler_;
};

TEST_F(AudioStreamHandlerTest, Play) {
  base::RunLoop run_loop;
  TestObserver observer(run_loop.QuitClosure());

  SetObserverForTesting(&observer);

  ASSERT_TRUE(audio_stream_handler()->IsInitialized());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(20u),
            audio_stream_handler()->duration());

  ASSERT_TRUE(audio_stream_handler()->Play());

  run_loop.Run();

  SetObserverForTesting(NULL);

  ASSERT_EQ(1, observer.num_play_requests());
  ASSERT_EQ(1, observer.num_stop_requests());
  ASSERT_EQ(4, observer.cursor());
}

TEST_F(AudioStreamHandlerTest, ConsecutivePlayRequests) {
  base::RunLoop run_loop;
  TestObserver observer(run_loop.QuitClosure());
  SineWaveAudioSource source(CHANNEL_LAYOUT_STEREO, 200.0, 8000);

  SetObserverForTesting(&observer);
  SetAudioSourceForTesting(&source);

  ASSERT_TRUE(audio_stream_handler()->IsInitialized());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(20u),
            audio_stream_handler()->duration());

  ASSERT_TRUE(audio_stream_handler()->Play());
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&AudioStreamHandler::Play),
                 base::Unretained(audio_stream_handler())),
      base::TimeDelta::FromSeconds(1));
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&AudioStreamHandler::Stop,
                 base::Unretained(audio_stream_handler())),
      base::TimeDelta::FromSeconds(2));

  run_loop.Run();

  SetObserverForTesting(NULL);
  SetAudioSourceForTesting(NULL);

  ASSERT_EQ(1, observer.num_play_requests());
  ASSERT_EQ(1, observer.num_stop_requests());
}

TEST_F(AudioStreamHandlerTest, BadWavDataDoesNotInitialize) {
  // The class members and SetUp() will be ignored for this test. Create a
  // handler on the stack with some bad WAV data.
  AudioStreamHandler handler("RIFF1234WAVEjunkjunkjunkjunk");
  EXPECT_FALSE(handler.IsInitialized());
  EXPECT_FALSE(handler.Play());
  EXPECT_EQ(base::TimeDelta(), handler.duration());

  // Call Stop() to ensure that there is no crash.
  handler.Stop();
}

}  // namespace media
