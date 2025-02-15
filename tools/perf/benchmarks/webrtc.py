# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from measurements import webrtc
import page_sets
from telemetry import benchmark
from telemetry.timeline import tracing_category_filter
from telemetry.web_perf import timeline_based_measurement
from telemetry.web_perf.metrics import webrtc_rendering_timeline

RENDERING_VALUE_PREFIX = 'WebRTCRendering_'

# TODO(qyearsley, mcasas): Add webrtc.audio when http://crbug.com/468732
# is fixed, or revert https://codereview.chromium.org/1544573002/ when
# http://crbug.com/568333 is fixed.


# Disabled because the reference set becomes flaky with the new
# https:// page set introduced in http://crbug.com/523517.
# Try removing once the Chrome used for ref builds advances
# past blink commit pos 200986.
@benchmark.Disabled('reference')
class _Webrtc(perf_benchmark.PerfBenchmark):
  """Base class for WebRTC metrics for real-time communications tests."""
  test = webrtc.WebRTC


class WebrtcGetusermedia(_Webrtc):
  """Measures WebRtc GetUserMedia for video capture and local playback."""
  page_set = page_sets.WebrtcGetusermediaPageSet

  @classmethod
  def Name(cls):
    return 'webrtc.getusermedia'


class WebrtcPeerConnection(_Webrtc):
  """Measures WebRtc Peerconnection for remote video and audio communication."""
  page_set = page_sets.WebrtcPeerconnectionPageSet

  @classmethod
  def Name(cls):
    return 'webrtc.peerconnection'


class WebrtcDataChannel(_Webrtc):
  """Measures WebRtc DataChannel loopback."""
  page_set = page_sets.WebrtcDatachannelPageSet

  @classmethod
  def Name(cls):
    return 'webrtc.datachannel'


# WebrtcRendering must be a PerfBenchmark, and not a _Webrtc, because it is a
# timeline-based.
# Disabled on reference builds because they crash and don't support tab
# capture. See http://crbug.com/603232.
@benchmark.Disabled('reference')
class WebrtcRendering(perf_benchmark.PerfBenchmark):
  """Specific time measurements (e.g. fps, smoothness) for WebRtc rendering."""

  page_set = page_sets.WebrtcRenderingPageSet

  def CreateTimelineBasedMeasurementOptions(self):
    category_filter = tracing_category_filter.TracingCategoryFilter(
        filter_string='webrtc,webkit.console,blink.console')
    options = timeline_based_measurement.Options(category_filter)
    options.SetLegacyTimelineBasedMetrics(
        [webrtc_rendering_timeline.WebRtcRenderingTimelineMetric()])
    return options

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--use-fake-device-for-media-stream')
    options.AppendExtraBrowserArgs('--use-fake-ui-for-media-stream')

  @classmethod
  def Name(cls):
    return 'webrtc.webrtc_smoothness'
