// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation notes:
//
// - It is recommended to first acquire the native sample rate of the default
//   output device and then use the same rate when creating this object.
//   Use AudioManagerMac::HardwareSampleRate() to retrieve the sample rate.
// - Calling Close() also leads to self destruction.
// - The latency consists of two parts:
//   1) Hardware latency, which includes Audio Unit latency, audio device
//      latency;
//   2) The delay between the moment getting the callback and the scheduled time
//      stamp that tells when the data is going to be played out.
//
#ifndef MEDIA_AUDIO_MAC_AUDIO_AUHAL_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_AUHAL_MAC_H_

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioManagerMac;
class AudioPullFifo;

// Implementation of AudioOuputStream for Mac OS X using the
// AUHAL Audio Unit present in OS 10.4 and later.
// It is useful for low-latency output.
//
// Overview of operation:
// 1) An object of AUHALStream is created by the AudioManager factory on the
//    object's main thread via audio_man->MakeAudioStream().  Calls to the
//    control routines (Open/Close/Start/Stop), must be made on this thread.
// 2) Next Open() will be called. At that point the underlying AUHAL Audio Unit
//    is created and configured to use the |device|.
// 3) Then Start(source) is called and the device is started which creates its
//    own thread (or uses an existing background thread) on which the AUHAL's
//    callback will periodically ask for more data as buffers are being
//    consumed.
//    Note that all AUHAL instances receive callbacks on that very same
//    thread, so avoid any contention in the callback as to not cause delays for
//    other instances.
// 4) At some point Stop() will be called, which we handle by stopping the
//    output Audio Unit.
// 6) Lastly, Close() will be called where we cleanup and notify the audio
//    manager, which will delete the object.

// TODO(tommi): Since the callback audio thread is shared for all instances of
// AUHALStream, one stream blocking, can cause others to be delayed.  Several
// occurrances of this can cause a buildup of delay which forces the OS
// to skip rendering frames. One known cause of this is the synchronzation
// between the browser and render process in AudioSyncReader.
// We need to fix this.

class AUHALStream : public AudioOutputStream {
 public:
  // |manager| creates this object.
  // |device| is the CoreAudio device to use for the stream.
  // It will often be the default output device.
  AUHALStream(AudioManagerMac* manager,
              const AudioParameters& params,
              AudioDeviceID device);
  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioOutputStream::Close().
  ~AUHALStream() override;

  // Implementation of AudioOutputStream.
  bool Open() override;
  void Close() override;
  void Start(AudioSourceCallback* callback) override;
  void Stop() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;

  AudioDeviceID device_id() const { return device_; }
  size_t requested_buffer_size() const { return number_of_frames_; }

 private:
  // AUHAL callback.
  static OSStatus InputProc(void* user_data,
                            AudioUnitRenderActionFlags* flags,
                            const AudioTimeStamp* time_stamp,
                            UInt32 bus_number,
                            UInt32 number_of_frames,
                            AudioBufferList* io_data);

  OSStatus Render(AudioUnitRenderActionFlags* flags,
                  const AudioTimeStamp* output_time_stamp,
                  UInt32 bus_number,
                  UInt32 number_of_frames,
                  AudioBufferList* io_data);

  // Called by either |audio_fifo_| or Render() to provide audio data.
  void ProvideInput(int frame_delay, AudioBus* dest);

  // Sets the stream format on the AUHAL to PCM Float32 non-interleaved
  // for the given number of channels on the given scope and element.
  // The created stream description will be stored in |desc|.
  bool SetStreamFormat(AudioStreamBasicDescription* desc,
                       int channels,
                       UInt32 scope,
                       UInt32 element);

  // Creates the AUHAL, sets its stream format, buffer-size, etc.
  bool ConfigureAUHAL();

  // Uninitializes audio_unit_ if needed.
  void CloseAudioUnit();

  // Creates the input and output busses.
  void CreateIOBusses();

  // Gets the fixed playout device hardware latency and stores it. Returns 0
  // if not available.
  double GetHardwareLatency();

  // Gets the current playout latency value.
  double GetPlayoutLatency(const AudioTimeStamp* output_time_stamp);

  // Updates playout timestamp, current lost frames, and total lost frames and
  // glitches.
  void UpdatePlayoutTimestamp(const AudioTimeStamp* timestamp);

  // Called from the dtor and when the stream is reset.
  void ReportAndResetStats();

  // Our creator, the audio manager needs to be notified when we close.
  AudioManagerMac* const manager_;

  const AudioParameters params_;
  // For convenience - same as in params_.
  const int output_channels_;

  // Buffer-size.
  const size_t number_of_frames_;

  // Stores the number of frames that we actually get callbacks for.
  // This may be different from what we ask for, so we use this for stats in
  // order to understand how often this happens and what are the typical values.
  size_t number_of_frames_requested_;

  // Pointer to the object that will provide the audio samples.
  AudioSourceCallback* source_;

  // Protects |source_|.  Necessary since Render() calls seem to be in flight
  // when |audio_unit_| is supposedly stopped.  See http://crbug.com/178765.
  base::Lock source_lock_;

  // Holds the stream format details such as bitrate.
  AudioStreamBasicDescription output_format_;

  // The audio device to use with the AUHAL.
  // We can potentially handle both input and output with this device.
  const AudioDeviceID device_;

  // The AUHAL Audio Unit which talks to |device_|.
  AudioUnit audio_unit_;

  // Volume level from 0 to 1.
  float volume_;

  // Fixed playout hardware latency in frames.
  double hardware_latency_frames_;

  // This flag will be set to false while we're actively receiving callbacks.
  bool stopped_;

  // Container for retrieving data from AudioSourceCallback::OnMoreData().
  std::unique_ptr<AudioBus> output_bus_;

  // Dynamically allocated FIFO used when CoreAudio asks for unexpected frame
  // sizes.
  std::unique_ptr<AudioPullFifo> audio_fifo_;

  // Current buffer delay.  Set by Render().
  uint32_t current_hardware_pending_bytes_;

  // Lost frames not yet reported to the provider. Increased in
  // UpdatePlayoutTimestamp() if any lost frame since last time. Forwarded to
  // the provider and reset in ProvideInput().
  uint32_t current_lost_frames_;

  // Stores the timestamp of the previous audio buffer requested by the OS.
  // We use this in combination with |last_number_of_frames_| to detect when
  // the OS has decided to skip rendering frames (i.e. a glitch).
  // This can happen in case of high CPU load or excessive blocking on the
  // callback audio thread.
  // These variables are only touched on the callback thread and then read
  // in the dtor (when no longer receiving callbacks).
  // NOTE: Float64 and UInt32 types are used for native API compatibility.
  Float64 last_sample_time_;
  UInt32 last_number_of_frames_;
  UInt32 total_lost_frames_;
  UInt32 largest_glitch_frames_;
  int glitches_detected_;

  // Used to defer Start() to workaround http://crbug.com/160920.
  base::CancelableClosure deferred_start_cb_;

  // Used to make sure control functions (Start(), Stop() etc) are called on the
  // right thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(AUHALStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_AUHAL_MAC_H_
